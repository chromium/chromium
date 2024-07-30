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
#include "chrome/browser/ui/extensions/mv2_disabled_dialog_controller.h"
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
        AddMV2ExtensionAndDisable("Extension A");
    extensions::Mv2DisabledDialogController::ExtensionInfo extension_info_A;
    extension_info_A.id = extension_A->id();
    extension_info_A.name = extension_A->name();
    extension_info_A.icon = gfx::Image();

    scoped_refptr<const extensions::Extension> extension_B =
        AddMV2ExtensionAndDisable("Extension B");
    extensions::Mv2DisabledDialogController::ExtensionInfo extension_info_B;
    extension_info_B.id = extension_B->id();
    extension_info_B.name = extension_B->name();
    extension_info_B.icon = gfx::Image();

    std::vector<extensions::Mv2DisabledDialogController::ExtensionInfo>
        extensions_info;
    extensions_info.push_back(extension_info_A);
    extensions_info.push_back(extension_info_B);

    extensions::ShowMv2DeprecationDisabledDialog(
        browser()->profile(), browser()->window()->GetNativeWindow(),
        extensions_info,
        /*remove_callback=*/base::DoNothing(),
        /*manage_callback=*/base::DoNothing(),
        /*close_callback=*/base::DoNothing());
  }

  scoped_refptr<const extensions::Extension> AddMV2ExtensionAndDisable(
      const std::string extension_name) {
    scoped_refptr<const extensions::Extension> extension =
        extensions::ExtensionBuilder(extension_name)
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
