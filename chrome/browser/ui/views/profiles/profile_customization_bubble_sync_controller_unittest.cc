// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_customization_bubble_sync_controller.h"

#include "base/memory/raw_ptr.h"
#include "base/test/mock_callback.h"
#include "base/time/time.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/search/background/ntp_custom_background_service.h"
#include "chrome/browser/search/background/ntp_custom_background_service_factory.h"
#include "chrome/browser/themes/test/theme_service_changed_waiter.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/themes/theme_service_test_utils.h"
#include "chrome/browser/themes/theme_syncable_service.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/common/extensions/extension_test_util.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/test/test_sync_service.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/permissions/permission_set.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"

namespace {

#if BUILDFLAG(IS_WIN)
const base::FilePath::CharType kExtensionFilePath[] =
    FILE_PATH_LITERAL("c:\\foo");
#elif BUILDFLAG(IS_POSIX)
const base::FilePath::CharType kExtensionFilePath[] = FILE_PATH_LITERAL("/oo");
#else
#error "Unknown platform"
#endif

using theme_service::test::MakeThemeExtension;

const char* kCustomThemeId = "abcdefghijklmnopabcdefghijklmnop";
const char kCustomThemeName[] = "name";
const char kCustomThemeUrl[] = "http://update.url/foo";

const char kBackgroundUrl[] = "https://www.foo.com";

constexpr SkColor kNewProfileColor = SK_ColorRED;
constexpr SkColor kSyncedProfileColor = SK_ColorBLUE;

class ProfileCustomizationBubbleSyncControllerTest
    : public extensions::ExtensionServiceTestBase {
 public:
  using Outcome = ProfileCustomizationBubbleSyncController::Outcome;
  ProfileCustomizationBubbleSyncControllerTest()
      : extensions::ExtensionServiceTestBase(
            std::make_unique<content::BrowserTaskEnvironment>(
                base::test::TaskEnvironment::TimeSource::MOCK_TIME)) {}

  void SetUp() override {
    // Setting a matching update URL is necessary to make the test theme
    // considered syncable.
    extension_test_util::SetGalleryUpdateURL(GURL(kCustomThemeUrl));

    // Trying to write the theme pak just produces error messages.
    ThemeService::DisableThemePackForTesting();

    extensions::ExtensionServiceTestBase::SetUp();
    InitializeExtensionService(ExtensionServiceInitParams());
    service()->Init();

    // Create and add custom theme extension so the ThemeSyncableService can
    // find it.
    theme_extension_ = MakeThemeExtension(
        base::FilePath(kExtensionFilePath), kCustomThemeId, kCustomThemeName,
        extensions::mojom::ManifestLocation::kInternal, kCustomThemeUrl);
    extensions::ExtensionPrefs::Get(profile())->AddGrantedPermissions(
        theme_extension_->id(), extensions::PermissionSet());
    registrar()->AddExtension(theme_extension_);
    ASSERT_EQ(1u, extensions::ExtensionRegistry::Get(profile())
                      ->enabled_extensions()
                      .size());

    Browser::CreateParams params(profile(), /*user_gesture=*/true);
    auto browser_window = std::make_unique<TestBrowserWindow>();
    params.window = browser_window.release();
    browser_ = Browser::DeprecatedCreateOwnedForTesting(params);

    theme_service_ = ThemeServiceFactory::GetForProfile(profile());
    ntp_custom_background_service_ =
        NtpCustomBackgroundServiceFactory::GetForProfile(profile());
  }

  void ApplyColorAndShowBubbleWhenNoValueSynced(
      ProfileCustomizationBubbleSyncController::ShowBubbleCallback
          show_bubble_callback) {
    browser_->GetFeatures()
        .profile_customization_bubble_sync_controller()
        ->ShowOnSyncFailedOrDefaultThemeForTesting(
            kNewProfileColor, std::move(show_bubble_callback),
            &test_sync_service_, theme_service_,
            ntp_custom_background_service_);
  }

  void SetSyncedProfileTheme() {
    {
      test::ThemeServiceChangedWaiter waiter(theme_service_);
      theme_service_->SetTheme(theme_extension_.get());
      waiter.WaitForThemeChanged();
    }
    ASSERT_TRUE(theme_service_->UsingExtensionTheme());
  }

  void CloseBrowser() { browser_.reset(); }

  void NotifyOnSyncStarted(bool waiting_for_extension_installation = false) {
    theme_service_->GetThemeSyncableService()->NotifyOnSyncStartedForTesting(
        waiting_for_extension_installation
            ? ThemeSyncableService::ThemeSyncState::
                  kWaitingForExtensionInstallation
            : ThemeSyncableService::ThemeSyncState::kApplied);
  }

 protected:
  std::unique_ptr<Browser> browser_;
  syncer::TestSyncService test_sync_service_;
  raw_ptr<ThemeService> theme_service_ = nullptr;
  raw_ptr<NtpCustomBackgroundService> ntp_custom_background_service_ = nullptr;
  scoped_refptr<extensions::Extension> theme_extension_;
};

TEST_F(ProfileCustomizationBubbleSyncControllerTest,
       ShouldShowWhenSyncGetsDefaultTheme) {
  base::MockCallback<base::OnceCallback<void(Outcome)>> show_bubble;
  EXPECT_CALL(show_bubble, Run(Outcome::kShowBubble));

  ApplyColorAndShowBubbleWhenNoValueSynced(show_bubble.Get());
  NotifyOnSyncStarted();
}

TEST_F(ProfileCustomizationBubbleSyncControllerTest,
       ShouldShowWhenSyncDisabled) {
  base::MockCallback<base::OnceCallback<void(Outcome)>> show_bubble;
  EXPECT_CALL(show_bubble, Run(Outcome::kShowBubble));

  test_sync_service_.SetAllowedByEnterprisePolicy(false);
  ApplyColorAndShowBubbleWhenNoValueSynced(show_bubble.Get());
}

TEST_F(ProfileCustomizationBubbleSyncControllerTest,
       ShouldNotShowWhenSyncGetsCustomAutogeneratedColor) {
  base::MockCallback<base::OnceCallback<void(Outcome)>> show_bubble;
  EXPECT_CALL(show_bubble, Run(Outcome::kSkipBubble));

  ApplyColorAndShowBubbleWhenNoValueSynced(show_bubble.Get());
  // Simulate account autogenerated color.
  theme_service_->BuildAutogeneratedThemeFromColor(kSyncedProfileColor);
  NotifyOnSyncStarted();
}

TEST_F(ProfileCustomizationBubbleSyncControllerTest,
       ShouldNotShowWhenSyncGetsCustomUserColor) {
  base::MockCallback<base::OnceCallback<void(Outcome)>> show_bubble;
  EXPECT_CALL(show_bubble, Run(Outcome::kSkipBubble));

  ApplyColorAndShowBubbleWhenNoValueSynced(show_bubble.Get());
  // Set account user color.
  theme_service_->SetUserColorAndBrowserColorVariant(
      kSyncedProfileColor, ui::mojom::BrowserColorVariant::kTonalSpot);
  NotifyOnSyncStarted();
}

TEST_F(ProfileCustomizationBubbleSyncControllerTest,
       ShouldNotShowWhenSyncGetsCustomNtpBackground) {
  base::MockCallback<base::OnceCallback<void(Outcome)>> show_bubble;
  EXPECT_CALL(show_bubble, Run(Outcome::kSkipBubble));

  ApplyColorAndShowBubbleWhenNoValueSynced(show_bubble.Get());
  // Set account ntp background.
  ntp_custom_background_service_->AddValidBackdropUrlForTesting(
      GURL(kBackgroundUrl));
  ntp_custom_background_service_->SetCustomBackgroundInfo(
      GURL(kBackgroundUrl), GURL(), "", "", GURL(), "");
  ASSERT_TRUE(
      ntp_custom_background_service_->GetCustomBackground().has_value());
  NotifyOnSyncStarted();
}

TEST_F(ProfileCustomizationBubbleSyncControllerTest,
       ShouldNotShowWhenSyncGetsGrayscaleTheme) {
  base::MockCallback<base::OnceCallback<void(Outcome)>> show_bubble;
  EXPECT_CALL(show_bubble, Run(Outcome::kSkipBubble));

  ApplyColorAndShowBubbleWhenNoValueSynced(show_bubble.Get());
  // Set account grayscale theme.
  theme_service_->SetIsGrayscale(true);
  NotifyOnSyncStarted();
}

// Regression test for crbug.com/1213109.
TEST_F(ProfileCustomizationBubbleSyncControllerTest,
       ShouldNotShowWhenSyncGetsCustomColorBeforeStarting) {
  base::MockCallback<base::OnceCallback<void(Outcome)>> show_bubble;
  EXPECT_CALL(show_bubble, Run(Outcome::kSkipBubble));

  // Set up theme sync before the bubble controller gets created.
  theme_service_->SetUserColorAndBrowserColorVariant(
      kSyncedProfileColor, ui::mojom::BrowserColorVariant::kTonalSpot);
  NotifyOnSyncStarted();

  ApplyColorAndShowBubbleWhenNoValueSynced(show_bubble.Get());
}

TEST_F(ProfileCustomizationBubbleSyncControllerTest,
       ShouldNotShowWhenSyncGetsCustomTheme) {
  base::MockCallback<base::OnceCallback<void(Outcome)>> show_bubble;
  EXPECT_CALL(show_bubble, Run(Outcome::kSkipBubble));

  ApplyColorAndShowBubbleWhenNoValueSynced(show_bubble.Get());
  SetSyncedProfileTheme();
  NotifyOnSyncStarted();
}

TEST_F(ProfileCustomizationBubbleSyncControllerTest,
       ShouldNotShowWhenSyncGetsCustomThemeToInstall) {
  base::MockCallback<base::OnceCallback<void(Outcome)>> show_bubble;
  EXPECT_CALL(show_bubble, Run(Outcome::kSkipBubble));

  ApplyColorAndShowBubbleWhenNoValueSynced(show_bubble.Get());
  NotifyOnSyncStarted(/*waiting_for_extension_installation=*/true);
  SetSyncedProfileTheme();
}

TEST_F(ProfileCustomizationBubbleSyncControllerTest,
       ShouldNotShowWhenSyncHasCustomPasshrase) {
  base::MockCallback<base::OnceCallback<void(Outcome)>> show_bubble;
  EXPECT_CALL(show_bubble, Run(Outcome::kSkipBubble));

  test_sync_service_.SetPassphraseRequired();
  ApplyColorAndShowBubbleWhenNoValueSynced(show_bubble.Get());
  test_sync_service_.FireStateChanged();
}

TEST_F(ProfileCustomizationBubbleSyncControllerTest, ShouldNotShowOnTimeout) {
  base::MockCallback<base::OnceCallback<void(Outcome)>> show_bubble;
  EXPECT_CALL(show_bubble, Run(Outcome::kSkipBubble));

  ApplyColorAndShowBubbleWhenNoValueSynced(show_bubble.Get());
  task_environment()->FastForwardBy(base::Seconds(4));
}

TEST_F(ProfileCustomizationBubbleSyncControllerTest,
       ShouldNotShowWhenProfileGetsDeleted) {
  base::MockCallback<base::OnceCallback<void(Outcome)>> show_bubble;
  EXPECT_CALL(show_bubble, Run(Outcome::kAbort));

  ApplyColorAndShowBubbleWhenNoValueSynced(show_bubble.Get());
  CloseBrowser();
}

TEST_F(ProfileCustomizationBubbleSyncControllerTest, ShouldAbortIfCalledAgain) {
  base::MockCallback<base::OnceCallback<void(Outcome)>> old_show_bubble;
  EXPECT_CALL(old_show_bubble, Run(Outcome::kAbort));
  base::MockCallback<base::OnceCallback<void(Outcome)>> new_show_bubble;
  EXPECT_CALL(new_show_bubble, Run(Outcome::kShowBubble));

  ApplyColorAndShowBubbleWhenNoValueSynced(old_show_bubble.Get());
  ApplyColorAndShowBubbleWhenNoValueSynced(new_show_bubble.Get());

  NotifyOnSyncStarted();
}

}  // namespace
