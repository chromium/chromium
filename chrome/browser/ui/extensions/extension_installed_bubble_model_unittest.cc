// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extension_installed_bubble_model.h"

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/load_error_reporter.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/common/extensions/api/omnibox.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/api/extension_action/action_info.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/accelerator.h"

using extensions::Extension;

class ExtensionInstalledBubbleModelTest : public BrowserWithTestWindowTest {
 public:
  ExtensionInstalledBubbleModelTest() = default;
  ~ExtensionInstalledBubbleModelTest() override = default;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    extensions::LoadErrorReporter::Init(false);
    extensions::TestExtensionSystem* extension_system =
        static_cast<extensions::TestExtensionSystem*>(
            extensions::ExtensionSystem::Get(profile()));
    extension_system->CreateExtensionService(
        base::CommandLine::ForCurrentProcess(), base::FilePath(), false);
    extension_service_ =
        extensions::ExtensionSystem::Get(profile())->extension_service();
  }

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

  extensions::ExtensionService* extension_service() {
    return extension_service_;
  }

  const SkBitmap empty_icon_;

 private:
  raw_ptr<extensions::ExtensionService, DanglingUntriaged> extension_service_ =
      nullptr;
};

TEST_F(ExtensionInstalledBubbleModelTest, SyntheticPageActionExtension) {
  // An extension with no action info in the manifest at all gets a synthesized
  // page action.
  auto extension = extensions::ExtensionBuilder("Foo").Build();
  extension_service()->AddExtension(extension.get());

  ExtensionInstalledBubbleModel model(browser()->profile(), extension.get(),
                                      empty_icon_);

  // It should anchor to the synthesized action...
  EXPECT_TRUE(model.anchor_to_action());
  EXPECT_FALSE(model.anchor_to_omnibox());

  // ... but there should not be how-to-use text, since it has no actual way to
  // activate it other than that synthesized action.
  EXPECT_FALSE(model.show_how_to_use());
  EXPECT_TRUE(model.show_how_to_manage());
  EXPECT_FALSE(model.show_key_binding());
}

TEST_F(ExtensionInstalledBubbleModelTest, OmniboxExtension) {
  // An extension with a regular action and an omnibox keyword...
  auto builder = extensions::ExtensionBuilder("Foo");
  AddOmniboxKeyword(&builder, "fookey");
  builder.AddFlags(extensions::Extension::WAS_INSTALLED_BY_DEFAULT);
  auto extension = builder.Build();
  extension_service()->AddExtension(extension.get());

  ExtensionInstalledBubbleModel model(browser()->profile(), extension.get(),
                                      empty_icon_);

  // ... should be anchored to the omnibox, not to the action ...
  EXPECT_FALSE(model.anchor_to_action());
  EXPECT_TRUE(model.anchor_to_omnibox());

  // ... and should have how-to-use and how-to-manage text, but not show a key
  // binding, since it doesn't have one.
  EXPECT_TRUE(model.show_how_to_use());
  EXPECT_TRUE(model.show_how_to_manage());
  EXPECT_FALSE(model.show_key_binding());
}

TEST_F(ExtensionInstalledBubbleModelTest, PageActionExtension) {
  // An extension with a page action...
  auto extension = extensions::ExtensionBuilder("Foo")
                       .SetManifestVersion(2)
                       .SetAction(extensions::ActionInfo::Type::kPage)
                       .Build();
  extension_service()->AddExtension(extension.get());

  ExtensionInstalledBubbleModel model(browser()->profile(), extension.get(),
                                      empty_icon_);

  // should anchor to that action
  EXPECT_TRUE(model.anchor_to_action());
  EXPECT_FALSE(model.anchor_to_omnibox());

  // and have how-to-use and how-to-manage but no key binding, since it doesn't
  // have one.
  EXPECT_TRUE(model.show_how_to_use());
  EXPECT_TRUE(model.show_how_to_manage());
  EXPECT_FALSE(model.show_key_binding());
}

TEST_F(ExtensionInstalledBubbleModelTest, ExtensionWithKeyBinding) {
  // An extension with a browser action and a key binding...
  auto builder = extensions::ExtensionBuilder("Foo");
  builder.SetAction(extensions::ActionInfo::Type::kBrowser);
  builder.SetManifestVersion(2);
  AddBrowserActionKeyBinding(&builder, "Alt+Shift+E");
  auto extension = builder.Build();

  // Note that we have to OnExtensionInstalled() here rather than just adding it
  // - hotkeys are picked up at install time, not add time.
  extension_service()->OnExtensionInstalled(extension.get(),
                                            syncer::StringOrdinal());

  ExtensionInstalledBubbleModel model(browser()->profile(), extension.get(),
                                      empty_icon_);

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

TEST_F(ExtensionInstalledBubbleModelTest, OmniboxKeywordAndSyntheticAction) {
  auto builder = extensions::ExtensionBuilder("Foo");
  AddOmniboxKeyword(&builder, "fookey");
  auto extension = builder.Build();

  extension_service()->AddExtension(extension.get());

  ExtensionInstalledBubbleModel model(browser()->profile(), extension.get(),
                                      empty_icon_);

  // This extension has a synthesized action and an omnibox keyword. It should
  // have how-to-use text, and be anchored to its (synthesized) page action.
  EXPECT_TRUE(model.show_how_to_use());
  EXPECT_TRUE(model.anchor_to_action());
}

// TODO(ellyjones): Add a test for a syncable extension with a sync-eligible
// profile, to test model.show_sign_in_promo(). Reference
// ExtensionServiceSyncTest for an example.
