// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"

#include <optional>
#include <string>
#include <string_view>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/with_feature_override.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_ui_delegate.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/profiles/profile_colors_util.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/profiles/profile_menu_coordinator.h"
#include "chrome/browser/ui/views/profiles/profile_menu_view_base.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/profile_destruction_waiter.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/user_education/interactive_feature_promo_test.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/signin/public/identity_manager/signin_constants.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/test/test_sync_service.h"
#include "components/user_education/common/user_education_features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "google_apis/gaia/core_account_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/mojom/themes.mojom.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_switches.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/testing_profile.h"
#include "components/user_manager/user_names.h"
#endif

using signin::constants::kNoHostedDomainFound;

namespace {
using ::testing::StrictMock;
using ::testing::ValuesIn;
using ::testing::WithParamInterface;

ui::mojom::BrowserColorVariant kColorVariant =
    ui::mojom::BrowserColorVariant::kTonalSpot;

const gfx::Image kSignedInImage = gfx::test::CreateImage(20, 20, SK_ColorBLUE);
const char kSignedInImageUrl[] = "SIGNED_IN_IMAGE_URL";

constexpr std::string_view kTestPassphrase = "testpassphrase";

enum class ColorThemeType { kAutogeneratedTheme, kUserColor };

std::unique_ptr<KeyedService> TestingSyncFactoryFunction(
    content::BrowserContext* context) {
  return std::make_unique<syncer::TestSyncService>();
}

class ProfileLoader {
 public:
  Profile* LoadFirstAndOnlyProfile() {
    auto* profile_manager = g_browser_process->profile_manager();
    auto& storage = profile_manager->GetProfileAttributesStorage();
    EXPECT_EQ(1U, storage.GetNumberOfProfiles());

    profile_manager->LoadProfileByPath(
        storage.GetAllProfilesAttributes()[0]->GetPath(), /*incognito=*/false,
        base::BindRepeating(&ProfileLoader::OnProfileLoaded,
                            base::Unretained(this)));

    profile_loading_run_loop_.Run();
    return profile_;
  }

 private:
  void OnProfileLoaded(Profile* profile) {
    profile_ = profile;
    profile_loading_run_loop_.Quit();
  }

  raw_ptr<Profile> profile_ = nullptr;
  base::RunLoop profile_loading_run_loop_;
};

class MockSigninUiDelegate : public signin_ui_util::SigninUiDelegate {
 public:
  MOCK_METHOD(void,
              ShowTurnSyncOnUI,
              (Profile*,
               signin_metrics::AccessPoint,
               signin_metrics::PromoAction,
               const CoreAccountId&,
               TurnSyncOnHelper::SigninAbortedMode,
               bool,
               bool),
              (override));
  MOCK_METHOD(void,
              ShowSigninUI,
              (Profile*,
               bool,
               signin_metrics::AccessPoint,
               signin_metrics::PromoAction),
              (override));
  MOCK_METHOD(void,
              ShowReauthUI,
              (Profile*,
               const std::string&,
               bool,
               signin_metrics::AccessPoint,
               signin_metrics::PromoAction),
              (override));
};

}  // namespace

class AvatarToolbarButtonBaseBrowserTest {
 public:
  AvatarToolbarButtonBaseBrowserTest()
      : dependency_manager_subscription_(
            BrowserContextDependencyManager::GetInstance()
                ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                    &AvatarToolbarButtonBaseBrowserTest::SetTestingFactories,
                    base::Unretained(this)))) {
    // By default make all delays infinite to avoid flakiness. The tests that
    // needs to test bypass the delay effects will have to enforce timing out
    // the delays using
    // `AvatarToolbarButton::TriggerTimeoutForTesting()`. This allows to
    // properly test the behavior pre/post delay without being time dependent.
    SetInfiniteAvatarDelay(AvatarDelayType::kNameGreeting);
    SetInfiniteAvatarDelay(AvatarDelayType::kSigninPendingText);
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
    SetInfiniteAvatarDelay(AvatarDelayType::kHistorySyncOptin);
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
  }

  AvatarToolbarButtonBaseBrowserTest(
      const AvatarToolbarButtonBaseBrowserTest&) = delete;
  AvatarToolbarButtonBaseBrowserTest& operator=(
      const AvatarToolbarButtonBaseBrowserTest&) = delete;
  ~AvatarToolbarButtonBaseBrowserTest() = default;

  AvatarToolbarButton* GetAvatarToolbarButton(Browser* browser) {
    return BrowserView::GetBrowserViewForBrowser(browser)->toolbar()->avatar_;
  }

  virtual Browser* GetBrowser() const = 0;

  // Allows overriding the delay of different events that have a timing
  // duration. Sets the delay to infinite in order to be able to test the
  // behavior while the delay is happening. In order to stop the delay, use
  // `AvatarToolbarButton::TriggerTimeoutForTesting()` at any point.
  void SetInfiniteAvatarDelay(AvatarDelayType delay_type) {
    delay_resets_.push_back(
        AvatarToolbarButton::CreateScopedInfiniteDelayOverrideForTesting(
            delay_type));
  }

  // Special override for the `AvatarDelayType::kSigninPendingText` delay to set
  // it to 0 given that the start time is stored as a ProfileUserData, which can
  // remain even if no browser exist. Setting it to 0 allows testing the
  // behavior where the delay is elapsed and then opening a new browser (while
  // no browser existed already).
  void SetZeroAvatarDelayForSigninPendingText() {
    delay_resets_.push_back(
        AvatarToolbarButton::
            CreateScopedZeroDelayOverrideSigninPendingTextForTesting());
  }

  // Returns the window count in avatar button text, if it exists.
  std::optional<int> GetWindowCountInAvatarButtonText(
      AvatarToolbarButton* avatar_button) {
    const std::u16string_view button_text = avatar_button->GetText();

    size_t before_number = button_text.find('(');
    if (before_number == std::u16string_view::npos) {
      return std::optional<int>();
    }

    size_t after_number = button_text.find(')');
    EXPECT_NE(std::u16string_view::npos, after_number);

    const std::u16string_view number_text =
        button_text.substr(before_number + 1, after_number - before_number - 1);
    int window_count;
    return base::StringToInt(number_text, &window_count)
               ? std::optional<int>(window_count)
               : std::optional<int>();
  }

  ProfileAttributesEntry* GetProfileAttributesEntry(Profile* profile) {
    ProfileAttributesEntry* entry =
        g_browser_process->profile_manager()
            ->GetProfileAttributesStorage()
            .GetProfileAttributesWithPath(profile->GetPath());
    CHECK(entry);
    return entry;
  }

  // - Helper functions

  signin::IdentityManager* GetIdentityManager() {
    return IdentityManagerFactory::GetForProfile(GetBrowser()->profile());
  }

  // Make account primary account with `consent_level` set and sets the account
  // name to `name`.
  AccountInfo MakePrimaryAccountAvailableWithName(
      signin::ConsentLevel consent_level,
      const std::u16string& email,
      const std::u16string& name) {
    AccountInfo account_info = signin::MakePrimaryAccountAvailable(
        GetIdentityManager(), base::UTF16ToUTF8(email), consent_level);
    EXPECT_FALSE(account_info.IsEmpty());

    account_info.given_name = base::UTF16ToUTF8(name);
    account_info.full_name = base::UTF16ToUTF8(name);
    account_info.picture_url = "SOME_FAKE_URL";
    account_info.hosted_domain = kNoHostedDomainFound;
    account_info.locale = "en";

    // Make sure account is valid so that all changes are persisted properly.
    CHECK(account_info.IsValid());

    signin::UpdateAccountInfoForAccount(GetIdentityManager(), account_info);

    GetTestSyncService()->SetSignedIn(consent_level, account_info);

    return account_info;
  }

  // Signs in to Chrome with `email` and set the `name` to the account name.
  AccountInfo Signin(const std::u16string& email, const std::u16string& name) {
    return MakePrimaryAccountAvailableWithName(signin::ConsentLevel::kSignin,
                                               email, name);
  }

  // Make sure `image_url` is different for each new image in order for the
  // changes to reflect into the profile as well.
  void AddAccountImage(CoreAccountId account_id,
                       gfx::Image image,
                       const std::string& image_url) {
    signin::SimulateAccountImageFetch(GetIdentityManager(), account_id,
                                      image_url, image);
  }

  // Sets `kSignedInImage` by default as the account image. This will allow to
  // show the name greeting.
  void AddSignedInImage(CoreAccountId account_id) {
    AddAccountImage(account_id, kSignedInImage, kSignedInImageUrl);
  }

  // Checks that the current image on the avtar button is the added account
  // image. Uses `kSignedInImage` by default.
  bool IsSignedInImageUsed(gfx::Image account_image = kSignedInImage) {
    AvatarToolbarButton* avatar_button = GetAvatarToolbarButton(GetBrowser());
    gfx::Image current_avatar_icon = gfx::Image(
        avatar_button->GetImage(views::Button::ButtonState::STATE_NORMAL));
    gfx::Image adapted_signed_in_image = profiles::GetSizedAvatarIcon(
        account_image, avatar_button->GetIconSize(),
        avatar_button->GetIconSize(), profiles::SHAPE_CIRCLE);
    return gfx::test::AreImagesEqual(current_avatar_icon,
                                     adapted_signed_in_image);
  }

  // Sign in with an image should show the greeting name.
  AccountInfo SigninWithImage(const std::u16string& email,
                              const std::u16string& name = u"account_name") {
    AccountInfo account_info = Signin(email, name);
    AddSignedInImage(account_info.account_id);
    return account_info;
  }

  // Sign in with the full account information that triggers the name greeting
  // followed by the history sync opt-in promo (if enabled and not syncing), but
  // force timing both out right away to clear the animation (in all windows).
  AccountInfo SigninWithImageAndClearGreetingAndSyncPromo(
      AvatarToolbarButton* avatar,
      const std::u16string& email,
      const std::u16string& name = u"account_name") {
    AccountInfo account_info = SigninWithImage(email, name);
    avatar->TriggerTimeoutForTesting(AvatarDelayType::kNameGreeting);
    // Make sure the cross window animation replay is not triggered. This is
    // needed to clear the animation in all windows.
    delay_resets_.push_back(
        signin_ui_util::
            CreateZeroOverrideDelayForCrossWindowAnimationReplayForTesting());
    ClearHistorySyncOptinPromoIfEnabled(avatar);
    return account_info;
  }

  // Clears the history sync optin promo if it is enabled. This is a no-op if
  // the promo is disabled.
  void ClearHistorySyncOptinPromoIfEnabled(AvatarToolbarButton* avatar) {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
    if (base::FeatureList::IsEnabled(
            switches::kEnableHistorySyncOptinExpansionPill)) {
      avatar->TriggerTimeoutForTesting(AvatarDelayType::kHistorySyncOptin);
    }
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
  }

#if !BUILDFLAG(IS_CHROMEOS)
  void Signout() {
    ASSERT_TRUE(
        GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));

    GetIdentityManager()->GetPrimaryAccountMutator()->ClearPrimaryAccount(
        signin_metrics::ProfileSignout::kTest);

    ASSERT_FALSE(
        GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  }
#endif

  void SimulateSigninError(bool web_sign_out) {
    ASSERT_TRUE(
        GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));

    signin_metrics::SourceForRefreshTokenOperation token_operation_source =
        web_sign_out ? signin_metrics::SourceForRefreshTokenOperation::
                           kDiceResponseHandler_Signout
                     : signin_metrics::SourceForRefreshTokenOperation::kUnknown;

    signin::SetInvalidRefreshTokenForPrimaryAccount(GetIdentityManager(),
                                                    token_operation_source);
  }

  void ClearSigninError() {
    ASSERT_TRUE(
        GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));

    signin::SetRefreshTokenForPrimaryAccount(GetIdentityManager());
  }

  // Enables sync for account with `email` and set the `name` to the account
  // name.
  AccountInfo EnableSync(const std::u16string& email,
                         const std::u16string name) {
    return MakePrimaryAccountAvailableWithName(signin::ConsentLevel::kSync,
                                               email, name);
  }

  // Enables Sync with image should attempt to show the name greeting.
  AccountInfo EnableSyncWithImage(const std::u16string& email) {
    // Using a default name, this function is not expected to be used if we care
    // about the name.
    AccountInfo account_info = EnableSync(email, u"account_name");
    AddSignedInImage(account_info.account_id);
    return account_info;
  }

  // Enables sync with the full account information that triggers the name
  // greeting, but force timing it out right away to clear the animation (in all
  // windows).
  AccountInfo EnableSyncWithImageAndClearGreeting(AvatarToolbarButton* avatar,
                                                  const std::u16string& email) {
    AccountInfo account_info = EnableSyncWithImage(email);
    avatar->TriggerTimeoutForTesting(AvatarDelayType::kNameGreeting);
    // Make sure the cross window animation replay is not triggered. This is
    // needed to clear the animation in all windows.
    delay_resets_.push_back(
        signin_ui_util::
            CreateZeroOverrideDelayForCrossWindowAnimationReplayForTesting());
    return account_info;
  }

  void SimulateSyncPaused() {
    ASSERT_TRUE(
        GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSync));

    // Simulates Sync Paused.
    GetTestSyncService()->SetPersistentAuthError();
    GetTestSyncService()->FireStateChanged();
  }

  void ClearSyncPaused() {
    ASSERT_TRUE(
        GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSync));

    // Clear Sync Paused introduced in `SimulateSyncPaused()`.
    GetTestSyncService()->ClearAuthError();
    GetTestSyncService()->FireStateChanged();
  }

  void ExpectSyncPaused(AvatarToolbarButton* avatar_button) {
    EXPECT_EQ(avatar_button->GetText(), l10n_util::GetStringUTF16(
#if !BUILDFLAG(IS_CHROMEOS)
                                            IDS_AVATAR_BUTTON_SYNC_PAUSED
#else
                                            IDS_AVATAR_BUTTON_SYNC_ERROR
#endif
                                            ));
  }

  void SimulateSyncError() {
    ASSERT_TRUE(
        GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSync));

    // Triggers Sync Error.
    GetTestSyncService()->SetTrustedVaultKeyRequired(true);
    GetTestSyncService()->FireStateChanged();
  }

  void ClearSyncError() {
    ASSERT_TRUE(
        GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSync));

    // Clear Sync Error introduces in `SimulateSyncError()`.
    GetTestSyncService()->SetTrustedVaultKeyRequired(false);
    GetTestSyncService()->FireStateChanged();
  }

  // Waits for `time`.
  void WaitForTime(base::TimeDelta time) {
    base::RunLoop waiting_run_loop;
    base::OneShotTimer timer;
    timer.Start(FROM_HERE, time, waiting_run_loop.QuitClosure());
    waiting_run_loop.Run();
  }

  void SimulateDisableSyncByPolicyWithError() {
    GetTestSyncService()->SetAllowedByEnterprisePolicy(false);
    // Disabling sync by policy resets the sync setup.
    GetTestSyncService()->SetInitialSyncFeatureSetupComplete(false);
    GetTestSyncService()->FireStateChanged();
  }

  void SimulateTypeManagedByPolicy(syncer::UserSelectableType type) {
    GetTestSyncService()->GetUserSettings()->SetTypeIsManagedByPolicy(type,
                                                                      true);
    GetTestSyncService()->FireStateChanged();
  }

  void SimulateTypeManagedByCustodian(syncer::UserSelectableType type) {
    GetTestSyncService()->GetUserSettings()->SetTypeIsManagedByCustodian(type,
                                                                         true);
    GetTestSyncService()->FireStateChanged();
  }

  void SimulatePassphraseError() {
    GetTestSyncService()->GetUserSettings()->SetPassphraseRequired(
        std::string(kTestPassphrase));
    GetTestSyncService()->FireStateChanged();
  }

  void ClearPassphraseError() {
    GetTestSyncService()->GetUserSettings()->SetDecryptionPassphrase(
        std::string(kTestPassphrase));
    GetTestSyncService()->FireStateChanged();
  }

  void SimulateUpgradeClientError() {
    syncer::SyncStatus sync_status;
    sync_status.sync_protocol_error.action = syncer::UPGRADE_CLIENT;
    GetTestSyncService()->SetDetailedSyncStatus(true, sync_status);
    GetTestSyncService()->FireStateChanged();
    ASSERT_TRUE(GetTestSyncService()->RequiresClientUpgrade());
  }

  void ClearUpgradeClientError() {
    syncer::SyncStatus sync_status;
    GetTestSyncService()->SetDetailedSyncStatus(true, sync_status);
    GetTestSyncService()->FireStateChanged();
    ASSERT_FALSE(GetTestSyncService()->RequiresClientUpgrade());
  }

 private:
  void SetTestingFactories(content::BrowserContext* context) {
    SyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        context, base::BindRepeating(&TestingSyncFactoryFunction));
  }

  syncer::TestSyncService* GetTestSyncService() {
    return static_cast<syncer::TestSyncService*>(
        SyncServiceFactory::GetForProfile(GetBrowser()->profile()));
  }

  base::CallbackListSubscription dependency_manager_subscription_;
  std::vector<base::AutoReset<std::optional<base::TimeDelta>>> delay_resets_;
};

