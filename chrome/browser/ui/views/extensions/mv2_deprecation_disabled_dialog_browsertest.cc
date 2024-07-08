// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback_helpers.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/extensions_dialogs.h"
#include "chrome/browser/ui/views/extensions/extensions_dialogs_browsertest.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/mojom/manifest.mojom.h"

class Mv2DeprecationDisabledDialogBrowserTest
    : public ExtensionsDialogBrowserTest {
 public:
  Mv2DeprecationDisabledDialogBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        extensions_features::kExtensionManifestV2Disabled);
  }
  ~Mv2DeprecationDisabledDialogBrowserTest() override = default;
  Mv2DeprecationDisabledDialogBrowserTest(
      const Mv2DeprecationDisabledDialogBrowserTest&) = delete;
  Mv2DeprecationDisabledDialogBrowserTest& operator=(
      const Mv2DeprecationDisabledDialogBrowserTest&) = delete;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    scoped_refptr<const extensions::Extension> extension_A =
        AddMV2ExtensionAndDisable(u"Extension A");
    scoped_refptr<const extensions::Extension> extension_B =
        AddMV2ExtensionAndDisable(u"Extension A");
    extensions::ShowMv2DeprecationDisabledDialog(
        browser()->profile(), browser()->window()->GetNativeWindow(),
        {extension_A->id(), extension_B->id()},
        /*accept_callback=*/base::DoNothing(),
        /*cancel_callback=*/base::DoNothing());
  }

  scoped_refptr<const extensions::Extension> AddMV2ExtensionAndDisable(
      const std::u16string extension_name) {
    scoped_refptr<const extensions::Extension> extension =
        extensions::ExtensionBuilder("MV2 Extension")
            .SetManifestVersion(2)
            .SetLocation(extensions::mojom::ManifestLocation::kInternal)
            .Build();
    auto* extension_service =
        extensions::ExtensionSystem::Get(browser()->profile())
            ->extension_service();

    extension_service->AddExtension(extension.get());
    extension_service->DisableExtension(
        extension->id(),
        extensions::disable_reason::DISABLE_UNSUPPORTED_MANIFEST_VERSION);
    return extension;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(Mv2DeprecationDisabledDialogBrowserTest, InvokeUi) {
  ShowAndVerifyUi();
}
