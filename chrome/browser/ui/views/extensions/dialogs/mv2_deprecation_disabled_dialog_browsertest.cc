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
    extensions::Mv2DisabledDialogController::ExtensionInfo extension_info_A;
    extension_info_A.id = "extA";
    extension_info_A.name = "Extension A";
    extension_info_A.icon = gfx::Image();

    extensions::Mv2DisabledDialogController::ExtensionInfo extension_info_B;
    extension_info_B.id = "extB";
    extension_info_B.name = "Extension B";
    extension_info_B.icon = gfx::Image();

    std::vector<extensions::Mv2DisabledDialogController::ExtensionInfo>
        extensions_info;
    extensions_info.push_back(extension_info_A);
    extensions_info.push_back(extension_info_B);

    extensions::ShowMv2DeprecationDisabledDialog(
        browser(), extensions_info,
        /*remove_callback=*/base::DoNothing(),
        /*manage_callback=*/base::DoNothing(),
        /*close_callback=*/base::DoNothing());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(Mv2DeprecationDisabledDialogBrowserTest, InvokeUi) {
  ShowAndVerifyUi();
}
