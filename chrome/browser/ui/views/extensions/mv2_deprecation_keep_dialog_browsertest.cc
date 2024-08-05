// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback_helpers.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/extensions/extensions_dialogs.h"
#include "chrome/browser/ui/views/extensions/extensions_dialogs_browsertest.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/mojom/manifest.mojom.h"

class Mv2DeprecationKeepDialogBrowserTest : public ExtensionsDialogBrowserTest {
 public:
  Mv2DeprecationKeepDialogBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        extensions_features::kExtensionManifestV2Disabled);
  }
  ~Mv2DeprecationKeepDialogBrowserTest() override = default;
  Mv2DeprecationKeepDialogBrowserTest(
      const Mv2DeprecationKeepDialogBrowserTest&) = delete;
  Mv2DeprecationKeepDialogBrowserTest& operator=(
      const Mv2DeprecationKeepDialogBrowserTest&) = delete;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    // Add an extension that should be affected by the MV2 deprecation.
    scoped_refptr<const extensions::Extension> extension =
        extensions::ExtensionBuilder("MV2 Extension")
            .SetManifestVersion(2)
            .SetLocation(extensions::mojom::ManifestLocation::kInternal)
            .Build();
    extensions::ExtensionSystem::Get(browser()->profile())
        ->extension_service()
        ->AddExtension(extension.get());

    extensions::ShowMv2DeprecationKeepDialog(
        browser(), *extension,
        /*accept_callback=*/base::DoNothing(),
        /*cancel_callback=*/base::DoNothing());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(Mv2DeprecationKeepDialogBrowserTest, InvokeUi) {
  ShowAndVerifyUi();
}