class AvatarToolbarButtonBrowserTest
    : public InProcessBrowserTest,
      public AvatarToolbarButtonBaseBrowserTest {
 protected:
  // AvatarToolbarButtonBaseBrowserTest:
  Browser* GetBrowser() const override { return browser(); }

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    if (GetIdentityManager()) {
      // Puts `IdentityManager` in a known good state to avoid flakiness.
      signin::WaitForRefreshTokensLoaded(GetIdentityManager());
    }
  }
};

IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonBrowserTest, IncognitoWindowCount) {
  Profile* profile = browser()->profile();
  Browser* browser1 = CreateIncognitoBrowser(profile);
  AvatarToolbarButton* avatar_button1 = GetAvatarToolbarButton(browser1);
  EXPECT_TRUE(avatar_button1->GetEnabled());
  EXPECT_TRUE(avatar_button1->GetVisible());
  EXPECT_FALSE(GetWindowCountInAvatarButtonText(avatar_button1).has_value());

  Browser* browser2 = CreateIncognitoBrowser(profile);
  AvatarToolbarButton* avatar_button2 = GetAvatarToolbarButton(browser2);
  EXPECT_EQ(2, *GetWindowCountInAvatarButtonText(avatar_button1));
  EXPECT_EQ(2, *GetWindowCountInAvatarButtonText(avatar_button2));

  CloseBrowserSynchronously(browser2);
  EXPECT_FALSE(GetWindowCountInAvatarButtonText(avatar_button1).has_value());
}

#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonBrowserTest, GuestWindowCount) {
  Browser* browser1 = CreateGuestBrowser();
  AvatarToolbarButton* avatar_button1 = GetAvatarToolbarButton(browser1);
  EXPECT_TRUE(avatar_button1->GetEnabled());
  EXPECT_TRUE(avatar_button1->GetVisible());
  EXPECT_FALSE(GetWindowCountInAvatarButtonText(avatar_button1).has_value());

  Browser* browser2 = CreateGuestBrowser();
  AvatarToolbarButton* avatar_button2 = GetAvatarToolbarButton(browser2);
  EXPECT_EQ(2, *GetWindowCountInAvatarButtonText(avatar_button1));
  EXPECT_EQ(2, *GetWindowCountInAvatarButtonText(avatar_button2));

  CloseBrowserSynchronously(browser2);
  EXPECT_FALSE(GetWindowCountInAvatarButtonText(avatar_button1).has_value());
}
#endif

#if BUILDFLAG(IS_CHROMEOS)
class AvatarToolbarButtonAshBrowserTest
    : public AvatarToolbarButtonBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Adding these command lines simulates Ash in Guest mode.
    command_line->AppendSwitch(ash::switches::kGuestSession);
    command_line->AppendSwitchASCII(ash::switches::kLoginUser,
                                    user_manager::kGuestUserName);
    command_line->AppendSwitchASCII(ash::switches::kLoginProfile,
                                    TestingProfile::kTestUserProfileDir);
    command_line->AppendSwitch(switches::kIncognito);
  }
};

IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonAshBrowserTest, GuestSession) {
  Profile* guest_profile = browser()->profile();
  ASSERT_TRUE(guest_profile->IsGuestSession());

  AvatarToolbarButton* avatar_button = GetAvatarToolbarButton(browser());
  EXPECT_TRUE(avatar_button->GetVisible());
  EXPECT_FALSE(avatar_button->GetEnabled());

  EXPECT_EQ(avatar_button->GetText(),
            l10n_util::GetPluralStringFUTF16(IDS_AVATAR_BUTTON_GUEST, 1));

  Browser* browser_2 = CreateBrowser(guest_profile);
  AvatarToolbarButton* avatar_button_2 = GetAvatarToolbarButton(browser_2);
  EXPECT_TRUE(avatar_button_2->GetVisible());
  EXPECT_FALSE(avatar_button_2->GetEnabled());

  // Browser count is not taken into consideration on purpose for Ash Guest
  // windows since the button is not enabled, both buttons still show the same
  // text as if it was a single window, which is different from other platforms.
  EXPECT_EQ(avatar_button->GetText(),
            l10n_util::GetPluralStringFUTF16(IDS_AVATAR_BUTTON_GUEST, 1));
  EXPECT_EQ(avatar_button_2->GetText(),
            l10n_util::GetPluralStringFUTF16(IDS_AVATAR_BUTTON_GUEST, 1));
}
#endif

IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonBrowserTest, DefaultBrowser) {
  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser());
  ASSERT_TRUE(avatar);
#if BUILDFLAG(IS_CHROMEOS)
  // No avatar button is shown in normal Ash windows.
  EXPECT_FALSE(avatar->GetVisible());
#else
  EXPECT_TRUE(avatar->GetVisible());
  EXPECT_TRUE(avatar->GetEnabled());
#endif
}

IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonBrowserTest, IncognitoBrowser) {
  Browser* browser1 = CreateIncognitoBrowser(browser()->profile());
  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser1);
  ASSERT_TRUE(avatar);
  // Incognito browsers always show an enabled avatar button.
  EXPECT_TRUE(avatar->GetVisible());
  EXPECT_TRUE(avatar->GetEnabled());
}

#if BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonBrowserTest, SigninBrowser) {
  // Create an Incognito browser first.
  CreateIncognitoBrowser(browser()->profile());
  // Create a portal signin browser which will not be the Incognito browser.
  Profile::OTRProfileID profile_id(
      Profile::OTRProfileID::CreateUniqueForCaptivePortal());
  Browser* browser1 = Browser::Create(Browser::CreateParams(
      browser()->profile()->GetOffTheRecordProfile(profile_id,
                                                   /*create_if_needed=*/true),
      true));
  AddBlankTabAndShow(browser1);
  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser1);
  ASSERT_TRUE(avatar);
  // On ChromeOS, captive portal signin windows show a
  // disabled avatar button to indicate that the window is incognito.
  EXPECT_TRUE(avatar->GetVisible());
  EXPECT_FALSE(avatar->GetEnabled());
}
#endif

// TODO(b/331746545): Check flaky test issue on windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_ShowNameOnSigninThenSync DISABLED_ShowNameOnSigninThenSync
#else
#define MAYBE_ShowNameOnSigninThenSync ShowNameOnSigninThenSync
#endif
IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonBrowserTest,
                       MAYBE_ShowNameOnSigninThenSync) {
  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser());
  // Normal state.
  ASSERT_TRUE(avatar->GetText().empty());

  std::u16string email(u"test@gmail.com");
  std::u16string name(u"TestName");
  AccountInfo account_info = Signin(email, name);
  // The button is in a waiting for image state, the name is not yet displayed.
  EXPECT_EQ(avatar->GetText(), std::u16string());

  // The greeting will only show when the image is loaded.
  AddSignedInImage(account_info.account_id);
  EXPECT_EQ(avatar->GetText(),
            l10n_util::GetStringFUTF16(IDS_AVATAR_BUTTON_GREETING, name));

  avatar->TriggerTimeoutForTesting(AvatarDelayType::kNameGreeting);
  ClearHistorySyncOptinPromoIfEnabled(avatar);
  // Once the name is not shown anymore, we expect no text.
  EXPECT_EQ(avatar->GetText(), std::u16string());

  // Enabling Sync after already being signed in does not show the name again.
  EnableSync(email, name);
  EXPECT_EQ(avatar->GetText(), std::u16string());
}

// TODO(b/331746545): Check flaky test issue on windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_ShowNameOnSync DISABLED_ShowNameOnSync
#else
#define MAYBE_ShowNameOnSync ShowNameOnSync
#endif
IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonBrowserTest, MAYBE_ShowNameOnSync) {
  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser());
  // Normal state.
  ASSERT_TRUE(avatar->GetText().empty());

  std::u16string email(u"test@gmail.com");
  std::u16string name(u"TestName");
  AccountInfo account_info = EnableSync(email, name);
  // The button is in a waiting for image state, the name is not yet displayed.
  EXPECT_EQ(avatar->GetText(), std::u16string());

  // The greeting will only show when the image is loaded.
  AddSignedInImage(account_info.account_id);
  EXPECT_EQ(avatar->GetText(),
            l10n_util::GetStringFUTF16(IDS_AVATAR_BUTTON_GREETING, name));

  avatar->TriggerTimeoutForTesting(AvatarDelayType::kNameGreeting);
  // Once the name is not shown anymore, we expect no text.
  EXPECT_EQ(avatar->GetText(), std::u16string());
}

// Check www.crbug.com/331499330: This test makes sure that no states attempt to
// request an update during their construction. But rather do so after all the
// states are created and the view is added to the Widget.
IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonBrowserTest,
                       OpenNewBrowserWhileNameIsShown) {
  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser());
  // Normal state.
  ASSERT_TRUE(avatar->GetText().empty());

  std::u16string name(u"TestName");
  AccountInfo account_info = Signin(u"test@gmail.com", name);

  // The button is in a waiting for image state, the name is not yet displayed.
  EXPECT_EQ(avatar->GetText(), std::u16string());

  // The greeting will only show when the image is loaded.
  AddSignedInImage(account_info.account_id);
  EXPECT_EQ(avatar->GetText(),
            l10n_util::GetStringFUTF16(IDS_AVATAR_BUTTON_GREETING, name));

  // Creating a new browser while the refresh tokens are already loaded and the
  // name showing should not break/crash.
  Browser* new_browser = CreateBrowser(browser()->profile());
  AvatarToolbarButton* new_avatar_button = GetAvatarToolbarButton(new_browser);
  // Name is expected to be shown while it is still shown on the first browser.
  ASSERT_EQ(avatar->GetText(),
            l10n_util::GetStringFUTF16(IDS_AVATAR_BUTTON_GREETING, name));
  EXPECT_EQ(new_avatar_button->GetText(),
            l10n_util::GetStringFUTF16(IDS_AVATAR_BUTTON_GREETING, name));
}

IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonBrowserTest, SyncPaused) {
  AvatarToolbarButton* avatar_button = GetAvatarToolbarButton(browser());
  // Normal state.
  ASSERT_TRUE(avatar_button->GetText().empty());

  AccountInfo account_info =
      EnableSyncWithImageAndClearGreeting(avatar_button, u"test@gmail.com");
  SimulateSyncPaused();
  ExpectSyncPaused(avatar_button);

  ClearSyncPaused();
  EXPECT_EQ(avatar_button->GetText(), std::u16string());
}

// Checks that "Sync paused" has higher priority than passphrase errors.
// Regression test for https://crbug.com/368997513
IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonBrowserTest,
                       SyncPausedWithPassphraseError) {
  AvatarToolbarButton* avatar_button = GetAvatarToolbarButton(browser());
  ASSERT_TRUE(avatar_button->GetText().empty());

  AccountInfo account_info =
      EnableSyncWithImageAndClearGreeting(avatar_button, u"test@gmail.com");
  SimulatePassphraseError();
  SimulateSyncPaused();
  ExpectSyncPaused(avatar_button);
}

IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonBrowserTest, SyncError) {
  AvatarToolbarButton* avatar_button = GetAvatarToolbarButton(browser());
  // Normal state.
  ASSERT_TRUE(avatar_button->GetText().empty());

  EnableSyncWithImageAndClearGreeting(avatar_button, u"test@gmail.com");
  SimulateSyncError();
  EXPECT_EQ(avatar_button->GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SYNC_ERROR));

  ClearSyncError();
  EXPECT_EQ(avatar_button->GetText(), std::u16string());
}

IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonBrowserTest,
                       SyncPausedThenExplicitText) {
  AvatarToolbarButton* avatar_button = GetAvatarToolbarButton(browser());
  // Normal state.
  ASSERT_TRUE(avatar_button->GetText().empty());

  EnableSyncWithImageAndClearGreeting(avatar_button, u"test@gmail.com");
  SimulateSyncPaused();
  ExpectSyncPaused(avatar_button);

  std::u16string profile_switch_text(u"Profile Switch?");
  base::ScopedClosureRunner hide_callback =
      avatar_button->SetExplicitButtonState(
          profile_switch_text, /*accessibility_label=*/std::nullopt,
          /*explicit_action=*/std::nullopt);
  EXPECT_EQ(avatar_button->GetText(), profile_switch_text);

  // Clearing explicit text should go back to Sync Pause.
  hide_callback.RunAndReset();
  ExpectSyncPaused(avatar_button);
}

// Explicit text over sync paused/error.
IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonBrowserTest,
                       ExplicitTextThenSyncPause) {
  AvatarToolbarButton* avatar_button = GetAvatarToolbarButton(browser());
  // Normal state.
  ASSERT_TRUE(avatar_button->GetText().empty());

  EnableSyncWithImageAndClearGreeting(avatar_button, u"test@gmail.com");
  std::u16string profile_switch_text(u"Profile Switch?");
  base::ScopedClosureRunner hide_callback =
      avatar_button->SetExplicitButtonState(
          profile_switch_text, /*accessibility_label=*/std::nullopt,
          /*explicit_action=*/std::nullopt);
  EXPECT_EQ(avatar_button->GetText(), profile_switch_text);

  SimulateSyncPaused();
  // Explicit text should still be shown even if Sync is now Paused.
  EXPECT_EQ(avatar_button->GetText(), profile_switch_text);

  // Clearing explicit text should go back to Sync Pause.
  hide_callback.RunAndReset();
  ExpectSyncPaused(avatar_button);
}

IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonBrowserTest,
                       ShowExplicitTextAndHide) {
  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser());
  ASSERT_EQ(avatar->GetText(), std::u16string());

  std::u16string new_text(u"Some New Text");
  base::ScopedClosureRunner hide_callback = avatar->SetExplicitButtonState(
      new_text, /*accessibility_label=*/std::nullopt,
      /*explicit_action=*/std::nullopt);

  EXPECT_EQ(avatar->GetText(), new_text);
  hide_callback.RunAndReset();
  EXPECT_EQ(avatar->GetText(), std::u16string());
}

IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonBrowserTest,
                       ShowExplicitTextAndDefaultHide) {
  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser());
  ASSERT_EQ(avatar->GetText(), std::u16string());

  // Simulates a stack that enforces the change of text, but never explicitly
  // call the hide callback. It should still be done on explicitly destroying
  // the caller.
  {
    std::u16string new_text(u"Some New Text");
    base::ScopedClosureRunner hide_callback = avatar->SetExplicitButtonState(
        new_text, /*accessibility_label=*/std::nullopt,
        /*explicit_action=*/std::nullopt);
    EXPECT_EQ(avatar->GetText(), new_text);
  }

  EXPECT_EQ(avatar->GetText(), std::u16string());
}

IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonBrowserTest,
                       ShowExplicitTextWithExplicitAction) {
  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser());
  ASSERT_EQ(avatar->GetText(), std::u16string());
  ASSERT_FALSE(avatar->HasExplicitButtonAction());

  const std::u16string text_1(u"Some New Text 1");
  base::MockCallback<base::RepeatingClosure> mock_callback_1;
  base::ScopedClosureRunner reset_callback_1 = avatar->SetExplicitButtonState(
      text_1, /*accessibility_label=*/std::nullopt, mock_callback_1.Get());
  EXPECT_EQ(avatar->GetText(), text_1);
  EXPECT_TRUE(avatar->HasExplicitButtonAction());
  EXPECT_CALL(mock_callback_1, Run).Times(1);
  avatar->ButtonPressed();

  const std::u16string text_2(u"Some New Text 2");
  base::MockCallback<base::RepeatingClosure> mock_callback_2;
  base::ScopedClosureRunner reset_callback_2 = avatar->SetExplicitButtonState(
      text_2, /*accessibility_label=*/std::nullopt, mock_callback_2.Get());
  EXPECT_EQ(avatar->GetText(), text_2);
  EXPECT_TRUE(avatar->HasExplicitButtonAction());
  EXPECT_CALL(mock_callback_2, Run).Times(1);
  avatar->ButtonPressed();

  // Calling the first reset callback should do nothing after the second call
  // to `SetExplicitButtonState`.
  reset_callback_1.RunAndReset();
  EXPECT_EQ(avatar->GetText(), text_2);
  EXPECT_TRUE(avatar->HasExplicitButtonAction());

  // Calling the second reset callback should reset the text and the action.
  reset_callback_2.RunAndReset();
  EXPECT_EQ(avatar->GetText(), std::u16string());
  EXPECT_FALSE(avatar->HasExplicitButtonAction());
}

// Avatar button is not shown on Ash. No need to perform those tests as the info
// checked might not be adapted.
#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonBrowserTest, SignInOutIconEffect) {
  ASSERT_FALSE(IsSignedInImageUsed());

  SigninWithImage(u"test@gmail.com");
  EXPECT_TRUE(IsSignedInImageUsed());

  Signout();
  EXPECT_FALSE(IsSignedInImageUsed());
}

IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonBrowserTest, SignedInChangeIcon) {
  ASSERT_FALSE(IsSignedInImageUsed());

  AccountInfo account_info = SigninWithImage(u"test@gmail.com");
  EXPECT_TRUE(IsSignedInImageUsed());

  // Same image but different color as `kSignedInImage`.
  gfx::Image updated_image = gfx::test::CreateImage(20, 20, SK_ColorGREEN);
  AddAccountImage(account_info.account_id, updated_image,
                  "UPDATED_IMAGE_FAKE_URL");

  EXPECT_TRUE(IsSignedInImageUsed(updated_image));
}

IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonBrowserTest,
                       PRE_SignedInWithNewSessionKeepIcon) {
  ASSERT_FALSE(IsSignedInImageUsed());

  SigninWithImage(u"test@gmail.com");
  EXPECT_TRUE(IsSignedInImageUsed());
}

IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonBrowserTest,
                       SignedInWithNewSessionKeepIcon) {
  ASSERT_TRUE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  // Previously added image on signin should still be shown in the new session.
  EXPECT_TRUE(IsSignedInImageUsed());
}

IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonBrowserTest, TooltipText) {
  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser());
  ASSERT_EQ(avatar->GetText(), std::u16string());

  const std::u16string account_name(u"Account name");
  AccountInfo account_info = Signin(u"test@gmail.com", account_name);

  AddSignedInImage(account_info.account_id);

  EXPECT_EQ(avatar->GetRenderedTooltipText(gfx::Point()), account_name);

  avatar->TriggerTimeoutForTesting(AvatarDelayType::kNameGreeting);

  // Tooltip is the same after hiding the name.
  EXPECT_EQ(avatar->GetRenderedTooltipText(gfx::Point()), account_name);
}

// TODO(b/331746545): Check flaky test issue on windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_EnableSyncWithSyncDisabled DISABLED_EnableSyncWithSyncDisabled
#else
#define MAYBE_EnableSyncWithSyncDisabled EnableSyncWithSyncDisabled
#endif
IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonBrowserTest,
                       MAYBE_EnableSyncWithSyncDisabled) {
  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser());
  ASSERT_EQ(avatar->GetText(), std::u16string());

  EnableSyncWithImageAndClearGreeting(avatar, u"test@gmail.com");
  EXPECT_EQ(avatar->GetText(), std::u16string());

  SimulateDisableSyncByPolicyWithError();

  EXPECT_EQ(avatar->GetText(), std::u16string());

  Browser* new_browser = CreateBrowser(browser()->profile());
  AvatarToolbarButton* new_avatar = GetAvatarToolbarButton(new_browser);
  EXPECT_EQ(new_avatar->GetText(), std::u16string());
}

#endif

class AvatarToolbarButtonWithInteractiveFeaturePromoBrowserTest
    : public InteractiveFeaturePromoTest,
      public AvatarToolbarButtonBaseBrowserTest {
 protected:
  AvatarToolbarButtonWithInteractiveFeaturePromoBrowserTest()
      : InteractiveFeaturePromoTest(UseDefaultTrackerAllowingPromos({})) {}

  // AvatarToolbarButtonBaseBrowserTest:
  Browser* GetBrowser() const override { return browser(); }

  // InteractiveFeaturePromoTest:
  void SetUpOnMainThread() override {
    InteractiveFeaturePromoTest::SetUpOnMainThread();
    if (GetIdentityManager()) {
      // Puts `IdentityManager` in a known good state to avoid flakiness.
      signin::WaitForRefreshTokensLoaded(GetIdentityManager());
    }
  }
};

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
class AvatarToolbarButtonHistorySyncOptinBrowserTest
    : public AvatarToolbarButtonWithInteractiveFeaturePromoBrowserTest {
 protected:
  explicit AvatarToolbarButtonHistorySyncOptinBrowserTest(
      base::FieldTrialParams feature_parameters = {}) {
    feature_list_.InitAndEnableFeatureWithParameters(
        switches::kEnableHistorySyncOptinExpansionPill, feature_parameters);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonHistorySyncOptinBrowserTest,
                       HistorySyncOptinNotShownIfGreetingNotShown) {
  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser());
  // Normal state.
  ASSERT_TRUE(avatar->GetText().empty());
  Signin(/*email=*/u"test@gmail.com", /*name=*/u"TestName");
  // The button is in a waiting for image state, the greeting is not yet
  // displayed, hence the history sync opt-in should not be shown.
  EXPECT_EQ(avatar->GetText(), std::u16string());
}

// TODO(crbug.com/331746545): Check the flaky test issue on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_HistorySyncOptinNotShownWhenSyncEnabled \
  DISABLED_HistorySyncOptinNotShownWhenSyncEnabled
#else
#define MAYBE_HistorySyncOptinNotShownWhenSyncEnabled \
  HistorySyncOptinNotShownWhenSyncEnabled
#endif
// TODO(crbug.com/407964657): Merge this test with
// AvatarToolbarButtonBrowserTest.SyncError once the feature is enabled by
// default.
IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonHistorySyncOptinBrowserTest,
                       MAYBE_HistorySyncOptinNotShownWhenSyncEnabled) {
  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser());
  // Normal state.
  ASSERT_TRUE(avatar->GetText().empty());
  const AccountInfo account = EnableSyncWithImage(/*email=*/u"test@gmail.com");
  ASSERT_EQ(avatar->GetText(),
            l10n_util::GetStringFUTF16(IDS_AVATAR_BUTTON_GREETING,
                                       base::UTF8ToUTF16(account.given_name)));
  avatar->TriggerTimeoutForTesting(AvatarDelayType::kNameGreeting);
  // The greeting should NOT be followed by the history sync opt-in entry point
  // if sync is already enabled.
  EXPECT_TRUE(avatar->GetText().empty());
  SimulateSyncError();
  // The sync error should be shown.
  EXPECT_EQ(avatar->GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SYNC_ERROR));
  ClearSyncError();
  // After clearing the sync error, the history sync opt-in entry point should
  // NOT be shown.
  EXPECT_TRUE(avatar->GetText().empty());
}

// TODO(crbug.com/331746545): Check the flaky test issue on Windows.
// TODO(crbug.com/331746545): Re-enable this test
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
#define MAYBE_HistorySyncOptinNotShownWhenPromotionsDisabled \
  DISABLED_HistorySyncOptinNotShownWhenPromotionsDisabled
#else
#define MAYBE_HistorySyncOptinNotShownWhenPromotionsDisabled \
  HistorySyncOptinNotShownWhenPromotionsDisabled
#endif
IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonHistorySyncOptinBrowserTest,
                       MAYBE_HistorySyncOptinNotShownWhenPromotionsDisabled) {
  TestingBrowserProcess::GetGlobal()->local_state()->SetBoolean(
      prefs::kPromotionsEnabled, false);
  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser());
  // Normal state.
  ASSERT_TRUE(avatar->GetText().empty());
  const AccountInfo account = SigninWithImage(/*email=*/u"test@gmail.com");
  ASSERT_EQ(avatar->GetText(),
            l10n_util::GetStringFUTF16(IDS_AVATAR_BUTTON_GREETING,
                                       base::UTF8ToUTF16(account.given_name)));
  avatar->TriggerTimeoutForTesting(AvatarDelayType::kNameGreeting);
  // The greeting should NOT be followed by the history sync opt-in entry point
  // if promotions are disabled.
  EXPECT_TRUE(avatar->GetText().empty());
}

// TODO(crbug.com/331746545): Check the flaky test issue on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_HistorySyncOptinNotShownWhenSyncNotAllowed \
  DISABLED_HistorySyncOptinNotShownWhenSyncNotAllowed
#else
#define MAYBE_HistorySyncOptinNotShownWhenSyncNotAllowed \
  HistorySyncOptinNotShownWhenSyncNotAllowed
#endif
IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonHistorySyncOptinBrowserTest,
                       MAYBE_HistorySyncOptinNotShownWhenSyncNotAllowed) {
  SimulateDisableSyncByPolicyWithError();
  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser());
  // Normal state.
  ASSERT_TRUE(avatar->GetText().empty());
  const std::u16string account_name(u"Account name");
  SigninWithImage(/*email=*/u"test@gmail.com", account_name);
  ASSERT_EQ(avatar->GetText(), l10n_util::GetStringFUTF16(
                                   IDS_AVATAR_BUTTON_GREETING, account_name));
  avatar->TriggerTimeoutForTesting(AvatarDelayType::kNameGreeting);
  // The greeting should NOT be followed by the history sync opt-in entry point
  // if sync is not allowed.
  EXPECT_TRUE(avatar->GetText().empty());
}

enum class ManagedBy {
  kPolicy,
  kCustodian,
};

struct HistorySyncOptinSyncManagedTypeTestCase {
  ManagedBy managed_by;
  syncer::UserSelectableType managed_type;
};

class AvatarToolbarButtonHistorySyncOptinManagedTypeTest
    : public AvatarToolbarButtonHistorySyncOptinBrowserTest,
      public WithParamInterface<HistorySyncOptinSyncManagedTypeTestCase> {};

// TODO(crbug.com/331746545): Check the flaky test issue on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_HistorySyncOptinNotShownWhenSyncManaged \
  DISABLED_HistorySyncOptinNotShownWhenSyncManaged
#else
#define MAYBE_HistorySyncOptinNotShownWhenSyncManaged \
  HistorySyncOptinNotShownWhenSyncManaged
#endif
IN_PROC_BROWSER_TEST_P(AvatarToolbarButtonHistorySyncOptinManagedTypeTest,
                       MAYBE_HistorySyncOptinNotShownWhenSyncManaged) {
  switch (GetParam().managed_by) {
    case ManagedBy::kPolicy:
      SimulateTypeManagedByPolicy(GetParam().managed_type);
      break;
    case ManagedBy::kCustodian:
      SimulateTypeManagedByCustodian(GetParam().managed_type);
      break;
    default:
      NOTREACHED();
  }
  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser());
  // Normal state.
  ASSERT_TRUE(avatar->GetText().empty());
  const std::u16string account_name(u"Account name");
  SigninWithImage(/*email=*/u"test@gmail.com", account_name);
  ASSERT_EQ(avatar->GetText(), l10n_util::GetStringFUTF16(
                                   IDS_AVATAR_BUTTON_GREETING, account_name));
  avatar->TriggerTimeoutForTesting(AvatarDelayType::kNameGreeting);
  // The greeting should NOT be followed by the history sync opt-in entry point
  // if sync is not allowed.
  EXPECT_TRUE(avatar->GetText().empty());
}

const HistorySyncOptinSyncManagedTypeTestCase
    kHistorySyncOptinSyncManagedTypeTestCases[] = {
        {
            ManagedBy::kPolicy,
            syncer::UserSelectableType::kHistory,
        },
        {
            ManagedBy::kPolicy,
            syncer::UserSelectableType::kTabs,
        },
        {
            ManagedBy::kCustodian,
            syncer::UserSelectableType::kHistory,
        },
        {
            ManagedBy::kCustodian,
            syncer::UserSelectableType::kTabs,
        },
};

INSTANTIATE_TEST_SUITE_P(HistorySyncOptinManagedType,
                         AvatarToolbarButtonHistorySyncOptinManagedTypeTest,
                         ValuesIn(kHistorySyncOptinSyncManagedTypeTestCases));

// TODO(crbug.com/331746545): Check the flaky test issue on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_HistorySyncOptinThenPassphraseError \
  DISABLED_HistorySyncOptinThenPassphraseError
#else
#define MAYBE_HistorySyncOptinThenPassphraseError \
  HistorySyncOptinThenPassphraseError
#endif
IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonHistorySyncOptinBrowserTest,
                       MAYBE_HistorySyncOptinThenPassphraseError) {
  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser());
  // Normal state.
  ASSERT_TRUE(avatar->GetText().empty());
  const std::u16string account_name(u"Account name");
  SigninWithImage(/*email=*/u"test@gmail.com", account_name);
  ASSERT_EQ(avatar->GetText(), l10n_util::GetStringFUTF16(
                                   IDS_AVATAR_BUTTON_GREETING, account_name));
  avatar->TriggerTimeoutForTesting(AvatarDelayType::kNameGreeting);
  // The greeting should be followed by the history sync opt-in entry point.
  EXPECT_EQ(avatar->GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_BROWSE_ACROSS_DEVICES));
  SimulatePassphraseError();
  // The history sync opt-in entry point should be replaced by the passphrase
  // error message.
  EXPECT_EQ(avatar->GetText(), l10n_util::GetStringUTF16(
                                   IDS_SYNC_ERROR_USER_MENU_PASSPHRASE_BUTTON));
  ClearPassphraseError();
  // After clearing the passphrase error, the history sync opt-in entry point
  // should NOT be shown.
  EXPECT_TRUE(avatar->GetText().empty());
}

