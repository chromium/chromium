// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback_helpers.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/extensions/extensions_dialogs.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/mojom/manifest.mojom.h"

class Mv2DeprecationKeepDialogInteractiveTest : public InteractiveBrowserTest {
 public:
  Mv2DeprecationKeepDialogInteractiveTest() {
    scoped_feature_list_.InitAndEnableFeature(
        extensions_features::kExtensionManifestV2Disabled);
  }
  ~Mv2DeprecationKeepDialogInteractiveTest() override = default;
  Mv2DeprecationKeepDialogInteractiveTest(
      const Mv2DeprecationKeepDialogInteractiveTest&) = delete;
  Mv2DeprecationKeepDialogInteractiveTest& operator=(
      const Mv2DeprecationKeepDialogInteractiveTest&) = delete;

  scoped_refptr<const extensions::Extension> InstallMV2Extension(
      const std::string extension_name) {
    scoped_refptr<const extensions::Extension> extension =
        extensions::ExtensionBuilder(extension_name)
            .SetManifestVersion(2)
            .SetLocation(extensions::mojom::ManifestLocation::kInternal)
            .Build();

    extension_registrar()->AddExtension(extension);
    return extension;
  }

  void DisableMV2Extension(const extensions::ExtensionId& extension_id) {
    extension_registrar()->DisableExtension(
        extension_id,
        {extensions::disable_reason::DISABLE_UNSUPPORTED_MANIFEST_VERSION});
  }

  extensions::ExtensionRegistrar* extension_registrar() {
    return extensions::ExtensionRegistrar::Get(browser()->profile());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(Mv2DeprecationKeepDialogInteractiveTest, ShowDialog) {
  auto extension = InstallMV2Extension("MV2 Extension");
  DisableMV2Extension(extension->id());

  RunTestSequence(
      Do([&]() {
        extensions::ShowMv2DeprecationKeepDialog(
            browser(), *extension,
            /*accept_callback=*/base::DoNothing(),
            /*cancel_callback=*/base::DoNothing());
      }),
      // We cannot add an element identifier to the dialog when it's built using
      // DialogModel::Builder. Thus, we check for its existence by checking the
      // visibility of one of its elements.
      WaitForShow(extensions::kMv2KeepDialogOkButtonElementId));
}

IN_PROC_BROWSER_TEST_F(Mv2DeprecationKeepDialogInteractiveTest,
                       ShowDialog_LongNameExtension) {
  const std::string long_name =
      "This extension name should be longer than our truncation threshold "
      "to test that the bubble can handle long names";
  auto extension = InstallMV2Extension(long_name);
  DisableMV2Extension(extension->id());

  RunTestSequence(
      Do([&]() {
        extensions::ShowMv2DeprecationKeepDialog(
            browser(), *extension,
            /*accept_callback=*/base::DoNothing(),
            /*cancel_callback=*/base::DoNothing());
      }),
      // We cannot add an element identifier to the dialog when it's built using
      // DialogModel::Builder. Thus, we check for its existence by checking the
      // visibility of one of its elements.
      WaitForShow(extensions::kMv2KeepDialogOkButtonElementId));
}
