// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/settings_overridden_params_providers.h"

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/extension_web_ui.h"
#include "chrome/browser/extensions/extension_web_ui_override_registrar.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/search_test_utils.h"
#include "components/search_engines/template_url_service.h"
#include "extensions/common/extension_builder.h"

class SettingsOverriddenParamsProvidersUnitTest
    : public extensions::ExtensionServiceTestBase {
 public:
  void SetUp() override {
    extensions::ExtensionServiceTestBase::SetUp();
    InitializeEmptyExtensionService();

    // The NtpOverriddenDialogController rellies on ExtensionWebUI; ensure one
    // exists.
    extensions::ExtensionWebUIOverrideRegistrar::GetFactoryInstance()
        ->SetTestingFactoryAndUse(
            profile(),
            base::BindRepeating([](content::BrowserContext* context)
                                    -> std::unique_ptr<KeyedService> {
              return std::make_unique<
                  extensions::ExtensionWebUIOverrideRegistrar>(context);
            }));
    auto* template_url_service = static_cast<TemplateURLService*>(
        TemplateURLServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile(),
            base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor)));
    search_test_utils::WaitForTemplateURLServiceToLoad(template_url_service);
  }

  // Adds a new extension that overrides the NTP.
  const extensions::Extension* AddExtensionControllingNewTab(
      const char* name = "ntp override") {
    base::Value::Dict chrome_url_overrides =
        base::Value::Dict().Set("newtab", "newtab.html");
    scoped_refptr<const extensions::Extension> extension =
        extensions::ExtensionBuilder(name)
            .SetLocation(extensions::mojom::ManifestLocation::kInternal)
            .SetManifestKey("chrome_url_overrides",
                            std::move(chrome_url_overrides))
            .Build();

    service()->AddExtension(extension.get());
    EXPECT_EQ(extension, ExtensionWebUI::GetExtensionControllingURL(
                             GURL(chrome::kChromeUINewTabURL), profile()));

    return extension.get();
  }
};

TEST_F(SettingsOverriddenParamsProvidersUnitTest,
       GetExtensionControllingNewTab) {
  // With no extensions installed, there should be no controlling extension.
  EXPECT_EQ(std::nullopt,
            settings_overridden_params::GetNtpOverriddenParams(profile()));

  // Install an extension, but not one that overrides the NTP. There should
  // still be no controlling extension.
  scoped_refptr<const extensions::Extension> regular_extension =
      extensions::ExtensionBuilder("regular").Build();
  service()->AddExtension(regular_extension.get());
  EXPECT_EQ(std::nullopt,
            settings_overridden_params::GetNtpOverriddenParams(profile()));

  // Finally, install an extension that overrides the NTP. It should be the
  // controlling extension.
  const extensions::Extension* ntp_extension = AddExtensionControllingNewTab();
  std::optional<ExtensionSettingsOverriddenDialog::Params> params =
      settings_overridden_params::GetNtpOverriddenParams(profile());
  ASSERT_TRUE(params);
  EXPECT_EQ(ntp_extension->id(), params->controlling_extension_id);

  // In this case, disabling the extension would go back to the default NTP, so
  // a specific message should show.
  EXPECT_EQ("Change back to Google?", base::UTF16ToUTF8(params->dialog_title));
}

TEST_F(SettingsOverriddenParamsProvidersUnitTest,
       DialogStringsWhenMultipleNtpOverrides_MultipleExtensions) {
  const extensions::Extension* extension1 =
      AddExtensionControllingNewTab("uno");
  const extensions::Extension* extension2 =
      AddExtensionControllingNewTab("dos");
  EXPECT_NE(extension1->id(), extension2->id());

  // When there are multiple extensions that could override the NTP, we should
  // show a generic dialog (rather than prompting to go back to the default
  // NTP), because the other extension would just take over.
  std::optional<ExtensionSettingsOverriddenDialog::Params> params =
      settings_overridden_params::GetNtpOverriddenParams(profile());
  ASSERT_TRUE(params);
  EXPECT_EQ(extension2->id(), params->controlling_extension_id);
  EXPECT_EQ("Did you mean to change this page?",
            base::UTF16ToUTF8(params->dialog_title));
}