// TODO(crbug.com/331746545): Check the flaky test issue on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_HistorySyncOptinThenClientUpgradeError \
  DISABLED_HistorySyncOptinThenClientUpgradeError
#else
#define MAYBE_HistorySyncOptinThenClientUpgradeError \
  HistorySyncOptinThenClientUpgradeError
#endif
IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonHistorySyncOptinBrowserTest,
                       MAYBE_HistorySyncOptinThenClientUpgradeError) {
  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser());
  // Normal state.
  ASSERT_TRUE(avatar->GetText().empty());
  const std::u16string account_name(u"Account name");
  SigninWithImage(/*email=*/u"test@gmail.com", account_name);
  ASSERT_EQ(avatar->GetText(), l10n_util::GetStringFUTF16(
                                   IDS_AVATAR_BUTTON_GREETING, account_name));
  avatar->TriggerTimeoutForTesting(AvatarDelayType::kNameGreeting);
  // The greeting should be followed by the history sync opt-in entry point.
  EXPECT_EQ(avatar->GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_BROWSE_ACROSS_DEVICES));
  SimulateUpgradeClientError();
  // The history sync opt-in entry point should be replaced by the passphrase
  // error message.
  EXPECT_EQ(avatar->GetText(),
            l10n_util::GetStringUTF16(IDS_SYNC_ERROR_USER_MENU_UPGRADE_BUTTON));
  ClearUpgradeClientError();
  // After clearing the passphrase error, the history sync opt-in entry point
  // should NOT be shown.
  EXPECT_TRUE(avatar->GetText().empty());
}

// TODO(crbug.com/331746545): Check the flaky test issue on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_HistorySyncOptinThenSigninPending \
  DISABLED_HistorySyncOptinThenSigninPending
#else
#define MAYBE_HistorySyncOptinThenSigninPending \
  HistorySyncOptinThenSigninPending
#endif
IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonHistorySyncOptinBrowserTest,
                       MAYBE_HistorySyncOptinThenSigninPending) {
  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser());
  // Normal state.
  ASSERT_TRUE(avatar->GetText().empty());
  const std::u16string account_name(u"Account name");
  SigninWithImage(/*email=*/u"test@gmail.com", account_name);
  ASSERT_EQ(avatar->GetText(), l10n_util::GetStringFUTF16(
                                   IDS_AVATAR_BUTTON_GREETING, account_name));
  avatar->TriggerTimeoutForTesting(AvatarDelayType::kNameGreeting);
  // The greeting should be followed by the history sync opt-in entry point.
  EXPECT_EQ(avatar->GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_BROWSE_ACROSS_DEVICES));
  SimulateSigninError(/*web_sign_out=*/false);
  // The history sync opt-in entry point should be replaced by the signin
  // pending message.
  EXPECT_EQ(avatar->GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SIGNIN_PAUSED));
  ClearSigninError();
  // After clearing the sign in error, the history sync opt-in entry point
  // should NOT be shown.
  EXPECT_TRUE(avatar->GetText().empty());
}

// TODO(crbug.com/331746545): Check the flaky test issue on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_HistorySyncOptinThenExplicitText \
  DISABLED_HistorySyncOptinThenExplicitText
#else
#define MAYBE_HistorySyncOptinThenExplicitText HistorySyncOptinThenExplicitText
#endif
IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonHistorySyncOptinBrowserTest,
                       MAYBE_HistorySyncOptinThenExplicitText) {
  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser());
  // Normal state.
  ASSERT_TRUE(avatar->GetText().empty());
  const std::u16string account_name(u"Account name");
  SigninWithImage(/*email=*/u"test@gmail.com", account_name);
  ASSERT_EQ(avatar->GetText(), l10n_util::GetStringFUTF16(
                                   IDS_AVATAR_BUTTON_GREETING, account_name));
  avatar->TriggerTimeoutForTesting(AvatarDelayType::kNameGreeting);
  // The greeting should be followed by the history sync opt-in entry point.
  EXPECT_EQ(avatar->GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_BROWSE_ACROSS_DEVICES));
  const std::u16string explicit_text(u"Explicit Text");
  base::ScopedClosureRunner hide_callback = avatar->SetExplicitButtonState(
      explicit_text, /*accessibility_label=*/std::nullopt,
      /*explicit_action=*/std::nullopt);
  // The history sync opt-in entry point should be replaced by the explicit
  // text message.
  EXPECT_EQ(avatar->GetText(), explicit_text);
  hide_callback.RunAndReset();
  // After clearing the explicit text, the history sync opt-in entry point
  // should NOT be shown.
  EXPECT_TRUE(avatar->GetText().empty());
}

// TODO(crbug.com/331746545): Check the flaky test issue on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_HistorySyncOptinNotShownIfErrorBeforeGreetingTimesOut \
  DISABLED_HistorySyncOptinNotShownIfErrorBeforeGreetingTimesOut
#else
#define MAYBE_HistorySyncOptinNotShownIfErrorBeforeGreetingTimesOut \
  HistorySyncOptinNotShownIfErrorBeforeGreetingTimesOut
#endif
IN_PROC_BROWSER_TEST_F(
    AvatarToolbarButtonHistorySyncOptinBrowserTest,
    MAYBE_HistorySyncOptinNotShownIfErrorBeforeGreetingTimesOut) {
  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser());
  // Normal state.
  ASSERT_TRUE(avatar->GetText().empty());
  const std::u16string account_name(u"Account name");
  SigninWithImage(/*email=*/u"test@gmail.com", account_name);
  ASSERT_EQ(avatar->GetText(), l10n_util::GetStringFUTF16(
                                   IDS_AVATAR_BUTTON_GREETING, account_name));
  SimulatePassphraseError();
  avatar->TriggerTimeoutForTesting(AvatarDelayType::kNameGreeting);
  // No history sync opt-in entry point should be shown if the error is shown
  // before the greeting times out.
  EXPECT_EQ(avatar->GetText(), l10n_util::GetStringUTF16(
                                   IDS_SYNC_ERROR_USER_MENU_PASSPHRASE_BUTTON));
  ClearPassphraseError();
  // After clearing the passphrase error, the history sync opt-in entry point
  // should NOT be shown.
  EXPECT_TRUE(avatar->GetText().empty());
}

struct HistorySyncOptinExpansionPillOptionTestCase {
  std::string feature_param;
  int expected_history_sync_message_id;
};

class AvatarToolbarButtonHistorySyncOptinWithParamBrowserTest
    : public AvatarToolbarButtonHistorySyncOptinBrowserTest,
      public WithParamInterface<HistorySyncOptinExpansionPillOptionTestCase> {
 public:
  AvatarToolbarButtonHistorySyncOptinWithParamBrowserTest()
      : AvatarToolbarButtonHistorySyncOptinBrowserTest(/*feature_parameters=*/
                                                       {{"history-sync-optin-"
                                                         "expansion-pill-"
                                                         "option",
                                                         GetParam()
                                                             .feature_param}}) {
  }
};

// TODO(crbug.com/331746545): Check the flaky test issue on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_CollapsesOnSyncTurnedOn DISABLED_CollapsesOnSyncTurnedOn
#else
#define MAYBE_CollapsesOnSyncTurnedOn CollapsesOnSyncTurnedOn
#endif
IN_PROC_BROWSER_TEST_P(AvatarToolbarButtonHistorySyncOptinWithParamBrowserTest,
                       MAYBE_CollapsesOnSyncTurnedOn) {
  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser());
  // Normal state.
  ASSERT_TRUE(avatar->GetText().empty());
  const std::u16string email(u"test@gmail.com");
  const std::u16string account_name(u"Account name");
  const AccountInfo account_info = SigninWithImage(email, account_name);
  EXPECT_EQ(avatar->GetText(), l10n_util::GetStringFUTF16(
                                   IDS_AVATAR_BUTTON_GREETING, account_name));
  avatar->TriggerTimeoutForTesting(AvatarDelayType::kNameGreeting);
  // The greeting should be followed by the history sync opt-in entry point.
  EXPECT_EQ(
      avatar->GetText(),
      l10n_util::GetStringUTF16(GetParam().expected_history_sync_message_id));
  EnableSync(email, account_name);
  // Once sync is turned on, the button should return to the normal state.
  EXPECT_TRUE(avatar->GetText().empty());
}

// TODO(crbug.com/331746545): Check the flaky test issue on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_CollapsesOnSignOut DISABLED_CollapsesOnSignOut
#else
#define MAYBE_CollapsesOnSignOut CollapsesOnSignOut
#endif
IN_PROC_BROWSER_TEST_P(AvatarToolbarButtonHistorySyncOptinWithParamBrowserTest,
                       MAYBE_CollapsesOnSignOut) {
  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser());
  // Normal state.
  ASSERT_TRUE(avatar->GetText().empty());
  const std::u16string email(u"test@gmail.com");
  const std::u16string account_name(u"Account name");
  const AccountInfo account_info = SigninWithImage(email, account_name);
  EXPECT_EQ(avatar->GetText(), l10n_util::GetStringFUTF16(
                                   IDS_AVATAR_BUTTON_GREETING, account_name));
  avatar->TriggerTimeoutForTesting(AvatarDelayType::kNameGreeting);
  // The greeting should be followed by the history sync opt-in entry point.
  EXPECT_EQ(
      avatar->GetText(),
      l10n_util::GetStringUTF16(GetParam().expected_history_sync_message_id));
  Signout();
  // Once the user signs out, the button should return to the normal state.
  EXPECT_TRUE(avatar->GetText().empty());
}

// TODO(crbug.com/331746545): Check the flaky test issue on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_PRE_ShowsOnBrowserRestart DISABLED_PRE_ShowsOnBrowserRestart
#define MAYBE_ShowsOnBrowserRestart DISABLED_ShowsOnBrowserRestart
#else
#define MAYBE_PRE_ShowsOnBrowserRestart PRE_ShowsOnBrowserRestart
#define MAYBE_ShowsOnBrowserRestart ShowsOnBrowserRestart
#endif
IN_PROC_BROWSER_TEST_P(AvatarToolbarButtonHistorySyncOptinWithParamBrowserTest,
                       MAYBE_PRE_ShowsOnBrowserRestart) {
  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser());
  // Normal state.
  ASSERT_TRUE(avatar->GetText().empty());
  const std::u16string email(u"test@gmail.com");
  const std::u16string account_name(u"Account name");
  const AccountInfo account_info = SigninWithImage(email, account_name);
  EXPECT_EQ(avatar->GetText(), l10n_util::GetStringFUTF16(
                                   IDS_AVATAR_BUTTON_GREETING, account_name));
  avatar->TriggerTimeoutForTesting(AvatarDelayType::kNameGreeting);
  // The greeting should be followed by the history sync opt-in entry point.
  EXPECT_EQ(
      avatar->GetText(),
      l10n_util::GetStringUTF16(GetParam().expected_history_sync_message_id));
  avatar->TriggerTimeoutForTesting(AvatarDelayType::kHistorySyncOptin);
  // The button should return to the normal state.
  EXPECT_TRUE(avatar->GetText().empty());
}

IN_PROC_BROWSER_TEST_P(AvatarToolbarButtonHistorySyncOptinWithParamBrowserTest,
                       MAYBE_ShowsOnBrowserRestart) {
  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser());
  // The greeting is shown after the restart.
  ASSERT_EQ(
      avatar->GetText(),
      l10n_util::GetStringFUTF16(IDS_AVATAR_BUTTON_GREETING, u"Account name"));
  avatar->TriggerTimeoutForTesting(AvatarDelayType::kNameGreeting);
  // The greeting should be followed by the history sync opt-in entry point.
  EXPECT_EQ(
      avatar->GetText(),
      l10n_util::GetStringUTF16(GetParam().expected_history_sync_message_id));
  avatar->TriggerTimeoutForTesting(AvatarDelayType::kHistorySyncOptin);
  // The button should return to the normal state.
  EXPECT_TRUE(avatar->GetText().empty());
}

// TODO(crbug.com/331746545): Check the flaky test issue on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_HistorySyncOptinShowsAfterGreetingAndOnInactivity \
  DISABLED_HistorySyncOptinShowsAfterGreetingAndOnInactivity
#else
#define MAYBE_HistorySyncOptinShowsAfterGreetingAndOnInactivity \
  HistorySyncOptinShowsAfterGreetingAndOnInactivity
#endif
IN_PROC_BROWSER_TEST_P(
    AvatarToolbarButtonHistorySyncOptinWithParamBrowserTest,
    MAYBE_HistorySyncOptinShowsAfterGreetingAndOnInactivity) {
  base::TimeDelta last_active_time;
  RunTestSequence(SetLastActive(last_active_time));
  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser());
  // Normal state.
  ASSERT_TRUE(avatar->GetText().empty());
  const std::u16string account_name(u"Account name");
  const AccountInfo account_info =
      Signin(/*email=*/u"test@gmail.com", account_name);
  // Simulate inactivity for enough time to trigger the new session.
  last_active_time += user_education::features::GetIdleTimeBetweenSessions();
  RunTestSequence(SetLastActive(last_active_time));
  // The history sync opt-in entry point should NOT be shown after the
  // inactivity period if the greeting has not been shown yet.
  EXPECT_TRUE(avatar->GetText().empty());
  AddSignedInImage(account_info.account_id);
  EXPECT_EQ(avatar->GetText(), l10n_util::GetStringFUTF16(
                                   IDS_AVATAR_BUTTON_GREETING, account_name));
  avatar->TriggerTimeoutForTesting(AvatarDelayType::kNameGreeting);
  // The greeting should be followed by the history sync opt-in entry point.
  EXPECT_EQ(
      avatar->GetText(),
      l10n_util::GetStringUTF16(GetParam().expected_history_sync_message_id));
  avatar->TriggerTimeoutForTesting(AvatarDelayType::kHistorySyncOptin);
  // Once the history sync opt-in entry point collapses, the button should
  // return to the normal state.
  EXPECT_TRUE(avatar->GetText().empty());
  // Simulate inactivity for enough time to trigger the new session.
  last_active_time += user_education::features::GetIdleTimeBetweenSessions();
  RunTestSequence(SetLastActive(last_active_time));
  // The history sync opt-in entry point should be shown again after the
  // inactivity period.
  EXPECT_EQ(
      avatar->GetText(),
      l10n_util::GetStringUTF16(GetParam().expected_history_sync_message_id));
  avatar->TriggerTimeoutForTesting(AvatarDelayType::kHistorySyncOptin);
  // Once the history sync opt-in entry point collapses, the button should
  // return to the normal state.
  EXPECT_TRUE(avatar->GetText().empty());
  // Simulate inactivity for short time to not trigger the new session.
  const base::TimeDelta short_inactivity = base::Minutes(30);
  ASSERT_GT(user_education::features::GetIdleTimeBetweenSessions(),
            short_inactivity);
  last_active_time += short_inactivity;
  RunTestSequence(SetLastActive(last_active_time));
  // The history sync opt-in entry point should NOT be shown after the short
  // inactivity period.
  EXPECT_TRUE(avatar->GetText().empty());
}

