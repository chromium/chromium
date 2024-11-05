// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extension_installed_bubble_model.h"

#include "base/command_line.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_with_install.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/common/extensions/api/omnibox.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/base/features.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/common/api/extension_action/action_info.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/accelerator.h"

using extensions::Extension;

class ExtensionInstalledBubbleModelTest
    : public extensions::ExtensionServiceTestWithInstall {
 public:
  ExtensionInstalledBubbleModelTest() {
    scoped_feature_list_.InitAndDisableFeature(
        syncer::kSyncEnableExtensionsInTransportMode);
  }

  ~ExtensionInstalledBubbleModelTest() override = default;

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

  scoped_refptr<const Extension> LoadExtension(
      const std::string& extension_path,
      bool packed) {
    extensions::ChromeTestExtensionLoader extension_loader(profile());
    extension_loader.set_pack_extension(packed);
    return extension_loader.LoadExtension(
        data_dir().AppendASCII(extension_path));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ExtensionInstalledBubbleModelTest, SyntheticPageActionExtension) {
  // An extension with no action info in the manifest at all gets a synthesized
  // page action.
  auto extension = extensions::ExtensionBuilder("Foo").Build();
  service()->AddExtension(extension.get());

  ExtensionInstalledBubbleModel model(profile(), extension.get(), SkBitmap());

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
  service()->AddExtension(extension.get());

  ExtensionInstalledBubbleModel model(profile(), extension.get(), SkBitmap());

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
  service()->AddExtension(extension.get());

  ExtensionInstalledBubbleModel model(profile(), extension.get(), SkBitmap());

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
  service()->OnExtensionInstalled(extension.get(), syncer::StringOrdinal());

  ExtensionInstalledBubbleModel model(profile(), extension.get(), SkBitmap());

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

  service()->AddExtension(extension.get());

  ExtensionInstalledBubbleModel model(profile(), extension.get(), SkBitmap());

  // This extension has a synthesized action and an omnibox keyword. It should
  // have how-to-use text, and be anchored to its (synthesized) page action.
  EXPECT_TRUE(model.show_how_to_use());
  EXPECT_TRUE(model.anchor_to_action());
}

TEST_F(ExtensionInstalledBubbleModelTest, ShowSigninPromo) {
  // Returns whether the sign in promo is shown for the model based on the given
  // `extension`.
  auto should_show_signin_promo = [this](const Extension* extension) {
    ExtensionInstalledBubbleModel model(profile(), extension, SkBitmap());
    return model.show_sign_in_promo();
  };

  // Unpacked extensions cannot be synced so the sign in promo is not shown.
  auto unpacked_extension =
      LoadExtension("simple_with_popup", /*packed=*/false);
  ASSERT_TRUE(unpacked_extension);
  EXPECT_FALSE(should_show_signin_promo(unpacked_extension.get()));

  // Show a sign in promo for a syncable extension installed while the user is
  // not signed in.
  auto extension_before_sign_in =
      LoadExtension("simple_with_file", /*packed=*/true);
  ASSERT_TRUE(extension_before_sign_in);

#if BUILDFLAG(IS_CHROMEOS)
  // Note: User is always signed in for ChromeOS, so the sign in promo should
  // never be shown.
  EXPECT_FALSE(should_show_signin_promo(extension_before_sign_in.get()));
#else
  EXPECT_TRUE(should_show_signin_promo(extension_before_sign_in.get()));
#endif  // BUILDFLAG(IS_CHROMEOS)

  // Use a test identity environment to mimic signing in a user with sync
  // enabled.
  auto identity_test_env_profile_adaptor =
      std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile());
  identity_test_env_profile_adaptor->identity_test_env()
      ->MakePrimaryAccountAvailable("testy@mctestface.com",
                                    signin::ConsentLevel::kSync);

  // Don't show a sign in promo if the user is currently signed in and syncing.
  auto extension_after_sign_in =
      LoadExtension("simple_with_icon", /*packed=*/true);
  ASSERT_TRUE(extension_after_sign_in);
  EXPECT_FALSE(should_show_signin_promo(extension_after_sign_in.get()));
}

class ExtensionInstalledBubbleModelTransportModeTest
    : public ExtensionInstalledBubbleModelTest {
 public:
  ExtensionInstalledBubbleModelTransportModeTest() {
    scoped_feature_list_.InitAndEnableFeature(
        syncer::kSyncEnableExtensionsInTransportMode);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ExtensionInstalledBubbleModelTransportModeTest, ShowSigninPromo) {
  // Returns whether the sign in promo is shown for the model based on the given
  // `extension`.
  auto should_show_signin_promo = [this](const Extension* extension) {
    ExtensionInstalledBubbleModel model(profile(), extension, SkBitmap());
    return model.show_sign_in_promo();
  };

  // Show a sign in promo for a syncable extension installed while the user is
  // not signed in.
  auto extension_before_sign_in =
      LoadExtension("simple_with_file", /*packed=*/true);
  ASSERT_TRUE(extension_before_sign_in);

#if BUILDFLAG(IS_CHROMEOS)
  // Note: User is always signed in for ChromeOS, so the sign in promo should
  // never be shown.
  EXPECT_FALSE(should_show_signin_promo(extension_before_sign_in.get()));
#else
  EXPECT_TRUE(should_show_signin_promo(extension_before_sign_in.get()));
#endif  // BUILDFLAG(IS_CHROMEOS)

  // Use a test identity environment to mimic signing a user in with sync
  // disabled (transport mode).
  auto identity_test_env_profile_adaptor =
      std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile());
  identity_test_env_profile_adaptor->identity_test_env()
      ->MakePrimaryAccountAvailable("testy@mctestface.com",
                                    signin::ConsentLevel::kSignin);

  // Pretend the user has now explcitly signed in: this is needed to sync
  // extensions in transport mode.
  profile()->GetPrefs()->SetBoolean(prefs::kExplicitBrowserSignin, true);

  // Don't show a sign in promo if the user is currently syncing in transport
  // mode.
  auto extension_after_sign_in =
      LoadExtension("simple_with_icon", /*packed=*/true);
  ASSERT_TRUE(extension_after_sign_in);
  EXPECT_FALSE(should_show_signin_promo(extension_after_sign_in.get()));
}
