// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extension_post_install_dialog_model.h"

#include "base/command_line.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_with_install.h"
#include "chrome/common/extensions/api/omnibox.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/common/api/extension_action/action_info.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/accelerator.h"

using extensions::Extension;

class ExtensionPostInstallDialogModelTest
    : public extensions::ExtensionServiceTestWithInstall {
 public:
  ExtensionPostInstallDialogModelTest() = default;

  ~ExtensionPostInstallDialogModelTest() override = default;

  void SetUp() override {
    InitializeEmptyExtensionService();
    service()->Init();
  }

 protected:
  void AddOmniboxKeyword(extensions::ExtensionBuilder* builder,
                         const std::string& keyword) {
    using ManifestKeys = extensions::api::omnibox::ManifestKeys;
    base::Value::Dict info;
    info.Set(ManifestKeys::Omnibox::kKeyword, keyword);
    builder->SetManifestKey(ManifestKeys::kOmnibox, std::move(info));
  }

  void AddRegularAction(extensions::ExtensionBuilder* builder) {
    builder->SetManifestKey(extensions::manifest_keys::kAction,
                            base::Value::Dict());
  }

  void AddBrowserActionKeyBinding(extensions::ExtensionBuilder* builder,
                                  const std::string& key) {
    builder->SetManifestKey(
        extensions::manifest_keys::kCommands,
        base::Value::Dict().Set(
            extensions::manifest_values::kBrowserActionCommandEvent,
            base::Value::Dict()
                .Set("suggested_key", key)
                .Set("description", "Invoke the page action")));
  }
};

TEST_F(ExtensionPostInstallDialogModelTest, SyntheticPageActionExtension) {
  // An extension with no action info in the manifest at all gets a synthesized
  // page action.
  auto extension = extensions::ExtensionBuilder("Foo").Build();
  registrar()->AddExtension(extension);

  ExtensionPostInstallDialogModel model(profile(), extension.get(), SkBitmap());

  // It should anchor to the synthesized action...
  EXPECT_TRUE(model.anchor_to_action());
  EXPECT_FALSE(model.anchor_to_omnibox());

  // ... but there should not be how-to-use text, since it has no actual way to
  // activate it other than that synthesized action.
  EXPECT_FALSE(model.show_how_to_use());
  EXPECT_TRUE(model.show_how_to_manage());
  EXPECT_FALSE(model.show_key_binding());
}

TEST_F(ExtensionPostInstallDialogModelTest, OmniboxExtension) {
  // An extension with a regular action and an omnibox keyword...
  auto builder = extensions::ExtensionBuilder("Foo");
  AddOmniboxKeyword(&builder, "fookey");
  builder.AddFlags(extensions::Extension::WAS_INSTALLED_BY_DEFAULT);
  auto extension = builder.Build();
  registrar()->AddExtension(extension);

  ExtensionPostInstallDialogModel model(profile(), extension.get(), SkBitmap());

  // ... should be anchored to the omnibox, not to the action ...
  EXPECT_FALSE(model.anchor_to_action());
  EXPECT_TRUE(model.anchor_to_omnibox());

  // ... and should have how-to-use and how-to-manage text, but not show a key
  // binding, since it doesn't have one.
  EXPECT_TRUE(model.show_how_to_use());
  EXPECT_TRUE(model.show_how_to_manage());
  EXPECT_FALSE(model.show_key_binding());
}

TEST_F(ExtensionPostInstallDialogModelTest, PageActionExtension) {
  // An extension with a page action...
  auto extension = extensions::ExtensionBuilder("Foo")
                       .SetManifestVersion(2)
                       .SetAction(extensions::ActionInfo::Type::kPage)
                       .Build();
  registrar()->AddExtension(extension);

  ExtensionPostInstallDialogModel model(profile(), extension.get(), SkBitmap());

  // should anchor to that action
  EXPECT_TRUE(model.anchor_to_action());
  EXPECT_FALSE(model.anchor_to_omnibox());

  // and have how-to-use and how-to-manage but no key binding, since it doesn't
  // have one.
  EXPECT_TRUE(model.show_how_to_use());
  EXPECT_TRUE(model.show_how_to_manage());
  EXPECT_FALSE(model.show_key_binding());
}

// TODO(crbug.com/405148986): Modify this test once the appropriate how to use
// text is decided for extensions with actions.
TEST_F(ExtensionPostInstallDialogModelTest, ActionExtension) {
  auto extension = extensions::ExtensionBuilder("Foo")
                       .SetAction(extensions::ActionInfo::Type::kAction)
                       .Build();
  registrar()->AddExtension(extension);

  ExtensionPostInstallDialogModel model(profile(), extension.get(), SkBitmap());

  // should anchor to that action
  EXPECT_TRUE(model.anchor_to_action());
  EXPECT_FALSE(model.anchor_to_omnibox());

  // and have how-to-manage but no how-to-use and key binding.
  EXPECT_FALSE(model.show_how_to_use());
  EXPECT_TRUE(model.show_how_to_manage());
  EXPECT_FALSE(model.show_key_binding());
}

TEST_F(ExtensionPostInstallDialogModelTest, ExtensionWithKeyBinding) {
  // An extension with a browser action and a key binding...
  auto builder = extensions::ExtensionBuilder("Foo");
  builder.SetAction(extensions::ActionInfo::Type::kBrowser);
  builder.SetManifestVersion(2);
  AddBrowserActionKeyBinding(&builder, "Alt+Shift+E");
  auto extension = builder.Build();

  // Note that we have to OnExtensionInstalled() here rather than just adding it
  // - hotkeys are picked up at install time, not add time.
  registrar()->OnExtensionInstalled(extension.get(), syncer::StringOrdinal());

  ExtensionPostInstallDialogModel model(profile(), extension.get(), SkBitmap());

  // Should have a how-to-use that lists the key, but *not* a how-to-manage,
  // since it crowds the UI.
  EXPECT_TRUE(model.show_how_to_use());
  EXPECT_FALSE(model.show_how_to_manage());
  EXPECT_TRUE(model.show_key_binding());

  // Note that we can't just check for "Alt+Shift+E" in
  // model.GetHowToUseText(), since on Mac, modifier keys are represented by
  // special sigils rather than their textual names.
  ui::Accelerator accelerator(ui::VKEY_E, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN);
  EXPECT_NE(std::u16string::npos,
            model.GetHowToUseText().find(accelerator.GetShortcutText()));
}

TEST_F(ExtensionPostInstallDialogModelTest, OmniboxKeywordAndSyntheticAction) {
  auto builder = extensions::ExtensionBuilder("Foo");
  AddOmniboxKeyword(&builder, "fookey");
  auto extension = builder.Build();

  registrar()->AddExtension(extension);

  ExtensionPostInstallDialogModel model(profile(), extension.get(), SkBitmap());

  // This extension has a synthesized action and an omnibox keyword. It should
  // have how-to-use text, and be anchored to its (synthesized) page action.
  EXPECT_TRUE(model.show_how_to_use());
  EXPECT_TRUE(model.anchor_to_action());
}