// TODO(crbug.com/331746545): Check the flaky test issue on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_HistorySyncOptinNotShownIfMaxShownCountReached \
  DISABLED_HistorySyncOptinNotShownIfMaxShownCountReached
#else
#define MAYBE_HistorySyncOptinNotShownIfMaxShownCountReached \
  HistorySyncOptinNotShownIfMaxShownCountReached
#endif
IN_PROC_BROWSER_TEST_P(AvatarToolbarButtonHistorySyncOptinWithParamBrowserTest,
                       MAYBE_HistorySyncOptinNotShownIfMaxShownCountReached) {
  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser());
  // Normal state.
  ASSERT_TRUE(avatar->GetText().empty());
  const std::u16string account_name_1(u"Account name");
  SigninWithImage(/*email=*/u"test@gmail.com", account_name_1);
  ASSERT_EQ(avatar->GetText(), l10n_util::GetStringFUTF16(
                                   IDS_AVATAR_BUTTON_GREETING, account_name_1));
  avatar->TriggerTimeoutForTesting(AvatarDelayType::kNameGreeting);
  // The greeting should be followed by the history sync opt-in entry point.
  EXPECT_EQ(
      avatar->GetText(),
      l10n_util::GetStringUTF16(GetParam().expected_history_sync_message_id));
  int shown_count = 1;
  avatar->TriggerTimeoutForTesting(AvatarDelayType::kHistorySyncOptin);
  // The button comes back to the normal state.
  EXPECT_TRUE(avatar->GetText().empty());
  for (; shown_count < user_education::features::GetNewBadgeShowCount();
       ++shown_count) {
    // Simulate inactivity for enough time to trigger the new session.
    RunTestSequence(SetLastActive(
        shown_count * user_education::features::GetIdleTimeBetweenSessions()));
    EXPECT_EQ(
        avatar->GetText(),
        l10n_util::GetStringUTF16(GetParam().expected_history_sync_message_id));
    avatar->TriggerTimeoutForTesting(AvatarDelayType::kHistorySyncOptin);
    // The button comes back to the normal state.
    EXPECT_TRUE(avatar->GetText().empty());
  }
  RunTestSequence(SetLastActive(
      shown_count * user_education::features::GetIdleTimeBetweenSessions()));
  // The history sync opt-in entry point should NOT be shown after the
  // inactivity period if the max shown count has been reached.
  EXPECT_TRUE(avatar->GetText().empty());

  Signout();
  const std::u16string account_name_2(u"Account name 2");
  SigninWithImage(/*email=*/u"test2@gmail.com", account_name_2);
  ASSERT_EQ(avatar->GetText(), l10n_util::GetStringFUTF16(
                                   IDS_AVATAR_BUTTON_GREETING, account_name_2));
  avatar->TriggerTimeoutForTesting(AvatarDelayType::kNameGreeting);
  // The greeting should be followed by the history sync opt-in entry point
  // (rate limiting is per account).
  EXPECT_EQ(
      avatar->GetText(),
      l10n_util::GetStringUTF16(GetParam().expected_history_sync_message_id));
}

const HistorySyncOptinExpansionPillOptionTestCase kHistorySyncOptinTestCases[] =
    {
        {
            "browse-across-devices",
            IDS_AVATAR_BUTTON_BROWSE_ACROSS_DEVICES,
        },
        {
            "sync-history",
            IDS_AVATAR_BUTTON_SYNC_HISTORY,
        },
        {
            "see-tabs-from-other-devices",
            IDS_AVATAR_BUTTON_SEE_TABS_FROM_OTHER_DEVICES,
        },
        {
            "browse-across-devices-new-profile-menu-promo-variant",
            IDS_AVATAR_BUTTON_BROWSE_ACROSS_DEVICES,
        },
};

INSTANTIATE_TEST_SUITE_P(
    HistorySyncOptinExpansionPillOptions,
    AvatarToolbarButtonHistorySyncOptinWithParamBrowserTest,
    ValuesIn(kHistorySyncOptinTestCases));

class AvatarToolbarButtonHistorySyncOptinClickBrowserTest
    : public AvatarToolbarButtonHistorySyncOptinWithParamBrowserTest {
 protected:
  AvatarToolbarButtonHistorySyncOptinClickBrowserTest()
      : delegate_auto_reset_(signin_ui_util::SetSigninUiDelegateForTesting(
            &mock_signin_ui_delegate_)) {}

  void Click(views::View* clickable_view) {
    clickable_view->OnMousePressed(
        ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0));
    clickable_view->OnMouseReleased(ui::MouseEvent(
        ui::EventType::kMouseReleased, gfx::Point(), gfx::Point(),
        ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0));
  }

  void ClickSyncButton(ProfileMenuViewBase* profile_menu_view) {
    ASSERT_NE(profile_menu_view, nullptr);
    profile_menu_view->GetFocusManager()->AdvanceFocus(/*reverse=*/false);
    views::View* focused_item =
        profile_menu_view->GetFocusManager()->GetFocusedView();
    ASSERT_NE(focused_item, nullptr);
    Click(focused_item);
  }

  StrictMock<MockSigninUiDelegate> mock_signin_ui_delegate_;

 private:
  base::AutoReset<signin_ui_util::SigninUiDelegate*> delegate_auto_reset_;
};

// TODO(crbug.com/331746545): Check the flaky test issue on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_CollapsesOnClickAndTriggersProfileMenuStartup \
  DISABLED_CollapsesOnClickAndTriggersProfileMenuStartup
#else
#define MAYBE_CollapsesOnClickAndTriggersProfileMenuStartup \
  CollapsesOnClickAndTriggersProfileMenuStartup
#endif
IN_PROC_BROWSER_TEST_P(AvatarToolbarButtonHistorySyncOptinClickBrowserTest,
                       MAYBE_CollapsesOnClickAndTriggersProfileMenuStartup) {
  base::HistogramTester histogram_tester;
  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser());
  // Normal state.
  ASSERT_TRUE(avatar->GetText().empty());
  ASSERT_FALSE(avatar->HasExplicitButtonAction());
  const std::u16string account_name(u"Account name");
  const AccountInfo account_info =
      SigninWithImage(/*email=*/u"test@gmail.com", account_name);
  ASSERT_EQ(avatar->GetText(), l10n_util::GetStringFUTF16(
                                   IDS_AVATAR_BUTTON_GREETING, account_name));
  avatar->TriggerTimeoutForTesting(AvatarDelayType::kNameGreeting);
  // The greeting should be followed by the history sync opt-in entry point.
  EXPECT_EQ(
      avatar->GetText(),
      l10n_util::GetStringUTF16(GetParam().expected_history_sync_message_id));
  // `Signin.SyncOptIn.IdentityPill.Shown` should be recorded with the correct
  // access point.
  histogram_tester.ExpectBucketCount(
      "Signin.SyncOptIn.IdentityPill.Shown",
      signin_metrics::AccessPoint::kHistorySyncOptinExpansionPillOnStartup,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Signin.SyncOptIn.IdentityPill.Shown",
      signin_metrics::AccessPoint::kHistorySyncOptinExpansionPillOnInactivity,
      /*expected_count=*/0);
  // The button action should be overridden.
  EXPECT_TRUE(avatar->HasExplicitButtonAction());
  histogram_tester.ExpectTotalCount(
      "Signin.SyncOptIn.IdentityPill.DurationBeforeClick",
      /*expected_count=*/0);
  Click(avatar);
  histogram_tester.ExpectTotalCount(
      "Signin.SyncOptIn.IdentityPill.DurationBeforeClick",
      /*expected_count=*/1);
  auto* coordinator = ProfileMenuCoordinator::FromBrowser(browser());
  ASSERT_NE(coordinator, nullptr);
  EXPECT_TRUE(coordinator->IsShowing());
  EXPECT_TRUE(avatar->GetText().empty());
  // Once the history sync opt-in entry point collapses, the button action
  // should be reset to the default behavior.
  EXPECT_FALSE(avatar->HasExplicitButtonAction());
  // Clicking the sync button in the profile menu should trigger the sync
  // dialog with the correct access point
  // (`kHistorySyncOptinExpansionPillOnStartup`).
  EXPECT_CALL(
      mock_signin_ui_delegate_,
      ShowTurnSyncOnUI(
          browser()->profile(),
          signin_metrics::AccessPoint::kHistorySyncOptinExpansionPillOnStartup,
          signin_metrics::PromoAction::PROMO_ACTION_WITH_DEFAULT,
          account_info.account_id,
          TurnSyncOnHelper::SigninAbortedMode::KEEP_ACCOUNT,
          /*is_sync_promo=*/false,
          /*turn_sync_on_signed_profile=*/true));
  ASSERT_NO_FATAL_FAILURE(
      ClickSyncButton(coordinator->GetProfileMenuViewBaseForTesting()));
}

// TODO(crbug.com/331746545): Check the flaky test issue on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_CollapsesOnClickAndTriggersProfileMenuInactivity \
  DISABLED_CollapsesOnClickAndTriggersProfileMenuInactivity
#else
#define MAYBE_CollapsesOnClickAndTriggersProfileMenuInactivity \
  CollapsesOnClickAndTriggersProfileMenuInactivity
#endif
IN_PROC_BROWSER_TEST_P(AvatarToolbarButtonHistorySyncOptinClickBrowserTest,
                       MAYBE_CollapsesOnClickAndTriggersProfileMenuInactivity) {
  base::HistogramTester histogram_tester;
  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser());
  // Normal state.
  ASSERT_TRUE(avatar->GetText().empty());
  ASSERT_FALSE(avatar->HasExplicitButtonAction());
  const std::u16string account_name(u"Account name");
  const AccountInfo account_info =
      SigninWithImage(/*email=*/u"test@gmail.com", account_name);
  ASSERT_EQ(avatar->GetText(), l10n_util::GetStringFUTF16(
                                   IDS_AVATAR_BUTTON_GREETING, account_name));
  avatar->TriggerTimeoutForTesting(AvatarDelayType::kNameGreeting);
  // The greeting should be followed by the history sync opt-in entry point.
  EXPECT_EQ(
      avatar->GetText(),
      l10n_util::GetStringUTF16(GetParam().expected_history_sync_message_id));
  // `Signin.SyncOptIn.IdentityPill.Shown` should be recorded with the correct
  // access point.
  histogram_tester.ExpectBucketCount(
      "Signin.SyncOptIn.IdentityPill.Shown",
      signin_metrics::AccessPoint::kHistorySyncOptinExpansionPillOnStartup,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Signin.SyncOptIn.IdentityPill.Shown",
      signin_metrics::AccessPoint::kHistorySyncOptinExpansionPillOnInactivity,
      /*expected_count=*/0);
  EXPECT_TRUE(avatar->HasExplicitButtonAction());
  avatar->TriggerTimeoutForTesting(AvatarDelayType::kHistorySyncOptin);
  // The button comes back to the normal state.
  EXPECT_TRUE(avatar->GetText().empty());
  EXPECT_FALSE(avatar->HasExplicitButtonAction());
  // Simulate inactivity for enough time to trigger the new session.
  RunTestSequence(
      SetLastActive(user_education::features::GetIdleTimeBetweenSessions()));
  // The history sync opt-in entry point should be shown again after the
  // inactivity period.
  EXPECT_EQ(
      avatar->GetText(),
      l10n_util::GetStringUTF16(GetParam().expected_history_sync_message_id));
  // `Signin.SyncOptIn.IdentityPill.Shown` should be recorded with the correct
  // access point.
  histogram_tester.ExpectBucketCount(
      "Signin.SyncOptIn.IdentityPill.Shown",
      signin_metrics::AccessPoint::kHistorySyncOptinExpansionPillOnStartup,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Signin.SyncOptIn.IdentityPill.Shown",
      signin_metrics::AccessPoint::kHistorySyncOptinExpansionPillOnInactivity,
      /*expected_count=*/1);
  // The button action should be overridden.
  EXPECT_TRUE(avatar->HasExplicitButtonAction());
  histogram_tester.ExpectTotalCount(
      "Signin.SyncOptIn.IdentityPill.DurationBeforeClick",
      /*expected_count=*/0);
  Click(avatar);
  histogram_tester.ExpectTotalCount(
      "Signin.SyncOptIn.IdentityPill.DurationBeforeClick",
      /*expected_count=*/1);
  auto* coordinator = ProfileMenuCoordinator::FromBrowser(browser());
  ASSERT_NE(coordinator, nullptr);
  EXPECT_TRUE(coordinator->IsShowing());
  // The button comes back to the normal state.
  EXPECT_TRUE(avatar->GetText().empty());
  EXPECT_FALSE(avatar->HasExplicitButtonAction());
  EXPECT_TRUE(coordinator->IsShowing());
  // Clicking the sync button in the profile menu should trigger the sync
  // dialog with the correct access point
  // (`kHistorySyncOptinExpansionPillOnInactivity`).
  EXPECT_CALL(
      mock_signin_ui_delegate_,
      ShowTurnSyncOnUI(browser()->profile(),
                       signin_metrics::AccessPoint::
                           kHistorySyncOptinExpansionPillOnInactivity,
                       signin_metrics::PromoAction::PROMO_ACTION_WITH_DEFAULT,
                       account_info.account_id,
                       TurnSyncOnHelper::SigninAbortedMode::KEEP_ACCOUNT,
                       /*is_sync_promo=*/false,
                       /*turn_sync_on_signed_profile=*/true));
  ASSERT_NO_FATAL_FAILURE(
      ClickSyncButton(coordinator->GetProfileMenuViewBaseForTesting()));
}

// TODO(crbug.com/331746545): Check the flaky test issue on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_HistorySyncOptinNotShownIfUsedLimitReached \
  DISABLED_HistorySyncOptinNotShownIfUsedLimitReached
#else
#define MAYBE_HistorySyncOptinNotShownIfUsedLimitReached \
  HistorySyncOptinNotShownIfUsedLimitReached
#endif
IN_PROC_BROWSER_TEST_P(AvatarToolbarButtonHistorySyncOptinClickBrowserTest,
                       MAYBE_HistorySyncOptinNotShownIfUsedLimitReached) {
  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser());
  // Normal state.
  ASSERT_TRUE(avatar->GetText().empty());
  const std::u16string account_name_1(u"Account name");
  SigninWithImage(/*email=*/u"test@gmail.com", account_name_1);
  ASSERT_EQ(avatar->GetText(), l10n_util::GetStringFUTF16(
                                   IDS_AVATAR_BUTTON_GREETING, account_name_1));
  avatar->TriggerTimeoutForTesting(AvatarDelayType::kNameGreeting);
  // The greeting should be followed by the history sync opt-in entry point.
  EXPECT_EQ(
      avatar->GetText(),
      l10n_util::GetStringUTF16(GetParam().expected_history_sync_message_id));
  // The button action should be overridden.
  EXPECT_TRUE(avatar->HasExplicitButtonAction());
  Click(avatar);
  // The button comes back to the normal state.
  EXPECT_TRUE(avatar->GetText().empty());
  int used_count = 1;
  for (; used_count < user_education::features::GetNewBadgeFeatureUsedCount();
       ++used_count) {
    // Simulate inactivity for enough time to trigger the new session.
    RunTestSequence(SetLastActive(
        used_count * user_education::features::GetIdleTimeBetweenSessions()));
    EXPECT_EQ(
        avatar->GetText(),
        l10n_util::GetStringUTF16(GetParam().expected_history_sync_message_id));
    Click(avatar);
    // The button comes back to the normal state.
    EXPECT_TRUE(avatar->GetText().empty());
  }
  RunTestSequence(SetLastActive(
      used_count * user_education::features::GetIdleTimeBetweenSessions()));
  // The history sync opt-in entry point should NOT be shown after the
  // inactivity period if the max used count has been reached.
  EXPECT_TRUE(avatar->GetText().empty());

  Signout();
  const std::u16string account_name_2(u"Account name 2");
  SigninWithImage(/*email=*/u"test2@gmail.com", account_name_2);
  ASSERT_EQ(avatar->GetText(), l10n_util::GetStringFUTF16(
                                   IDS_AVATAR_BUTTON_GREETING, account_name_2));
  avatar->TriggerTimeoutForTesting(AvatarDelayType::kNameGreeting);
  // The greeting should be followed by the history sync opt-in entry point
  // (rate limiting is per account).
  EXPECT_EQ(
      avatar->GetText(),
      l10n_util::GetStringUTF16(GetParam().expected_history_sync_message_id));
}

