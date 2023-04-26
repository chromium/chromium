// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/settings_api_bubble_helpers.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/extension_web_ui_override_registrar.h"
#include "chrome/browser/profiles/profile.h"
#include "components/crx_file/id_util.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"

namespace extensions {

namespace {

std::unique_ptr<KeyedService> BuildOverrideRegistrar(
    content::BrowserContext* context) {
  return std::make_unique<ExtensionWebUIOverrideRegistrar>(context);
}

scoped_refptr<const Extension> GetNtpExtension(const std::string& name) {
  return ExtensionBuilder()
      .SetManifest(base::Value::Dict()
                       .Set("name", name)
                       .Set("version", "1.0")
                       .Set("manifest_version", 2)
                       .Set("chrome_url_overrides",
                            base::Value::Dict().Set("newtab", "newtab.html")))
      .SetID(crx_file::id_util::GenerateId(name))
      .Build();
}

}  // namespace

using SettingsApiBubbleHelpersUnitTest = ExtensionServiceTestBase;

TEST_F(SettingsApiBubbleHelpersUnitTest, TestAcknowledgeExistingExtensions) {
  base::AutoReset<bool> ack_existing =
      SetAcknowledgeExistingNtpExtensionsForTesting(true);

  InitializeEmptyExtensionService();
  ExtensionWebUIOverrideRegistrar::GetFactoryInstance()->SetTestingFactory(
      profile(), base::BindRepeating(&BuildOverrideRegistrar));
  // We need to trigger the instantiation of the WebUIOverrideRegistrar for
  // it to be constructed, since by default it's not constructed in tests.
  ExtensionWebUIOverrideRegistrar::GetFactoryInstance()->Get(profile());

  // Create an extension overriding the NTP.
  scoped_refptr<const Extension> first = GetNtpExtension("first");
  service()->AddExtension(first.get());

  auto is_acknowledged = [this](const ExtensionId& id) {
    bool is_acked = false;
    return ExtensionPrefs::Get(profile())->ReadPrefAsBoolean(
               id, kNtpOverridingExtensionAcknowledged, &is_acked) &&
           is_acked;
  };
  // By default, the extension should not be acknowledged.
  EXPECT_FALSE(is_acknowledged(first->id()));

  // Acknowledge existing extensions. Now, `first` should be acknowledged.
  AcknowledgePreExistingNtpExtensions(profile());
  EXPECT_TRUE(is_acknowledged(first->id()));

  // Install a second NTP-overriding extension. The new extension should not be
  // acknowledged.
  scoped_refptr<const Extension> second = GetNtpExtension("second");
  service()->AddExtension(second.get());
  EXPECT_FALSE(is_acknowledged(second->id()));

  // Try acknowledging existing extensions. Since we already did this once for
  // this profile, this should have no effect, and we should still consider the
  // second extension unacknowledged.
  AcknowledgePreExistingNtpExtensions(profile());
  EXPECT_FALSE(is_acknowledged(second->id()));

  // But the first should still be acknowledged.
  EXPECT_TRUE(is_acknowledged(first->id()));
}

}  // namespace extensions