// TODO(crbug.com/331746545): Check the flaky test issue on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_TriggersAndCollapsesConsistentlyAcrossMultipleBrowsers \
  DISABLED_TriggersAndCollapsesConsistentlyAcrossMultipleBrowsers
#else
#define MAYBE_TriggersAndCollapsesConsistentlyAcrossMultipleBrowsers \
  TriggersAndCollapsesConsistentlyAcrossMultipleBrowsers
#endif
IN_PROC_BROWSER_TEST_P(
    AvatarToolbarButtonHistorySyncOptinClickBrowserTest,
    MAYBE_TriggersAndCollapsesConsistentlyAcrossMultipleBrowsers) {
  // Make the delay for cross window animation replay zero to avoid flakiness.
  base::AutoReset<std::optional<base::TimeDelta>> delay_override_reset =
      signin_ui_util::
          CreateZeroOverrideDelayForCrossWindowAnimationReplayForTesting();
  base::HistogramTester histogram_tester;
  Profile* profile = browser()->profile();
  Browser* browser_1 = browser();
  AvatarToolbarButton* avatar_1 = GetAvatarToolbarButton(browser_1);
  // Normal state.
  ASSERT_TRUE(avatar_1->GetText().empty());
  const std::u16string account_name(u"Account name");
  const AccountInfo account_info =
      SigninWithImage(/*email=*/u"test@gmail.com", account_name);
  ASSERT_EQ(avatar_1->GetText(), l10n_util::GetStringFUTF16(
                                     IDS_AVATAR_BUTTON_GREETING, account_name));
  avatar_1->TriggerTimeoutForTesting(AvatarDelayType::kNameGreeting);

  // The greeting should be followed by the history sync opt-in.
  EXPECT_EQ(
      avatar_1->GetText(),
      l10n_util::GetStringUTF16(GetParam().expected_history_sync_message_id));
  // Open the second browser before the history sync opt-in collapses.
  Browser* browser_2 = CreateBrowser(profile);
  AvatarToolbarButton* avatar_2 = GetAvatarToolbarButton(browser_2);
  // The history sync opt-in should be shown in the second browser as well.
  EXPECT_EQ(
      avatar_2->GetText(),
      l10n_util::GetStringUTF16(GetParam().expected_history_sync_message_id));
  // `Signin.SyncOptIn.IdentityPill.Shown` histogram should be recorded only
  // once.
  histogram_tester.ExpectBucketCount(
      "Signin.SyncOptIn.IdentityPill.Shown",
      signin_metrics::AccessPoint::kHistorySyncOptinExpansionPillOnStartup,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Signin.SyncOptIn.IdentityPill.Shown",
      signin_metrics::AccessPoint::kHistorySyncOptinExpansionPillOnInactivity,
      /*expected_count=*/0);
  avatar_1->TriggerTimeoutForTesting(AvatarDelayType::kHistorySyncOptin);
  // The button in both browsers comes back to the normal state.
  EXPECT_TRUE(avatar_1->GetText().empty());
  EXPECT_TRUE(avatar_2->GetText().empty());

  // Simulate inactivity for enough time to trigger the new session.
  RunTestSequence(
      SetLastActive(user_education::features::GetIdleTimeBetweenSessions()));
  // The history sync opt-in entry point should be shown again after the
  // inactivity period (in both browsers).
  EXPECT_EQ(
      avatar_1->GetText(),
      l10n_util::GetStringUTF16(GetParam().expected_history_sync_message_id));
  EXPECT_EQ(
      avatar_2->GetText(),
      l10n_util::GetStringUTF16(GetParam().expected_history_sync_message_id));
  // Open the third browser before the history sync opt-in collapses.
  Browser* browser_3 = CreateBrowser(profile);
  AvatarToolbarButton* avatar_3 = GetAvatarToolbarButton(browser_3);
  // The history sync opt-in should be shown in the third browser as well.
  EXPECT_EQ(
      avatar_3->GetText(),
      l10n_util::GetStringUTF16(GetParam().expected_history_sync_message_id));
  // `Signin.SyncOptIn.IdentityPill.Shown` histogram should be recorded only
  // once.
  histogram_tester.ExpectBucketCount(
      "Signin.SyncOptIn.IdentityPill.Shown",
      signin_metrics::AccessPoint::kHistorySyncOptinExpansionPillOnStartup,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Signin.SyncOptIn.IdentityPill.Shown",
      signin_metrics::AccessPoint::kHistorySyncOptinExpansionPillOnInactivity,
      /*expected_count=*/1);
  // Clicking the button on any browser should collapse the history sync opt-in
  // in all browsers.
  Click(avatar_2);
  // `Signin.SyncOptIn.IdentityPill.DurationBeforeClick` histogram should be
  // recorded only once.
  histogram_tester.ExpectTotalCount(
      "Signin.SyncOptIn.IdentityPill.DurationBeforeClick",
      /*expected_count=*/1);
  EXPECT_TRUE(avatar_1->GetText().empty());
  EXPECT_TRUE(avatar_2->GetText().empty());
  EXPECT_TRUE(avatar_3->GetText().empty());
}

INSTANTIATE_TEST_SUITE_P(HistorySyncOptinExpansionPillOptions,
                         AvatarToolbarButtonHistorySyncOptinClickBrowserTest,
                         ValuesIn(kHistorySyncOptinTestCases));
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

// Test suite for testing `AvatarToolbarButton`'s responsibility of updating
// color information in `ProfileAttributesStorage`.
class AvatarToolbarButtonProfileColorBrowserTest
    : public AvatarToolbarButtonBrowserTest,
      public WithParamInterface<ColorThemeType> {
 public:
  AvatarToolbarButtonProfileColorBrowserTest() = default;

  void SetUpOnMainThread() override {
    AvatarToolbarButtonBrowserTest::SetUpOnMainThread();
    theme_service(browser()->profile())
        ->SetBrowserColorScheme(ThemeService::BrowserColorScheme::kLight);
  }

  void SetColorTheme(Profile* profile, SkColor color) {
    ThemeService* service = theme_service(profile);
    switch (GetParam()) {
      case ColorThemeType::kAutogeneratedTheme:
        service->BuildAutogeneratedThemeFromColor(color);
        break;
      case ColorThemeType::kUserColor:
        service->SetUserColorAndBrowserColorVariant(color, kColorVariant);
        service->UseDeviceTheme(false);
        break;
    }
  }

  void SetDefaultTheme(Profile* profile) {
    ThemeService* service = theme_service(profile);
    switch (GetParam()) {
      case ColorThemeType::kAutogeneratedTheme:
        service->UseDefaultTheme();
        break;
      case ColorThemeType::kUserColor:
        service->SetUserColorAndBrowserColorVariant(SK_ColorTRANSPARENT,
                                                    kColorVariant);
        service->UseDeviceTheme(false);
        break;
    }
  }

  ThemeService* theme_service(Profile* profile) {
    return ThemeServiceFactory::GetForProfile(profile);
  }

  ProfileThemeColors ComputeProfileThemeColorsForBrowser(
      Browser* target_browser = nullptr) {
    target_browser = target_browser ? target_browser : browser();
    return GetCurrentProfileThemeColors(
        *target_browser->window()->GetColorProvider(),
        *ThemeServiceFactory::GetForProfile(target_browser->profile()));
  }
};

// Tests that the profile theme colors are updated when an autogenerated theme
// is set up.
IN_PROC_BROWSER_TEST_P(AvatarToolbarButtonProfileColorBrowserTest,
                       PRE_AutogeneratedTheme) {
  Profile* profile = browser()->profile();
  ProfileAttributesEntry* entry = GetProfileAttributesEntry(profile);
  SetDefaultTheme(profile);
  EXPECT_EQ(entry->GetProfileThemeColors(),
            ComputeProfileThemeColorsForBrowser());

  SetColorTheme(profile, SK_ColorGREEN);
  ProfileThemeColors theme_colors = entry->GetProfileThemeColors();
  EXPECT_EQ(theme_colors, ComputeProfileThemeColorsForBrowser());

  // Check that a switch to another autogenerated theme updates the colors.
  SetColorTheme(profile, SK_ColorMAGENTA);
  ProfileThemeColors theme_colors2 = entry->GetProfileThemeColors();
  EXPECT_NE(theme_colors, theme_colors2);
  EXPECT_NE(theme_colors2, GetDefaultProfileThemeColors());
  EXPECT_EQ(theme_colors2, ComputeProfileThemeColorsForBrowser());

  // Reset the cached colors to test that they're recreated on the next startup.
  entry->SetProfileThemeColors(std::nullopt);
  EXPECT_EQ(entry->GetProfileThemeColors(), GetDefaultProfileThemeColors());
}

// Tests that the profile theme colors are updated to reflect the autogenerated
// colors on startup.
IN_PROC_BROWSER_TEST_P(AvatarToolbarButtonProfileColorBrowserTest,
                       AutogeneratedTheme) {
  EXPECT_EQ(
      GetProfileAttributesEntry(browser()->profile())->GetProfileThemeColors(),
      ComputeProfileThemeColorsForBrowser());
}

// Tests that switching to the default theme updates profile colors.
IN_PROC_BROWSER_TEST_P(AvatarToolbarButtonProfileColorBrowserTest,
                       DefaultTheme) {
  Profile* profile = browser()->profile();
  ProfileAttributesEntry* entry = GetProfileAttributesEntry(profile);

  SetColorTheme(profile, SK_ColorGREEN);
  ProfileThemeColors theme_colors = entry->GetProfileThemeColors();
  EXPECT_EQ(theme_colors, ComputeProfileThemeColorsForBrowser());

  SetDefaultTheme(profile);
  ProfileThemeColors theme_colors2 = entry->GetProfileThemeColors();
  EXPECT_NE(theme_colors, theme_colors2);
  EXPECT_EQ(theme_colors2, ComputeProfileThemeColorsForBrowser());
}

// Tests that a theme is updated after opening a browser.
IN_PROC_BROWSER_TEST_P(AvatarToolbarButtonProfileColorBrowserTest,
                       UpdateThemeOnBrowserUpdate) {
  Profile* profile = browser()->profile();
  // Keeps the browser process and the profile alive while a browser window is
  // closed.
  ScopedKeepAlive keep_alive(KeepAliveOrigin::BROWSER,
                             KeepAliveRestartOption::DISABLED);
  ScopedProfileKeepAlive profile_keep_alive(
      profile, ProfileKeepAliveOrigin::kBackgroundMode);
  SetDefaultTheme(profile);
  ProfileAttributesEntry* entry = GetProfileAttributesEntry(profile);
  ProfileThemeColors theme_colors = entry->GetProfileThemeColors();
  CloseBrowserSynchronously(browser());

  SetColorTheme(profile, SK_ColorGREEN);
  // Colors haven't been changed yet because the profile has no active browsers.
  EXPECT_EQ(theme_colors, entry->GetProfileThemeColors());

  auto* target_browser = CreateBrowser(profile);
  ProfileThemeColors theme_colors2 = entry->GetProfileThemeColors();
  EXPECT_EQ(theme_colors2, ComputeProfileThemeColorsForBrowser(target_browser));
}

// Tests profile colors are updated when the browser's color scheme has changed.
IN_PROC_BROWSER_TEST_P(AvatarToolbarButtonProfileColorBrowserTest,
                       ProfileColorsUpdateOnColorSchemeChange) {
  theme_service(browser()->profile())
      ->SetBrowserColorScheme(ThemeService::BrowserColorScheme::kDark);
  Profile* profile = browser()->profile();
  ProfileAttributesEntry* entry = GetProfileAttributesEntry(profile);

  SetDefaultTheme(profile);
  ProfileThemeColors theme_colors = entry->GetProfileThemeColors();
  EXPECT_EQ(theme_colors, ComputeProfileThemeColorsForBrowser());

  theme_service(browser()->profile())
      ->SetBrowserColorScheme(ThemeService::BrowserColorScheme::kLight);
  ProfileThemeColors theme_colors2 = entry->GetProfileThemeColors();
  EXPECT_NE(theme_colors, theme_colors2);
  EXPECT_EQ(theme_colors2, ComputeProfileThemeColorsForBrowser());
}

INSTANTIATE_TEST_SUITE_P(,
                         AvatarToolbarButtonProfileColorBrowserTest,
                         testing::Values(ColorThemeType::kAutogeneratedTheme,
                                         ColorThemeType::kUserColor),
                         [](const auto& info) {
                           switch (info.param) {
                             case ColorThemeType::kAutogeneratedTheme:
                               return "AutogeneratedTheme";
                             case ColorThemeType::kUserColor:
                               return "UserColor";
                           }
                         });

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
class AvatarToolbarButtonEnterpriseBadgingBrowserTest
    : public AvatarToolbarButtonWithInteractiveFeaturePromoBrowserTest {
 public:
  AvatarToolbarButtonEnterpriseBadgingBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kEnterpriseProfileBadgingForAvatar}, {});
  }
  void SetUpInProcessBrowserTestFixture() override {
    provider_.SetDefaultReturns(
        true /* is_initialization_complete_return */,
        true /* is_first_policy_load_complete_return */);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
  }

  void SetUpOnMainThread() override {
    scoped_browser_management_ =
        std::make_unique<policy::ScopedManagementServiceOverrideForTesting>(
            policy::ManagementServiceFactory::GetForProfile(
                browser()->profile()),
            policy::EnterpriseManagementAuthority::CLOUD);
    AvatarToolbarButtonWithInteractiveFeaturePromoBrowserTest::
        SetUpOnMainThread();
  }

  void TearDownOnMainThread() override { scoped_browser_management_.reset(); }

 protected:
  testing::NiceMock<policy::MockConfigurationPolicyProvider> provider_;
  std::unique_ptr<policy::ScopedManagementServiceOverrideForTesting>
      scoped_browser_management_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonEnterpriseBadgingBrowserTest,
                       WorkProfileTextBadging) {
  AvatarToolbarButton* avatar_button = GetAvatarToolbarButton(browser());
  // Ensure enterprise badging can be shown.
  std::u16string work_label = u"Work";

  {
    enterprise_util::SetUserAcceptedAccountManagement(browser()->profile(),
                                                      true);
    EXPECT_EQ(avatar_button->GetText(), work_label);
    auto clear_closure = avatar_button->SetExplicitButtonState(
        u"Explicit text", /*accessibility_label=*/std::nullopt,
        /*explicit_action=*/std::nullopt);
    EXPECT_NE(avatar_button->GetText(), work_label);
    clear_closure.RunAndReset();
    EXPECT_EQ(avatar_button->GetText(), work_label);
    // The profile name should be the default profile name.
    std::u16string local_name =
        GetProfileAttributesEntry(browser()->profile())->GetLocalProfileName();
    EXPECT_TRUE(g_browser_process->profile_manager()
                    ->GetProfileAttributesStorage()
                    .IsDefaultProfileName(local_name, true));
  }

  {
    enterprise_util::SetUserAcceptedAccountManagement(browser()->profile(),
                                                      false);
    EXPECT_NE(avatar_button->GetText(), work_label);
    base::ScopedClosureRunner clear_closure =
        avatar_button->SetExplicitButtonState(
            u"Explicit text", /*accessibility_label=*/std::nullopt,
            /*explicit_action=*/std::nullopt);
    EXPECT_NE(avatar_button->GetText(), work_label);
    clear_closure.RunAndReset();
    EXPECT_NE(avatar_button->GetText(), work_label);
    EXPECT_EQ(GetProfileAttributesEntry(browser()->profile())
                  ->GetEnterpriseProfileLabel(),
              std::u16string());
    // The profile name should be the default profile name.
    std::u16string local_name =
        GetProfileAttributesEntry(browser()->profile())->GetLocalProfileName();
    EXPECT_TRUE(g_browser_process->profile_manager()
                    ->GetProfileAttributesStorage()
                    .IsDefaultProfileName(local_name, true));
  }
}

IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonEnterpriseBadgingBrowserTest,
                       DefaultBadgeUpdatedWithManagementChanges) {
  enterprise_util::SetUserAcceptedAccountManagement(browser()->profile(), true);
  AvatarToolbarButton* avatar_button = GetAvatarToolbarButton(browser());
  {
    policy::ScopedManagementServiceOverrideForTesting profile_management{
        policy::ManagementServiceFactory::GetForProfile(browser()->profile()),
        policy::EnterpriseManagementAuthority::CLOUD};
    policy::ManagementServiceFactory::GetForProfile(browser()->profile())
        ->TriggerPolicyStatusChangedForTesting();
    EXPECT_EQ(avatar_button->GetText(), u"Work");
  }
  {
    policy::ScopedManagementServiceOverrideForTesting profile_management{
        policy::ManagementServiceFactory::GetForProfile(browser()->profile()),
        policy::EnterpriseManagementAuthority::NONE};
    policy::ManagementServiceFactory::GetForProfile(browser()->profile())
        ->TriggerPolicyStatusChangedForTesting();
    EXPECT_EQ(avatar_button->GetText(), std::u16string());
  }
}

IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonEnterpriseBadgingBrowserTest,
                       DefaultBadgeDisabledbyPolicy) {
  std::u16string work_label = u"Work";
  AvatarToolbarButton* avatar_button = GetAvatarToolbarButton(browser());
  browser()->profile()->GetPrefs()->SetInteger(
      prefs::kEnterpriseProfileBadgeToolbarSettings, 1);

  enterprise_util::SetUserAcceptedAccountManagement(browser()->profile(), true);

  // There should be no text because the policy fully disables badging.
  EXPECT_EQ(avatar_button->GetText(), std::u16string());
  EXPECT_EQ(GetProfileAttributesEntry(browser()->profile())
                ->GetEnterpriseProfileLabel(),
            std::u16string());
  // The profile name should be the default profile name.
  std::u16string local_name =
      GetProfileAttributesEntry(browser()->profile())->GetLocalProfileName();
  EXPECT_TRUE(g_browser_process->profile_manager()
                  ->GetProfileAttributesStorage()
                  .IsDefaultProfileName(local_name, true));
}

IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonEnterpriseBadgingBrowserTest,
                       CustomBadgeDisabledbyPolicy) {
  browser()->profile()->GetPrefs()->SetString(
      prefs::kEnterpriseCustomLabelForProfile, "Custom Label");
  browser()->profile()->GetPrefs()->SetInteger(
      prefs::kEnterpriseProfileBadgeToolbarSettings, 1);

  AvatarToolbarButton* avatar_button = GetAvatarToolbarButton(browser());

  enterprise_util::SetUserAcceptedAccountManagement(browser()->profile(), true);

  // There should be no text because the policy fully disables badging.
  EXPECT_EQ(avatar_button->GetText(), std::u16string());
  EXPECT_EQ(GetProfileAttributesEntry(browser()->profile())
                ->GetEnterpriseProfileLabel(),
            std::u16string());
  // The profile name should be the default profile name.
  std::u16string local_name =
      GetProfileAttributesEntry(browser()->profile())->GetLocalProfileName();
  EXPECT_TRUE(g_browser_process->profile_manager()
                  ->GetProfileAttributesStorage()
                  .IsDefaultProfileName(local_name, true));
}

IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonEnterpriseBadgingBrowserTest,
                       CustomBadgeLengthLimited) {
  browser()->profile()->GetPrefs()->SetString(
      prefs::kEnterpriseCustomLabelForProfile,
      "Custom Label Can Be Max 16 Characters");

  AvatarToolbarButton* avatar_button = GetAvatarToolbarButton(browser());

  enterprise_util::SetUserAcceptedAccountManagement(browser()->profile(), true);
  // The text should be tuncated to 16 characters followed by "...".
  EXPECT_EQ(avatar_button->GetText(), u"Custom Label Can…");
  // The profile label will be handled by the individual UI components.
  EXPECT_EQ(GetProfileAttributesEntry(browser()->profile())
                ->GetEnterpriseProfileLabel(),
            u"Custom Label Can Be Max 16 Characters");
  EXPECT_EQ(
      GetProfileAttributesEntry(browser()->profile())->GetLocalProfileName(),
      u"Custom Label Can Be Max 16 Characters");
}

IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonEnterpriseBadgingBrowserTest,
                       WorkNewBrowserShowsBadgeWithCustomLabel) {
  browser()->profile()->GetPrefs()->SetString(
      prefs::kEnterpriseCustomLabelForProfile, "Custom Label");
  enterprise_util::SetUserAcceptedAccountManagement(browser()->profile(), true);

  Browser* second_browser = CreateBrowser(browser()->profile());
  AvatarToolbarButton* second_browser_avatar_button =
      GetAvatarToolbarButton(second_browser);
  EXPECT_EQ(second_browser_avatar_button->GetText(), u"Custom Label");

  browser()->profile()->GetPrefs()->SetString(
      prefs::kEnterpriseCustomLabelForProfile, "Updated Label");
  EXPECT_EQ(second_browser_avatar_button->GetText(), u"Updated Label");
}

IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonEnterpriseBadgingBrowserTest,
                       WorkNewBrowserShowsBadge) {
  std::u16string work_label = u"Work";
  enterprise_util::SetUserAcceptedAccountManagement(browser()->profile(), true);

  Browser* second_browser = CreateBrowser(browser()->profile());
  AvatarToolbarButton* second_browser_avatar_button =
      GetAvatarToolbarButton(second_browser);
  EXPECT_EQ(second_browser_avatar_button->GetText(), work_label);
}

// Sync Pause/Error has priority over WorkBadge.
IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonEnterpriseBadgingBrowserTest,
                       WorkBadgeAndSyncPaused) {
  AvatarToolbarButton* avatar_button = GetAvatarToolbarButton(browser());
  ASSERT_TRUE(avatar_button->GetText().empty());

  std::u16string work_label = u"Work";
  enterprise_util::SetUserAcceptedAccountManagement(browser()->profile(), true);
  EXPECT_EQ(avatar_button->GetText(), work_label);

  EnableSyncWithImageAndClearGreeting(avatar_button, u"work@managed.com");
  SimulateSyncPaused();
  // Sync Paused has priority over the Work badge.
  ExpectSyncPaused(avatar_button);

  ClearSyncPaused();
  // Non transient mode should permanently show the work badge by default.
  // TODO(b/324018028): This test result might change with the ongoing changes.
  // At the end, the exact behavior could be set again. To review.
  EXPECT_EQ(avatar_button->GetText(), work_label);
}

IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonEnterpriseBadgingBrowserTest,
                       DecliningManagementShouldRemoveWorkBadge) {
  AvatarToolbarButton* avatar_button = GetAvatarToolbarButton(browser());
  ASSERT_TRUE(avatar_button->GetText().empty());

  std::u16string work_label = u"Work";
  enterprise_util::SetUserAcceptedAccountManagement(browser()->profile(), true);
  EXPECT_EQ(avatar_button->GetText(), work_label);

  enterprise_util::SetUserAcceptedAccountManagement(browser()->profile(),
                                                    false);
  EXPECT_EQ(avatar_button->GetText(), std::u16string());
}

IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonEnterpriseBadgingBrowserTest,
                       GreetingNotShownWhenManagementAccepted) {
  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser());
  // Normal state.
  ASSERT_TRUE(avatar->GetText().empty());

  AccountInfo account_info = Signin(u"work@managed.com", u"TestName");
  enterprise_util::SetUserAcceptedAccountManagement(browser()->profile(), true);

  // The greeting would only show when the image is loaded. Set the image to
  // make sure we do not have a false positive later.
  AddSignedInImage(account_info.account_id);

  // We do not expect a greeting to be shown if user accepted management.
  EXPECT_EQ(avatar->GetText(), u"Work");
}

class AvatarToolbarButtonEnterpriseBadgingWithSyncPromoParamsBrowserTest
    : public base::test::WithFeatureOverride,
      public AvatarToolbarButtonEnterpriseBadgingBrowserTest {
 protected:
  AvatarToolbarButtonEnterpriseBadgingWithSyncPromoParamsBrowserTest()
      : WithFeatureOverride(switches::kEnableHistorySyncOptinExpansionPill) {}
};

// TODO(crbug.com/331746545): Check flaky test issue on windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_GreetingShownWhenManagementNotAccepted \
  DISABLED_GreetingShownWhenManagementNotAccepted
#else
#define MAYBE_GreetingShownWhenManagementNotAccepted \
  GreetingShownWhenManagementNotAccepted
#endif
// test makes sure the greeting is not shown when the management badge is shown
// in the profile avatar pill.
IN_PROC_BROWSER_TEST_P(
    AvatarToolbarButtonEnterpriseBadgingWithSyncPromoParamsBrowserTest,
    MAYBE_GreetingShownWhenManagementNotAccepted) {
  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser());
  // Normal state.
  ASSERT_TRUE(avatar->GetText().empty());

  std::u16string name(u"TestName");
  AccountInfo account_info = Signin(u"work@managed.com", name);
  enterprise_util::SetUserAcceptedAccountManagement(browser()->profile(),
                                                    false);

  // The button is in a waiting for image state, the name is not yet displayed.
  // At this point the user has not accepted management yet.
  EXPECT_EQ(avatar->GetText(), std::u16string());

  // The greeting will only show when the image is loaded.
  AddSignedInImage(account_info.account_id);

  // Since the user has not accepted management, the greeting will still be
  // shown.
  EXPECT_EQ(avatar->GetText(),
            l10n_util::GetStringFUTF16(IDS_AVATAR_BUTTON_GREETING, name));
  avatar->TriggerTimeoutForTesting(AvatarDelayType::kNameGreeting);
  if (IsParamFeatureEnabled()) {
    // The greeting is followed by the history sync opt-in.
    EXPECT_EQ(avatar->GetText(), l10n_util::GetStringUTF16(
                                     IDS_AVATAR_BUTTON_BROWSE_ACROSS_DEVICES));
    avatar->TriggerTimeoutForTesting(AvatarDelayType::kHistorySyncOptin);
  }
  // Once the name (or sync promo) is not shown anymore, we expect no text.
  EXPECT_EQ(avatar->GetText(), std::u16string());
}

// TODO(crbug.com/331746545): Check flaky test issue on windows.
// TODO(crbug.com/331746545): Check flaky test issue on windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_PRE_SignedInWithNewSessionKeepWorkBadge DISABLED_PRE_SignedInWithNewSessionKeepWorkBadge
#define MAYBE_SignedInWithNewSessionKeepWorkBadge DISABLED_SignedInWithNewSessionKeepWorkBadge
#else
#define MAYBE_PRE_SignedInWithNewSessionKeepWorkBadge PRE_SignedInWithNewSessionKeepWorkBadge
#define MAYBE_SignedInWithNewSessionKeepWorkBadge SignedInWithNewSessionKeepWorkBadge
#endif
// Tests the flow for a managed sign-in.
IN_PROC_BROWSER_TEST_P(
    AvatarToolbarButtonEnterpriseBadgingWithSyncPromoParamsBrowserTest,
    MAYBE_PRE_SignedInWithNewSessionKeepWorkBadge) {
  // Sign in.
  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser());
  std::u16string name(u"TestName");
  AccountInfo account_info = SigninWithImage(u"work@managed.com", name);

  // Since the user has not accepted management yet, the greeting will be
  // shown.
  EXPECT_EQ(avatar->GetText(),
            l10n_util::GetStringFUTF16(IDS_AVATAR_BUTTON_GREETING, name));
  avatar->TriggerTimeoutForTesting(AvatarDelayType::kNameGreeting);
  if (IsParamFeatureEnabled()) {
    // The greeting is followed by the history sync opt-in.
    EXPECT_EQ(avatar->GetText(), l10n_util::GetStringUTF16(
                                     IDS_AVATAR_BUTTON_BROWSE_ACROSS_DEVICES));
    avatar->TriggerTimeoutForTesting(AvatarDelayType::kHistorySyncOptin);
  }

  // Once the name (or sync promo) is not shown anymore, we expect no text since
  // management is not accepted.
  EXPECT_EQ(avatar->GetText(), std::u16string());

  // Management is usually accepted by the time the greeting is finished. The
  // work badgge should be shown once this happens.
  enterprise_util::SetUserAcceptedAccountManagement(browser()->profile(), true);
  browser()->profile()->GetPrefs()->SetString(
      prefs::kEnterpriseCustomLabelForProfile, "Custom Label");

  EXPECT_EQ(avatar->GetText(), u"Custom Label");
}

// Test that the work badge remains upon restart for a user that is managed.
// Note that we need to unset and reset UserAcceptedAccountManagement due to the
// management service override.
IN_PROC_BROWSER_TEST_P(
    AvatarToolbarButtonEnterpriseBadgingWithSyncPromoParamsBrowserTest,
    MAYBE_SignedInWithNewSessionKeepWorkBadge) {
  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser());
  // The greetings are shown due to the management service override (unaware of
  // the management acceptance after restart).
  EXPECT_EQ(avatar->GetText(), l10n_util::GetStringFUTF16(
                                   IDS_AVATAR_BUTTON_GREETING, u"TestName"));
  avatar->TriggerTimeoutForTesting(AvatarDelayType::kNameGreeting);
  if (IsParamFeatureEnabled()) {
    // The greeting is followed by the history sync opt-in.
    EXPECT_EQ(avatar->GetText(), l10n_util::GetStringUTF16(
                                     IDS_AVATAR_BUTTON_BROWSE_ACROSS_DEVICES));
    avatar->TriggerTimeoutForTesting(AvatarDelayType::kHistorySyncOptin);
  }

  enterprise_util::SetUserAcceptedAccountManagement(browser()->profile(),
                                                    false);
  enterprise_util::SetUserAcceptedAccountManagement(browser()->profile(), true);

  EXPECT_EQ(avatar->GetText(), u"Custom Label");
  EXPECT_EQ(GetProfileAttributesEntry(browser()->profile())
                ->GetEnterpriseProfileLabel(),
            u"Custom Label");
  EXPECT_EQ(
      GetProfileAttributesEntry(browser()->profile())->GetLocalProfileName(),
      u"Custom Label");
  // Previously added image on signin should still be shown in the new session.
  EXPECT_TRUE(IsSignedInImageUsed());
}

// TODO(crbug.com/331746545): Check the flaky test issue on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_SyncPromoShownOnInactivityWhenManagementAccepted \
  DISABLED_SyncPromoShownOnInactivityWhenManagementAccepted
#else
#define MAYBE_SyncPromoShownOnInactivityWhenManagementAccepted \
  SyncPromoShownOnInactivityWhenManagementAccepted
#endif
IN_PROC_BROWSER_TEST_P(
    AvatarToolbarButtonEnterpriseBadgingWithSyncPromoParamsBrowserTest,
    MAYBE_SyncPromoShownOnInactivityWhenManagementAccepted) {
  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser());
  // Normal state.
  ASSERT_TRUE(avatar->GetText().empty());

  AccountInfo account_info = Signin(u"work@managed.com", u"TestName");
  enterprise_util::SetUserAcceptedAccountManagement(browser()->profile(), true);

  // The greeting would only show when the image is loaded. Set the image to
  // make sure we do not have a false positive later.
  AddSignedInImage(account_info.account_id);
  // We do not expect a greeting to be shown if user accepted management.
  EXPECT_EQ(avatar->GetText(), u"Work");
  // Simulate long enough inactivity to trigger the sync promo.
  RunTestSequence(
      SetLastActive(user_education::features::GetIdleTimeBetweenSessions()));
  if (IsParamFeatureEnabled()) {
    EXPECT_EQ(avatar->GetText(), l10n_util::GetStringUTF16(
                                     IDS_AVATAR_BUTTON_BROWSE_ACROSS_DEVICES));
    avatar->TriggerTimeoutForTesting(AvatarDelayType::kHistorySyncOptin);
  }
  EXPECT_EQ(avatar->GetText(), u"Work");
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(
    AvatarToolbarButtonEnterpriseBadgingWithSyncPromoParamsBrowserTest);

// TODO(b/331746545): Check flaky test issue on windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_SigninPausedFromExternalErrorThenReauth \
  DISABLED_SigninPausedFromExternalErrorThenReauth
#else
#define MAYBE_SigninPausedFromExternalErrorThenReauth \
  SigninPausedFromExternalErrorThenReauth
#endif
IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonBrowserTest,
                       MAYBE_SigninPausedFromExternalErrorThenReauth) {
  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser());
  SigninWithImageAndClearGreetingAndSyncPromo(avatar, u"test@gmail.com");
  ASSERT_EQ(avatar->GetText(), std::u16string());

  // Browser opened before the error.
  Browser* opened_browser = CreateBrowser(browser()->profile());
  AvatarToolbarButton* opened_browser_avatar_button =
      GetAvatarToolbarButton(opened_browser);
  ASSERT_EQ(opened_browser_avatar_button->GetText(), std::u16string());

  SimulateSigninError(/*web_sign_out=*/false);
  EXPECT_EQ(avatar->GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SIGNIN_PAUSED));
  EXPECT_EQ(opened_browser_avatar_button->GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SIGNIN_PAUSED));

  // New browser opened after the error -- error should be shown directly.
  Browser* new_browser = CreateBrowser(browser()->profile());
  AvatarToolbarButton* new_browser_avatar_button =
      GetAvatarToolbarButton(new_browser);
  EXPECT_EQ(new_browser_avatar_button->GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SIGNIN_PAUSED));

  ClearSigninError();
  EXPECT_EQ(avatar->GetText(), std::u16string());
  EXPECT_EQ(opened_browser_avatar_button->GetText(), std::u16string());
  EXPECT_EQ(new_browser_avatar_button->GetText(), std::u16string());
}

// TODO(b/331746545): Check flaky test issue on windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_SigninPausedFromWebSignout DISABLED_SigninPausedFromWebSignout
#else
#define MAYBE_SigninPausedFromWebSignout SigninPausedFromWebSignout
#endif
IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonBrowserTest,
                       MAYBE_SigninPausedFromWebSignout) {
  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser());

  SigninWithImageAndClearGreetingAndSyncPromo(avatar, u"test@gmail.com");
  ASSERT_EQ(avatar->GetText(), std::u16string());

  // Browser opened before the error.
  Browser* opened_browser = CreateBrowser(browser()->profile());
  AvatarToolbarButton* opened_browser_avatar_button =
      GetAvatarToolbarButton(opened_browser);
  ASSERT_EQ(opened_browser_avatar_button->GetText(), std::u16string());

  SimulateSigninError(/*web_sign_out=*/true);
  // Text does not appear directly after a web sign out, a timer is started.
  EXPECT_EQ(avatar->GetText(), std::u16string());
  EXPECT_EQ(opened_browser_avatar_button->GetText(), std::u16string());

  // New browser opened after the error and before timer ends -- error is not
  // shown directly.
  Browser* new_browser = CreateBrowser(browser()->profile());
  AvatarToolbarButton* new_browser_avatar_button =
      GetAvatarToolbarButton(new_browser);
  EXPECT_EQ(new_browser_avatar_button->GetText(), std::u16string());

  // Simulate all the timer ends.
  avatar->TriggerTimeoutForTesting(AvatarDelayType::kSigninPendingText);
  opened_browser_avatar_button->TriggerTimeoutForTesting(
      AvatarDelayType::kSigninPendingText);
  new_browser_avatar_button->TriggerTimeoutForTesting(
      AvatarDelayType::kSigninPendingText);

  EXPECT_EQ(avatar->GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SIGNIN_PAUSED));
  EXPECT_EQ(opened_browser_avatar_button->GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SIGNIN_PAUSED));
  EXPECT_EQ(new_browser_avatar_button->GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SIGNIN_PAUSED));

  ClearSigninError();
  EXPECT_EQ(avatar->GetText(), std::u16string());
  EXPECT_EQ(opened_browser_avatar_button->GetText(), std::u16string());
  EXPECT_EQ(new_browser_avatar_button->GetText(), std::u16string());
}

// TODO(crbug.com/360106845): Fix flaky test and re-enable.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
#define MAYBE_SigninPausedFromWebSignoutThenRestartChrome \
  DISABLED_SigninPausedFromWebSignoutThenRestartChrome
#else
#define MAYBE_SigninPausedFromWebSignoutThenRestartChrome \
  SigninPausedFromWebSignoutThenRestartChrome
#endif
IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonBrowserTest,
                       MAYBE_SigninPausedFromWebSignoutThenRestartChrome) {
  // Needed because the current profile will be destroyed.
  ScopedKeepAlive keep_alive(KeepAliveOrigin::SESSION_RESTORE,
                             KeepAliveRestartOption::DISABLED);
  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser());
  const std::u16string name(u"new_profile_name");
  SigninWithImageAndClearGreetingAndSyncPromo(avatar, u"test@gmail.com", name);

  SimulateSigninError(/*web_sign_out=*/true);
  ASSERT_EQ(avatar->GetText(), std::u16string());

  ProfileDestructionWaiter destruction_waiter(browser()->profile());
  // Closing the browser will destroy profile from the memory.
  CloseAllBrowsers();
  destruction_waiter.Wait();

  // Load the profile again to open a new browser and check the button state.
  Profile* loaded_profile = ProfileLoader().LoadFirstAndOnlyProfile();
  Browser* new_browser = CreateBrowser(loaded_profile);
  AvatarToolbarButton* new_avatar = GetAvatarToolbarButton(new_browser);
  // The greetings are shown after the restart.
  EXPECT_EQ(new_avatar->GetText(),
            l10n_util::GetStringFUTF16(IDS_AVATAR_BUTTON_GREETING, name));
  new_avatar->TriggerTimeoutForTesting(AvatarDelayType::kNameGreeting);

  // The error text is expected to be shown even if the error delay has not
  // reached yet.
  EXPECT_EQ(new_avatar->GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SIGNIN_PAUSED));
}

// Regression test for https://crbug.com/348587566
IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonBrowserTest,
                       SigninPausedDelayEndedNoBrowser) {
  ASSERT_EQ(1u, chrome::GetTotalBrowserCount());
  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser());

  SigninWithImageAndClearGreetingAndSyncPromo(avatar, u"test@gmail.com",
                                              u"TestName");
  SimulateSigninError(/*web_sign_out=*/true);
  ASSERT_TRUE(avatar->GetText().empty());
  Profile* profile = browser()->profile();

  // Close the browser before the delay ends, but keep the profile and Chrome
  // alive by opening an incognito browser.
  CreateIncognitoBrowser(profile);
  CloseBrowserSynchronously(browser());

  // This simulates the delay expiry for the next browser. Instead of advancing
  // time, we set the expected delay to 0, making the elapsed time greater than
  // the delay for sure - simulating the delay expiry.
  SetZeroAvatarDelayForSigninPendingText();

  // Open a new browser, this should not crash.
  Browser* new_browser = CreateBrowser(profile);
  EXPECT_EQ(GetAvatarToolbarButton(new_browser)->GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SIGNIN_PAUSED));
}

// TODO(b/331746545): Check flaky test issue on windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_SigninPausedThenSignout DISABLED_SigninPausedThenSignout
#else
#define MAYBE_SigninPausedThenSignout SigninPausedThenSignout
#endif
IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonBrowserTest,
                       MAYBE_SigninPausedThenSignout) {
  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser());
  SigninWithImageAndClearGreetingAndSyncPromo(avatar, u"test@gmail.com");
  ASSERT_EQ(avatar->GetText(), std::u16string());

  SimulateSigninError(/*web_sign_out=*/false);

  EXPECT_EQ(avatar->GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SIGNIN_PAUSED));

  Signout();

  EXPECT_EQ(avatar->GetText(), std::u16string());
}

IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonBrowserTest, AccessibilityLabels) {
  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser());

  const std::u16string profile_name(u"new_profile_name");
  profiles::UpdateProfileName(browser()->profile(), profile_name);

  const views::ViewAccessibility& accessibility =
      avatar->GetViewAccessibility();

  EXPECT_EQ(accessibility.GetCachedName(), profile_name);
  EXPECT_EQ(accessibility.GetCachedDescription(), std::u16string());

  const std::u16string account_name(u"Test Name");
  SigninWithImageAndClearGreetingAndSyncPromo(avatar, u"test@gmail.com",
                                              account_name);

  const std::u16string expected_profile_name_with_account =
      account_name + u" (" + profile_name + u")";
  EXPECT_EQ(accessibility.GetCachedName(), expected_profile_name_with_account);
  EXPECT_EQ(accessibility.GetCachedDescription(), std::u16string());

  // Explicit text with accessibility text
  const std::u16string explicit_text(u"explicit_text");
  const std::u16string explicit_accessibility_text(u"explicit_text_acc");
  base::ScopedClosureRunner clear_explicit_text_callback =
      avatar->SetExplicitButtonState(explicit_text, explicit_accessibility_text,
                                     /*explicit_action=*/std::nullopt);

  EXPECT_EQ(accessibility.GetCachedName(), explicit_text);
  EXPECT_EQ(accessibility.GetCachedDescription(), explicit_accessibility_text);

  clear_explicit_text_callback.RunAndReset();

  EXPECT_EQ(accessibility.GetCachedName(), expected_profile_name_with_account);
  EXPECT_EQ(accessibility.GetCachedDescription(), std::u16string());

  // Explicit text without accessibility text
  base::ScopedClosureRunner clear_explicit_text_without_accessibility_callback =
      avatar->SetExplicitButtonState(explicit_text,
                                     /*accessibility_label=*/std::nullopt,
                                     /*explicit_action=*/std::nullopt);

  EXPECT_EQ(accessibility.GetCachedName(), explicit_text);
  EXPECT_EQ(accessibility.GetCachedDescription(),
            expected_profile_name_with_account);

  clear_explicit_text_without_accessibility_callback.RunAndReset();

  EXPECT_EQ(accessibility.GetCachedName(), expected_profile_name_with_account);
  EXPECT_EQ(accessibility.GetCachedDescription(), std::u16string());

  // This will trigger the immediate button content text change. Accessibility
  // text should adapt as well.
  SimulateSigninError(/*web_sign_out=*/false);

  EXPECT_EQ(accessibility.GetCachedName(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SIGNIN_PAUSED));
  EXPECT_EQ(accessibility.GetCachedDescription(),
            l10n_util::GetStringUTF16(
                IDS_AVATAR_BUTTON_SIGNIN_PENDING_ACCESSIBILITY_LABEL));

  ClearSigninError();

  EXPECT_EQ(accessibility.GetCachedName(), expected_profile_name_with_account);
  EXPECT_EQ(accessibility.GetCachedDescription(), std::u16string());

  // This will not trigger the immediate button content text change.
  // Accessibility text should adapt as well.
  SimulateSigninError(/*web_sign_out=*/true);

  EXPECT_EQ(accessibility.GetCachedName(),
            l10n_util::GetStringUTF16(
                IDS_AVATAR_BUTTON_SIGNIN_PENDING_ACCESSIBILITY_LABEL));
  EXPECT_EQ(accessibility.GetCachedDescription(),
            expected_profile_name_with_account);

  ClearSigninError();

  EXPECT_EQ(accessibility.GetCachedName(), expected_profile_name_with_account);
  EXPECT_EQ(accessibility.GetCachedDescription(), std::u16string());

  Signout();

  EXPECT_EQ(accessibility.GetCachedName(), profile_name);
  EXPECT_EQ(accessibility.GetCachedDescription(), std::u16string());
}

IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonBrowserTest,
                       PassphraseErrorSignedIn) {
  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser());
  SigninWithImageAndClearGreetingAndSyncPromo(avatar, u"test@gmail.com");
  ASSERT_EQ(avatar->GetText(), std::u16string());
  SimulatePassphraseError();
  EXPECT_EQ(avatar->GetText(), l10n_util::GetStringUTF16(
                                   IDS_SYNC_ERROR_USER_MENU_PASSPHRASE_BUTTON));
}

// TODO(crbug.com/359995696): Flaky on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_PassphraseErrorSyncing DISABLED_PassphraseErrorSyncing
#else
#define MAYBE_PassphraseErrorSyncing PassphraseErrorSyncing
#endif
IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonBrowserTest,
                       MAYBE_PassphraseErrorSyncing) {
  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser());
  EnableSyncWithImageAndClearGreeting(avatar, u"test@gmail.com");
  ASSERT_EQ(avatar->GetText(), std::u16string());
  SimulatePassphraseError();
  EXPECT_EQ(avatar->GetText(), l10n_util::GetStringUTF16(
                                   IDS_SYNC_ERROR_USER_MENU_PASSPHRASE_BUTTON));
}

// TODO(crbug.com/359995696): Flaky on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_UpgradeClientError DISABLED_UpgradeClientError
#else
#define MAYBE_UpgradeClientError UpgradeClientError
#endif
IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonBrowserTest,
                       MAYBE_UpgradeClientError) {
  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser());
  EnableSyncWithImageAndClearGreeting(avatar, u"test@gmail.com");
  ASSERT_EQ(avatar->GetText(), std::u16string());
  SimulateUpgradeClientError();
  EXPECT_EQ(avatar->GetText(),
            l10n_util::GetStringUTF16(IDS_SYNC_ERROR_USER_MENU_UPGRADE_BUTTON));
}

#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
