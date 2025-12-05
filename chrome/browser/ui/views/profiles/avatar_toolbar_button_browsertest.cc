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
#include "base/strings/string_number_conversions.h"
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
#include "chrome/browser/profiles/batch_upload/batch_upload_service_test_helper.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_promo_util.h"
#include "chrome/browser/signin/signin_ui_delegate.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/profiles/profile_colors_util.h"
#include "chrome/browser/ui/signin/dice_migration_service.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/profiles/profile_menu_coordinator.h"
#include "chrome/browser/ui/views/profiles/profile_menu_view_base.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/webauthn/passkey_unlock_manager.h"
#include "chrome/browser/webauthn/passkey_unlock_manager_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/profile_destruction_waiter.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/user_education/interactive_feature_promo_test.h"
#include "chrome/test/user_education/interactive_feature_promo_test_common.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/features.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/test/test_sync_service.h"
#include "components/user_education/common/user_education_features.h"
#include "components/user_education/views/help_bubble_view.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_launcher.h"
#include "content/public/test/test_utils.h"
#include "device/fido/features.h"
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

namespace {
using ::testing::StrictMock;
using ::testing::ValuesIn;
using ::testing::WithParamInterface;

ui::mojom::BrowserColorVariant kColorVariant =
    ui::mojom::BrowserColorVariant::kTonalSpot;

const gfx::Image kSignedInImage = gfx::test::CreateImage(20, 20, SK_ColorBLUE);
const char kSignedInImageUrl[] = "SIGNED_IN_IMAGE_URL";

constexpr std::string_view kTestPassphrase = "testpassphrase";

constexpr std::u16string_view kTestEmail = u"test@gmail.com";
std::u16string test_email() {
  return std::u16string(kTestEmail);
}
constexpr std::u16string_view kTestGivenName = u"TestName";
std::u16string test_given_name() {
  return std::u16string(kTestGivenName);
}
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
constexpr std::u16string_view kWorkBadge(u"Custom Label");
std::u16string work_badge() {
  return std::u16string(kWorkBadge);
}
#endif

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
  MOCK_METHOD(void,
              ShowHistorySyncOptinUI,
              (Profile*, const CoreAccountId&, signin_metrics::AccessPoint));
};

#if !BUILDFLAG(IS_CHROMEOS)
void Click(views::View* clickable_view) {
  clickable_view->OnMousePressed(
      ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
                     ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0));
  clickable_view->OnMouseReleased(
      ui::MouseEvent(ui::EventType::kMouseReleased, gfx::Point(), gfx::Point(),
                     ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0));
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

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
    // `AvatarToolbarButton::ClearActiveStateForTesting()`. This allows to
    // properly test the behavior pre/post delay without being time dependent.
    SetInfiniteAvatarDelay(AvatarDelayType::kNameGreeting);
    SetInfiniteAvatarDelay(AvatarDelayType::kOnSignin);
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
    SetInfiniteAvatarDelay(AvatarDelayType::kSigninPendingText);
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
  // behavior while the delay is happening. In order to clear the current state,
  // use `AvatarToolbarButton::ClearActiveStateForTesting()` at any point.
  void SetInfiniteAvatarDelay(AvatarDelayType delay_type) {
    delay_resets_.push_back(
        AvatarToolbarButton::CreateScopedInfiniteDelayOverrideForTesting(
            delay_type));
  }

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
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
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

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

    account_info = AccountInfo::Builder(account_info)
                       .SetGivenName(base::UTF16ToUTF8(name))
                       .SetFullName(base::UTF16ToUTF8(name))
                       .SetAvatarUrl("SOME_FAKE_URL")
                       .SetHostedDomain(std::string())
                       .SetLocale("en")
                       .Build();

    AccountCapabilitiesTestMutator(&account_info.capabilities)
        .set_is_subject_to_account_level_enterprise_policies(false);

    // Make sure account is valid so that all changes are persisted properly.
    CHECK(account_info.IsValid());

    signin::UpdateAccountInfoForAccount(GetIdentityManager(), account_info);

    GetTestSyncService()->SetSignedIn(consent_level, account_info);
    SetHistoryAndTabsSyncingPreference(/*enable_sync=*/false);

    return account_info;
  }

  // Signs in to Chrome with `email` and set the `name` to the account name.
  AccountInfo Signin(const std::u16string& email, const std::u16string& name) {
    AccountInfo account_info = MakePrimaryAccountAvailableWithName(
        signin::ConsentLevel::kSignin, email, name);

    // This simplifies the setup for tests that expect to show the SyncPromo.
    if (switches::IsAvatarSyncPromoFeatureEnabled()) {
      // Simulate setting enough time passing for the cookie change.
      GetBrowser()->GetProfile()->GetPrefs()->SetDouble(
          prefs::kGaiaCookieChangedTime,
          (base::Time::Now() -
           (switches::GetAvatarSyncPromoFeatureMinimumCookeAgeParam() +
            base::Minutes(1)))
              .InSecondsFSinceUnixEpoch());
    }

    return account_info;
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
    CHECK(!avatar->GetText().empty());
    avatar->ClearActiveStateForTesting();
    // Make sure the cross window animation replay is not triggered. This is
    // needed to clear the animation in all windows.
    delay_resets_.push_back(
        signin_ui_util::
            CreateZeroOverrideDelayForCrossWindowAnimationReplayForTesting());

    // Clears the sync optin promo if it is enabled. This is a no-op if the
    // promo is disabled. When `syncer::kReplaceSyncPromosWithSignInPromos` is
    // enabled, there is no promo after signing in.
    if (switches::IsAvatarSyncPromoFeatureEnabled() &&
        !base::FeatureList::IsEnabled(
            syncer::kReplaceSyncPromosWithSignInPromos)) {
      CHECK(!avatar->GetText().empty());
      avatar->ClearActiveStateForTesting();
    }

    return account_info;
  }

  void SetHistoryAndTabsSyncingPreference(bool enable_sync) {
    GetTestSyncService()->GetUserSettings()->SetSelectedType(
        syncer::UserSelectableType::kHistory, /*is_type_on=*/enable_sync);
    GetTestSyncService()->GetUserSettings()->SetSelectedType(
        syncer::UserSelectableType::kTabs, /*is_type_on=*/enable_sync);
    GetTestSyncService()->GetUserSettings()->SetSelectedType(
        syncer::UserSelectableType::kSavedTabGroups,
        /*is_type_on=*/enable_sync);
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

  void SimulateSigninPending(bool web_sign_out) {
    ASSERT_TRUE(
        GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));

    signin_metrics::SourceForRefreshTokenOperation token_operation_source =
        web_sign_out ? signin_metrics::SourceForRefreshTokenOperation::
                           kDiceResponseHandler_Signout
                     : signin_metrics::SourceForRefreshTokenOperation::kUnknown;

    signin::SetInvalidRefreshTokenForPrimaryAccount(GetIdentityManager(),
                                                    token_operation_source);
    ASSERT_TRUE(
        GetIdentityManager()->HasAccountWithRefreshTokenInPersistentErrorState(
            GetIdentityManager()->GetPrimaryAccountId(
                signin::ConsentLevel::kSignin)));
  }

  void ClearSigninPending() {
    ASSERT_TRUE(
        GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));

    signin::SetRefreshTokenForPrimaryAccount(GetIdentityManager());
    ASSERT_FALSE(
        GetIdentityManager()->HasAccountWithRefreshTokenInPersistentErrorState(
            GetIdentityManager()->GetPrimaryAccountId(
                signin::ConsentLevel::kSignin)));
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
    avatar->ClearActiveStateForTesting();
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
    signin::SetInvalidRefreshTokenForPrimaryAccount(
        GetIdentityManager(), signin_metrics::SourceForRefreshTokenOperation::
                                  kDiceResponseHandler_Signout);
    ASSERT_TRUE(
        GetIdentityManager()->HasAccountWithRefreshTokenInPersistentErrorState(
            GetIdentityManager()->GetPrimaryAccountId(
                signin::ConsentLevel::kSignin)));
  }

  void ClearSyncPaused() {
    ASSERT_TRUE(
        GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSync));

    // Clear Sync Paused introduced in `SimulateSyncPaused()`.
    GetTestSyncService()->ClearAuthError();
    GetTestSyncService()->FireStateChanged();
    signin::SetRefreshTokenForPrimaryAccount(GetIdentityManager());
    ASSERT_FALSE(
        GetIdentityManager()->HasAccountWithRefreshTokenInPersistentErrorState(
            GetIdentityManager()->GetPrimaryAccountId(
                signin::ConsentLevel::kSignin)));
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
    // Triggers Sync Error.
    GetTestSyncService()->SetTrustedVaultKeyRequired(true);
    GetTestSyncService()->FireStateChanged();
  }

  void ClearSyncError() {
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
    ASSERT_EQ(GetTestSyncService()->GetUserActionableError(),
              syncer::SyncService::UserActionableError::kNeedsClientUpgrade);
  }

  void ClearUpgradeClientError() {
    syncer::SyncStatus sync_status;
    GetTestSyncService()->SetDetailedSyncStatus(true, sync_status);
    GetTestSyncService()->FireStateChanged();
    ASSERT_NE(GetTestSyncService()->GetUserActionableError(),
              syncer::SyncService::UserActionableError::kNeedsClientUpgrade);
  }

  void SetSyncServiceTransportState(
      syncer::SyncService::TransportState transport_state) {
    GetTestSyncService()->SetMaxTransportState(transport_state);
    GetTestSyncService()->FireStateChanged();
  }

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  BatchUploadServiceTestHelper& batch_upload_test_helper() {
    return batch_upload_test_helper_;
  }
#endif

 private:
  void SetTestingFactories(content::BrowserContext* context) {
    SyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        context, base::BindRepeating(&TestingSyncFactoryFunction));

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
    batch_upload_test_helper_.SetupBatchUploadTestingFactoryInProfile(
        Profile::FromBrowserContext(context));
#endif
  }

  syncer::TestSyncService* GetTestSyncService() {
    return static_cast<syncer::TestSyncService*>(
        SyncServiceFactory::GetForProfile(GetBrowser()->profile()));
  }

  base::CallbackListSubscription dependency_manager_subscription_;
  std::vector<base::AutoReset<std::optional<base::TimeDelta>>> delay_resets_;

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  BatchUploadServiceTestHelper batch_upload_test_helper_;
#endif
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

// Test suite to mark test with Sync enabled (simulating Sync granted already -
// Enabling sync and only after clearing states performing checks) that have a
// Signin equivalent test. This test suite can be directly removed when removing
// `signin::ConsentLevel::kSync`.
using AvatarToolbarButtonWithSyncBrowserTest = AvatarToolbarButtonBrowserTest;

// TODO(crbug.com/438165525): During cleanup of
// `syncer::kReplaceSyncPromosWithSignInPromos`, remove all tests from this test
// suite.
class AvatarToolbarButtonReplaceSyncPromosWithSignInPromosOffBrowserTest
    : public AvatarToolbarButtonBrowserTest {
 public:
  AvatarToolbarButtonReplaceSyncPromosWithSignInPromosOffBrowserTest() {
    scoped_feature_list_.InitAndDisableFeature(
        syncer::kReplaceSyncPromosWithSignInPromos);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// TODO(crbug.com/438165525): During cleanup of
// `syncer::kReplaceSyncPromosWithSignInPromos`, move all tests to
// `AvatarToolbarButtonBrowserTest`.
class AvatarToolbarButtonReplaceSyncPromosWithSignInPromosBrowserTest
    : public AvatarToolbarButtonBrowserTest {
  base::test::ScopedFeatureList scoped_feature_list_{
      syncer::kReplaceSyncPromosWithSignInPromos};
};

// Macro that simplifies starting a test when already signed in.
// Internally signs the testing profile in the `PRE_` part of the test.
// The regular test is to be defined as a regular test, the only advantage is
// that the profile will already be signed in.
// This allows some specific scenarios to be easily tested; e.g. showing the
// greeting.
// Note: this does not scale accordingly with `MAYBE_*` tests; a simple (not
// ideal) solution is to just not compile the test at all for the expected
// DISABLED platform/buildflag. Also does not scale well with tests that expect
// `PRE_` in their main test definition.
#define TEST_WITH_SIGNED_IN_FROM_PRE(test_type, test_suite, test_name)       \
  test_type(test_suite, PRE_##test_name) {                                   \
    AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser());         \
    ASSERT_TRUE(avatar->GetText().empty());                                  \
                                                                             \
    SigninWithImage(test_email(), test_given_name());                        \
    if (base::FeatureList::IsEnabled(                                        \
            syncer::kReplaceSyncPromosWithSignInPromos)) {                   \
      ASSERT_EQ(                                                             \
          avatar->GetText(),                                                 \
          l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_MAKING_CHROME_YOURS)); \
      avatar->ClearActiveStateForTesting();                                  \
      EXPECT_TRUE(avatar->GetText().empty());                                \
    } else {                                                                 \
      ASSERT_EQ(avatar->GetText(),                                           \
                l10n_util::GetStringFUTF16(IDS_AVATAR_BUTTON_GREETING,       \
                                           test_given_name()));              \
      /* Explicitly do not clear the state not to activate any promo to   */ \
      /* avoid affecting the rate limiting mechanism that some tests rely */ \
      /* on.                                                              */ \
    }                                                                        \
  }                                                                          \
                                                                             \
  test_type(test_suite, test_name)  // Actual test implementation starts here.

// TODO(b/331746545): Check flaky test issue on windows.
#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_WIN)
TEST_WITH_SIGNED_IN_FROM_PRE(
    IN_PROC_BROWSER_TEST_F,
    AvatarToolbarButtonReplaceSyncPromosWithSignInPromosBrowserTest,
    ShowNameOnSignin) {
  ASSERT_TRUE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  SetHistoryAndTabsSyncingPreference(/*enable_sync=*/false);

  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser());
  ASSERT_EQ(avatar->GetText(),
            l10n_util::GetStringFUTF16(IDS_AVATAR_BUTTON_GREETING,
                                       test_given_name()));
  avatar->ClearActiveStateForTesting();

  ASSERT_EQ(avatar->GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SYNC_HISTORY));
  avatar->ClearActiveStateForTesting();

  // Once the greeting and promo are not shown anymore, we expect no text.
  EXPECT_EQ(avatar->GetText(), std::u16string());
}
#endif  // !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_WIN)

// TODO(b/331746545): Check flaky test issue on windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_ShowNameOnSync DISABLED_ShowNameOnSync
#else
#define MAYBE_ShowNameOnSync ShowNameOnSync
#endif
IN_PROC_BROWSER_TEST_F(
    AvatarToolbarButtonReplaceSyncPromosWithSignInPromosOffBrowserTest,
    MAYBE_ShowNameOnSync) {
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

  avatar->ClearActiveStateForTesting();
  // Once the name is not shown anymore, we expect no text.
  EXPECT_EQ(avatar->GetText(), std::u16string());
}

// Check www.crbug.com/331499330: This test makes sure that no states attempt to
// request an update during their construction. But rather do so after all the
// states are created and the view is added to the Widget.
TEST_WITH_SIGNED_IN_FROM_PRE(
    IN_PROC_BROWSER_TEST_F,
    AvatarToolbarButtonReplaceSyncPromosWithSignInPromosBrowserTest,
    OpenNewBrowserWhileNameIsShown) {
  ASSERT_TRUE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));

  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser());
  EXPECT_EQ(avatar->GetText(),
            l10n_util::GetStringFUTF16(IDS_AVATAR_BUTTON_GREETING,
                                       test_given_name()));

  // Creating a new browser while the refresh tokens are already loaded and the
  // name showing should not break/crash.
  Browser* new_browser = CreateBrowser(browser()->profile());
  AvatarToolbarButton* new_avatar_button = GetAvatarToolbarButton(new_browser);
  // Name is expected to be shown while it is still shown on the first browser.
  ASSERT_EQ(avatar->GetText(),
            l10n_util::GetStringFUTF16(IDS_AVATAR_BUTTON_GREETING,
                                       test_given_name()));
  EXPECT_EQ(new_avatar_button->GetText(),
            l10n_util::GetStringFUTF16(IDS_AVATAR_BUTTON_GREETING,
                                       test_given_name()));
}

IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonWithSyncBrowserTest, SyncPaused) {
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
IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonWithSyncBrowserTest,
                       SyncPausedWithPassphraseError) {
  AvatarToolbarButton* avatar_button = GetAvatarToolbarButton(browser());
  ASSERT_TRUE(avatar_button->GetText().empty());

  AccountInfo account_info =
      EnableSyncWithImageAndClearGreeting(avatar_button, u"test@gmail.com");
  SimulatePassphraseError();
  SimulateSyncPaused();
  ExpectSyncPaused(avatar_button);
}

#if !BUILDFLAG(IS_CHROMEOS)
// Checks that "Signin pending" has higher priority than passphrase errors.
// Adapted regression test for https://crbug.com/368997513
IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonBrowserTest,
                       SigninPendingWithPassphraseError) {
  AvatarToolbarButton* avatar_button = GetAvatarToolbarButton(browser());
  ASSERT_TRUE(avatar_button->GetText().empty());

  AccountInfo account_info = SigninWithImageAndClearGreetingAndSyncPromo(
      avatar_button, u"test@gmail.com");
  SimulatePassphraseError();
  SimulateSigninPending(/*web_sign_out=*/false);
  EXPECT_EQ(avatar_button->GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SIGNIN_PAUSED));
}
#endif

IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonWithSyncBrowserTest, SyncError) {
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

IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonWithSyncBrowserTest,
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
IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonWithSyncBrowserTest,
                       ExplicitTextThenSyncPaused) {
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
  ASSERT_FALSE(avatar->HasExplicitButtonState());

  const std::u16string text_1(u"Some New Text 1");
  base::MockCallback<base::RepeatingCallback<void(bool)>> mock_callback_1;
  base::ScopedClosureRunner reset_callback_1 = avatar->SetExplicitButtonState(
      text_1, /*accessibility_label=*/std::nullopt, mock_callback_1.Get());
  EXPECT_EQ(avatar->GetText(), text_1);
  EXPECT_TRUE(avatar->HasExplicitButtonState());
  EXPECT_CALL(mock_callback_1, Run).Times(1);
  avatar->ButtonPressed();

  const std::u16string text_2(u"Some New Text 2");
  base::MockCallback<base::RepeatingCallback<void(bool)>> mock_callback_2;
  base::ScopedClosureRunner reset_callback_2 = avatar->SetExplicitButtonState(
      text_2, /*accessibility_label=*/std::nullopt, mock_callback_2.Get());
  EXPECT_EQ(avatar->GetText(), text_2);
  EXPECT_TRUE(avatar->HasExplicitButtonState());
  EXPECT_CALL(mock_callback_2, Run).Times(1);
  avatar->ButtonPressed();

  // Calling the first reset callback should do nothing after the second call
  // to `SetExplicitButtonState`.
  reset_callback_1.RunAndReset();
  EXPECT_EQ(avatar->GetText(), text_2);
  EXPECT_TRUE(avatar->HasExplicitButtonState());

  // Calling the second reset callback should reset the text and the action.
  reset_callback_2.RunAndReset();
  EXPECT_EQ(avatar->GetText(), std::u16string());
  EXPECT_FALSE(avatar->HasExplicitButtonState());
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

  avatar->ClearActiveStateForTesting();

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

#if BUILDFLAG(ENABLE_DICE_SUPPORT)

IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonBrowserTest, SigninWithSyncError) {
  AvatarToolbarButton* avatar_button = GetAvatarToolbarButton(browser());
  // Normal state.
  ASSERT_TRUE(avatar_button->GetText().empty());

  SigninWithImageAndClearGreetingAndSyncPromo(avatar_button, u"test@gmail.com");
  SimulateSyncError();
  EXPECT_EQ(avatar_button->GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SIGNIN_PAUSED));

  ClearSyncError();
  EXPECT_EQ(avatar_button->GetText(), std::u16string());
}

IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonBrowserTest,
                       SigninPendingThenExplicitText) {
  AvatarToolbarButton* avatar_button = GetAvatarToolbarButton(browser());
  // Normal state.
  ASSERT_TRUE(avatar_button->GetText().empty());

  SigninWithImageAndClearGreetingAndSyncPromo(avatar_button, u"test@gmail.com");
  SimulateSigninPending(/*web_sign_out=*/false);
  ASSERT_EQ(avatar_button->GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SIGNIN_PAUSED));

  const std::u16string profile_switch_text(u"Profile Switch?");
  base::ScopedClosureRunner hide_callback =
      avatar_button->SetExplicitButtonState(
          profile_switch_text, /*accessibility_label=*/std::nullopt,
          /*explicit_action=*/std::nullopt);
  ASSERT_EQ(avatar_button->GetText(), profile_switch_text);

  // Clearing explicit text should go back to Signin Pending.
  hide_callback.RunAndReset();
  EXPECT_EQ(avatar_button->GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SIGNIN_PAUSED));
}

// Explicit text over signin pending.
IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonBrowserTest,
                       ExplicitTextThenSigninPending) {
  AvatarToolbarButton* avatar_button = GetAvatarToolbarButton(browser());
  // Normal state.
  ASSERT_TRUE(avatar_button->GetText().empty());

  SigninWithImageAndClearGreetingAndSyncPromo(avatar_button, u"test@gmail.com");
  const std::u16string profile_switch_text(u"Profile Switch?");
  base::ScopedClosureRunner hide_callback =
      avatar_button->SetExplicitButtonState(
          profile_switch_text, /*accessibility_label=*/std::nullopt,
          /*explicit_action=*/std::nullopt);
  ASSERT_EQ(avatar_button->GetText(), profile_switch_text);

  SimulateSigninPending(/*web_sign_out=*/false);
  // Explicit text should still be shown even if Signin Pending.
  ASSERT_EQ(avatar_button->GetText(), profile_switch_text);

  // Clearing explicit text should go back to Signin Pending.
  hide_callback.RunAndReset();
  EXPECT_EQ(avatar_button->GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SIGNIN_PAUSED));
}

IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonBrowserTest,
                       PRE_SigninPendingFromWebSignoutThenRestartChrome) {
  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser());
  SigninWithImageAndClearGreetingAndSyncPromo(avatar, test_email(),
                                              test_given_name());

  SimulateSigninPending(/*web_sign_out=*/true);
  ASSERT_EQ(avatar->GetText(), std::u16string());
}

IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonBrowserTest,
                       SigninPendingFromWebSignoutThenRestartChrome) {
  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser());
  // The greetings are shown after the restart.
  EXPECT_EQ(avatar->GetText(),
            l10n_util::GetStringFUTF16(IDS_AVATAR_BUTTON_GREETING,
                                       test_given_name()));

  avatar->ClearActiveStateForTesting();
  // The error text is expected to be shown even if the error delay has not
  // reached yet.
  EXPECT_EQ(avatar->GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SIGNIN_PAUSED));
}

// Regression test for https://crbug.com/348587566
IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonBrowserTest,
                       SigninPendingDelayEndedNoBrowser) {
  ASSERT_EQ(1u, chrome::GetTotalBrowserCount());
  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser());

  SigninWithImageAndClearGreetingAndSyncPromo(avatar, u"test@gmail.com",
                                              u"TestName");
  SimulateSigninPending(/*web_sign_out=*/true);
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

#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

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
// TODO(crbug.com/465718425): Support more promos; replace with
// `signin::ProfileMenuAvatarButtonPromoInfo::Type` values directly.
enum FeaturePromoType {
  // Enables `syncer::kReplaceSyncPromosWithSignInPromos` feature.
  kHistorySyncPromo,
  // Enables `switches::kAvatarButtonSyncPromoForTesting` feature, which is
  // checked through `switches::IsAvatarSyncPromoFeatureEnabled()`.
  kSyncPromo,
};

signin::ProfileMenuAvatarButtonPromoInfo::Type GetAvatarPromoType(
    FeaturePromoType param_promo_type) {
  switch (param_promo_type) {
    case FeaturePromoType::kHistorySyncPromo:
      return signin::ProfileMenuAvatarButtonPromoInfo::Type::kHistorySyncPromo;
    case FeaturePromoType::kSyncPromo:
      return signin::ProfileMenuAvatarButtonPromoInfo::Type::kSyncPromo;
  }
}

// The tests relying on this base class can test both the History Sync Promo and
// the Sync Promo. It is important to ensure that one of the feature flags is
// enabled at the same time, since the features are not compatible (SyncPromo
// have a higher priority than HistorySync in `HistorySyncOptinStateProvider`).
class AvatarToolbarButtonSyncPromoBaseBrowserTest
    : public AvatarToolbarButtonWithInteractiveFeaturePromoBrowserTest {
 protected:
  explicit AvatarToolbarButtonSyncPromoBaseBrowserTest(
      FeaturePromoType promo_type) {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;
    switch (promo_type) {
      case FeaturePromoType::kHistorySyncPromo:
        enabled_features.push_back(syncer::kReplaceSyncPromosWithSignInPromos);
        disabled_features.push_back(switches::kAvatarButtonSyncPromoForTesting);
        break;
      case FeaturePromoType::kSyncPromo:
        enabled_features.push_back(switches::kAvatarButtonSyncPromoForTesting);
        // `enable_replace_sync_with_signin` is ignored.
        disabled_features.push_back(syncer::kReplaceSyncPromosWithSignInPromos);
        break;
    }

    feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  void SetUpOnMainThread() override {
    AvatarToolbarButtonWithInteractiveFeaturePromoBrowserTest::
        SetUpOnMainThread();
    // Disable the preferences related to History sync to allow History sync
    // promos. Those are enabled by default in the `syncer::TestSyncService`.
    SetHistoryAndTabsSyncingPreference(/*enable_sync=*/false);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// TODO(crbug.com/331746545): Check the flaky test suite issue on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_AvatarToolbarButtonSyncPromoBrowserTest \
  DISABLED_AvatarToolbarButtonSyncPromoBrowserTest
#else
#define MAYBE_AvatarToolbarButtonSyncPromoBrowserTest \
  AvatarToolbarButtonSyncPromoBrowserTest
#endif
class MAYBE_AvatarToolbarButtonSyncPromoBrowserTest
    : public AvatarToolbarButtonSyncPromoBaseBrowserTest,
      public testing::WithParamInterface<FeaturePromoType> {
 protected:
  MAYBE_AvatarToolbarButtonSyncPromoBrowserTest()
      : AvatarToolbarButtonSyncPromoBaseBrowserTest(GetParam()) {}

  std::u16string GetExpectedPromoText() {
    if (switches::IsAvatarSyncPromoFeatureEnabled()) {
      return l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SYNC_PROMO);
    }
    return l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SYNC_HISTORY);
  }
};

IN_PROC_BROWSER_TEST_P(MAYBE_AvatarToolbarButtonSyncPromoBrowserTest,
                       PRE_HistorySyncOptinNotShownIfGreetingNotShown) {
  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser());
  // Explicitly sign in without an image.
  Signin(test_email(), test_given_name());
  switch (GetParam()) {
    case FeaturePromoType::kHistorySyncPromo:
      EXPECT_EQ(avatar->GetText(), l10n_util ::GetStringUTF16(
                                       IDS_AVATAR_BUTTON_MAKING_CHROME_YOURS));
      break;
    case FeaturePromoType::kSyncPromo:
      EXPECT_EQ(avatar->GetText(), std::u16string());
      break;
  }
}

IN_PROC_BROWSER_TEST_P(MAYBE_AvatarToolbarButtonSyncPromoBrowserTest,
                       HistorySyncOptinNotShownIfGreetingNotShown) {
  ASSERT_TRUE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));

  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser());
  // The button is in a waiting for image state, the greeting is not yet
  // displayed, hence the history sync opt-in should not be shown.
  EXPECT_EQ(avatar->GetText(), std::u16string());

  // Only after adding the image that the greeting is shown.
  AddSignedInImage(
      GetIdentityManager()->GetPrimaryAccountId(signin::ConsentLevel::kSignin));
  ASSERT_EQ(avatar->GetText(),
            l10n_util::GetStringFUTF16(IDS_AVATAR_BUTTON_GREETING,
                                       test_given_name()));
  avatar->ClearActiveStateForTesting();

  // Then the promo.
  ASSERT_EQ(avatar->GetText(), GetExpectedPromoText());
  avatar->ClearActiveStateForTesting();

  // Then normal state.
  EXPECT_EQ(avatar->GetText(), std::u16string());
}

// TODO(crbug.com/407964657): Merge this test with
// AvatarToolbarButtonBrowserTest.SyncError once the feature is enabled by
// default.
TEST_WITH_SIGNED_IN_FROM_PRE(IN_PROC_BROWSER_TEST_P,
                             MAYBE_AvatarToolbarButtonSyncPromoBrowserTest,
                             HistorySyncOptinNotShownWhenSyncEnabled) {
  SetHistoryAndTabsSyncingPreference(/*enable_sync=*/true);
  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser());
  ASSERT_EQ(avatar->GetText(),
            l10n_util::GetStringFUTF16(IDS_AVATAR_BUTTON_GREETING,
                                       test_given_name()));
  avatar->ClearActiveStateForTesting();
  switch (GetParam()) {
    case FeaturePromoType::kHistorySyncPromo:
      // The greeting should NOT be followed by the history sync opt-in entry
      // point if sync is already enabled.
      EXPECT_TRUE(avatar->GetText().empty());
      break;
    case FeaturePromoType::kSyncPromo:
      EXPECT_EQ(avatar->GetText(), GetExpectedPromoText());
      break;
  }
  SimulateSyncError();
  // The sync error should be shown.
  EXPECT_EQ(avatar->GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SIGNIN_PAUSED));
  ClearSyncError();
  // After clearing the sync error, the history sync opt-in entry point should
  // NOT be shown.
  EXPECT_TRUE(avatar->GetText().empty());
}

#if !BUILDFLAG(IS_LINUX)
TEST_WITH_SIGNED_IN_FROM_PRE(IN_PROC_BROWSER_TEST_P,
                             MAYBE_AvatarToolbarButtonSyncPromoBrowserTest,
                             HistorySyncOptinNotShownWhenPromotionsDisabled) {
  TestingBrowserProcess::GetGlobal()->local_state()->SetBoolean(
      prefs::kPromotionsEnabled, false);
  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser());
  ASSERT_EQ(avatar->GetText(),
            l10n_util::GetStringFUTF16(IDS_AVATAR_BUTTON_GREETING,
                                       test_given_name()));
  avatar->ClearActiveStateForTesting();
  // The greeting should NOT be followed by the history sync opt-in entry point
  // if promotions are disabled.
  EXPECT_TRUE(avatar->GetText().empty());
}
#endif

TEST_WITH_SIGNED_IN_FROM_PRE(IN_PROC_BROWSER_TEST_P,
                             MAYBE_AvatarToolbarButtonSyncPromoBrowserTest,
                             HistorySyncOptinNotShownWhenSyncNotAllowed) {
  SimulateDisableSyncByPolicyWithError();
  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser());
  // Normal state.
  ASSERT_EQ(avatar->GetText(),
            l10n_util::GetStringFUTF16(IDS_AVATAR_BUTTON_GREETING,
                                       test_given_name()));
  avatar->ClearActiveStateForTesting();
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

// TODO(crbug.com/331746545): Check the flaky test issue on Windows.
#if !BUILDFLAG(IS_WIN)
class AvatarToolbarButtonHistorySyncOptinManagedTypeTest
    : public AvatarToolbarButtonWithInteractiveFeaturePromoBrowserTest,
      public WithParamInterface<HistorySyncOptinSyncManagedTypeTestCase> {
 private:
  base::test::ScopedFeatureList feature_list_{
      syncer::kReplaceSyncPromosWithSignInPromos};
};

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

TEST_WITH_SIGNED_IN_FROM_PRE(IN_PROC_BROWSER_TEST_P,
                             AvatarToolbarButtonHistorySyncOptinManagedTypeTest,
                             HistorySyncOptinNotShownWhenSyncManaged) {
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
  ASSERT_EQ(avatar->GetText(),
            l10n_util::GetStringFUTF16(IDS_AVATAR_BUTTON_GREETING,
                                       test_given_name()));
  avatar->ClearActiveStateForTesting();
  // The greeting should NOT be followed by the history sync opt-in entry point
  // if sync is not allowed.
  EXPECT_TRUE(avatar->GetText().empty());
}

INSTANTIATE_TEST_SUITE_P(HistorySyncOptinManagedType,
                         AvatarToolbarButtonHistorySyncOptinManagedTypeTest,
                         ValuesIn(kHistorySyncOptinSyncManagedTypeTestCases));
#endif

TEST_WITH_SIGNED_IN_FROM_PRE(IN_PROC_BROWSER_TEST_P,
                             MAYBE_AvatarToolbarButtonSyncPromoBrowserTest,
                             HistorySyncOptinThenPassphraseError) {
  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser());
  ASSERT_EQ(avatar->GetText(),
            l10n_util::GetStringFUTF16(IDS_AVATAR_BUTTON_GREETING,
                                       test_given_name()));
  avatar->ClearActiveStateForTesting();
  // The greeting should be followed by the history sync opt-in entry point.
  EXPECT_EQ(avatar->GetText(), GetExpectedPromoText());
  SimulatePassphraseError();
  // The history sync opt-in entry point should be replaced by the passphrase
  // error message.
  EXPECT_EQ(avatar->GetText(),
            l10n_util::GetStringUTF16(
                IDS_SYNC_STATUS_NEEDS_PASSWORD_BUTTON_MAYBE_TITLE_CASE));
  ClearPassphraseError();
  // After clearing the passphrase error, the history sync opt-in entry point
  // should NOT be shown.
  EXPECT_TRUE(avatar->GetText().empty());
}

TEST_WITH_SIGNED_IN_FROM_PRE(IN_PROC_BROWSER_TEST_P,
                             MAYBE_AvatarToolbarButtonSyncPromoBrowserTest,
                             HistorySyncOptinThenClientUpgradeError) {
  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser());
  ASSERT_EQ(avatar->GetText(),
            l10n_util::GetStringFUTF16(IDS_AVATAR_BUTTON_GREETING,
                                       test_given_name()));
  avatar->ClearActiveStateForTesting();
  // The greeting should be followed by the history sync opt-in entry point.
  EXPECT_EQ(avatar->GetText(), GetExpectedPromoText());
  SimulateUpgradeClientError();
  // The history sync opt-in entry point should be replaced by the passphrase
  // error message.
  EXPECT_EQ(avatar->GetText(),
            l10n_util::GetStringUTF16(IDS_SYNC_UPGRADE_CLIENT_BUTTON));
  ClearUpgradeClientError();
  // After clearing the passphrase error, the history sync opt-in entry point
  // should NOT be shown.
  EXPECT_TRUE(avatar->GetText().empty());
}

TEST_WITH_SIGNED_IN_FROM_PRE(IN_PROC_BROWSER_TEST_P,
                             MAYBE_AvatarToolbarButtonSyncPromoBrowserTest,
                             HistorySyncOptinThenSigninPending) {
  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser());
  ASSERT_EQ(avatar->GetText(),
            l10n_util::GetStringFUTF16(IDS_AVATAR_BUTTON_GREETING,
                                       test_given_name()));
  avatar->ClearActiveStateForTesting();
  // The greeting should be followed by the history sync opt-in entry point.
  EXPECT_EQ(avatar->GetText(), GetExpectedPromoText());
  SimulateSigninPending(/*web_sign_out=*/false);
  // The history sync opt-in entry point should be replaced by the signin
  // pending message.
  EXPECT_EQ(avatar->GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SIGNIN_PAUSED));
  ClearSigninPending();
  // After clearing the sign in error, the history sync opt-in entry point
  // should NOT be shown.
  EXPECT_TRUE(avatar->GetText().empty());
}

TEST_WITH_SIGNED_IN_FROM_PRE(IN_PROC_BROWSER_TEST_P,
                             MAYBE_AvatarToolbarButtonSyncPromoBrowserTest,
                             HistorySyncOptinThenExplicitText) {
  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser());
  ASSERT_EQ(avatar->GetText(),
            l10n_util::GetStringFUTF16(IDS_AVATAR_BUTTON_GREETING,
                                       test_given_name()));
  avatar->ClearActiveStateForTesting();
  // The greeting should be followed by the history sync opt-in entry point.
  EXPECT_EQ(avatar->GetText(), GetExpectedPromoText());
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

TEST_WITH_SIGNED_IN_FROM_PRE(
    IN_PROC_BROWSER_TEST_P,
    MAYBE_AvatarToolbarButtonSyncPromoBrowserTest,
    HistorySyncOptinNotShownIfErrorBeforeGreetingTimesOut) {
  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser());
  ASSERT_EQ(avatar->GetText(),
            l10n_util::GetStringFUTF16(IDS_AVATAR_BUTTON_GREETING,
                                       test_given_name()));
  SimulatePassphraseError();
  avatar->ClearActiveStateForTesting();
  // No history sync opt-in entry point should be shown if the error is shown
  // before the greeting times out.
  EXPECT_EQ(avatar->GetText(),
            l10n_util::GetStringUTF16(
                IDS_SYNC_STATUS_NEEDS_PASSWORD_BUTTON_MAYBE_TITLE_CASE));
  ClearPassphraseError();
  // After clearing the passphrase error, the history sync opt-in entry point
  // should NOT be shown.
  EXPECT_TRUE(avatar->GetText().empty());
}

TEST_WITH_SIGNED_IN_FROM_PRE(IN_PROC_BROWSER_TEST_P,
                             MAYBE_AvatarToolbarButtonSyncPromoBrowserTest,
                             CollapsesOnSyncTurnedOn) {
  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser());
  EXPECT_EQ(avatar->GetText(),
            l10n_util::GetStringFUTF16(IDS_AVATAR_BUTTON_GREETING,
                                       test_given_name()));
  avatar->ClearActiveStateForTesting();
  // The greeting should be followed by the history sync opt-in entry point.
  EXPECT_EQ(avatar->GetText(), GetExpectedPromoText());
  // TODO(crbug.com/465718425): Change to enabling history sync instead of sync.
  EnableSync(test_email(), test_given_name());
  // Once sync is turned on, the button should return to the normal state.
  EXPECT_TRUE(avatar->GetText().empty());
}

TEST_WITH_SIGNED_IN_FROM_PRE(IN_PROC_BROWSER_TEST_P,
                             MAYBE_AvatarToolbarButtonSyncPromoBrowserTest,
                             CollapsesOnSignOut) {
  ASSERT_TRUE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));

  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser());
  EXPECT_EQ(avatar->GetText(),
            l10n_util::GetStringFUTF16(IDS_AVATAR_BUTTON_GREETING,
                                       test_given_name()));
  avatar->ClearActiveStateForTesting();
  // The greeting should be followed by the history sync opt-in entry point.
  EXPECT_EQ(avatar->GetText(), GetExpectedPromoText());
  Signout();
  // Once the user signs out, the button should return to the normal state.
  EXPECT_TRUE(avatar->GetText().empty());
}

IN_PROC_BROWSER_TEST_P(MAYBE_AvatarToolbarButtonSyncPromoBrowserTest,
                       PRE_ShowsOnBrowserRestart) {
  // Disable the preferences about syncing the tabs and history to make the
  // avatar promo eligible.
  SetHistoryAndTabsSyncingPreference(/*enable_sync=*/false);

  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser());
  // Normal state.
  ASSERT_TRUE(avatar->GetText().empty());
  const std::u16string email(u"test@gmail.com");
  const std::u16string account_name(u"Account name");
  const AccountInfo account_info = SigninWithImage(email, account_name);
  switch (GetParam()) {
    case FeaturePromoType::kHistorySyncPromo:
      ASSERT_EQ(avatar->GetText(), l10n_util::GetStringUTF16(
                                       IDS_AVATAR_BUTTON_MAKING_CHROME_YOURS));
      avatar->ClearActiveStateForTesting();
      break;
    case FeaturePromoType::kSyncPromo:
      ASSERT_EQ(
          avatar->GetText(),
          l10n_util::GetStringFUTF16(IDS_AVATAR_BUTTON_GREETING, account_name));
      avatar->ClearActiveStateForTesting();
      // The greeting should be followed by the sync opt-in entry point.
      ASSERT_EQ(avatar->GetText(), GetExpectedPromoText());
      avatar->ClearActiveStateForTesting();
      break;
  }
  // The button should return to the normal state.
  EXPECT_TRUE(avatar->GetText().empty());
}

IN_PROC_BROWSER_TEST_P(MAYBE_AvatarToolbarButtonSyncPromoBrowserTest,
                       ShowsOnBrowserRestart) {
  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser());
  // The greeting is shown after the restart.
  ASSERT_EQ(
      avatar->GetText(),
      l10n_util::GetStringFUTF16(IDS_AVATAR_BUTTON_GREETING, u"Account name"));
  avatar->ClearActiveStateForTesting();
  // The greeting should be followed by the history sync opt-in entry point.
  ASSERT_EQ(avatar->GetText(), GetExpectedPromoText());
  avatar->ClearActiveStateForTesting();
  // The button should return to the normal state.
  EXPECT_TRUE(avatar->GetText().empty());
}

TEST_WITH_SIGNED_IN_FROM_PRE(IN_PROC_BROWSER_TEST_P,
                             MAYBE_AvatarToolbarButtonSyncPromoBrowserTest,
                             HistorySyncOptinNotShownIfMaxShownCountReached) {
  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser());
  ASSERT_EQ(avatar->GetText(),
            l10n_util::GetStringFUTF16(IDS_AVATAR_BUTTON_GREETING,
                                       test_given_name()));
  avatar->ClearActiveStateForTesting();
  // The greeting should be followed by the history sync opt-in entry point.
  EXPECT_EQ(avatar->GetText(), GetExpectedPromoText());
  int shown_count = 1;
  avatar->ClearActiveStateForTesting();
  // The button comes back to the normal state.
  EXPECT_TRUE(avatar->GetText().empty());
  for (; shown_count < user_education::features::GetNewBadgeShowCount();
       ++shown_count) {
    avatar->ForceShowingPromoForTesting();
    EXPECT_EQ(avatar->GetText(), GetExpectedPromoText());
    avatar->ClearActiveStateForTesting();
    // The button comes back to the normal state.
    EXPECT_TRUE(avatar->GetText().empty());
  }
  avatar->ForceShowingPromoForTesting();
  // The history sync opt-in entry point should NOT be shown even after forcing
  // it to show if the max shown count has been reached.
  EXPECT_TRUE(avatar->GetText().empty());
}

INSTANTIATE_TEST_SUITE_P(,
                         MAYBE_AvatarToolbarButtonSyncPromoBrowserTest,
                         ValuesIn({FeaturePromoType::kHistorySyncPromo,
                                   FeaturePromoType::kSyncPromo}));

// TODO(crbug.com/331746545): Check the flaky test suite issue on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_AvatarToolbarButtonPromoClickBrowserTest \
  DISABLED_AvatarToolbarButtonPromoClickBrowserTest
#else
#define MAYBE_AvatarToolbarButtonPromoClickBrowserTest \
  AvatarToolbarButtonPromoClickBrowserTest
#endif
class MAYBE_AvatarToolbarButtonPromoClickBrowserTest
    : public MAYBE_AvatarToolbarButtonSyncPromoBrowserTest {
 protected:
  MAYBE_AvatarToolbarButtonPromoClickBrowserTest()
      : delegate_auto_reset_(signin_ui_util::SetSigninUiDelegateForTesting(
            &mock_signin_ui_delegate_)) {}

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

TEST_WITH_SIGNED_IN_FROM_PRE(IN_PROC_BROWSER_TEST_P,
                             MAYBE_AvatarToolbarButtonPromoClickBrowserTest,
                             CollapsesOnClickAndTriggersProfileMenuStartup) {
  ASSERT_TRUE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));

  base::HistogramTester histogram_tester;
  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser());
  ASSERT_EQ(avatar->GetText(),
            l10n_util::GetStringFUTF16(IDS_AVATAR_BUTTON_GREETING,
                                       test_given_name()));
  avatar->ClearActiveStateForTesting();
  // The greeting should be followed by the history sync opt-in entry point.
  EXPECT_EQ(avatar->GetText(), GetExpectedPromoText());
  histogram_tester.ExpectBucketCount("Signin.AvatarPillPromo.Shown",
                                     GetAvatarPromoType(GetParam()),
                                     /*expected_count=*/1);
  // The button action should be overridden.
  histogram_tester.ExpectTotalCount(
      "Signin.AvatarPillPromo.DurationBeforeClick",
      /*expected_count=*/0);
  Click(avatar);
  histogram_tester.ExpectTotalCount(
      "Signin.AvatarPillPromo.DurationBeforeClick",
      /*expected_count=*/1);
  auto* coordinator = browser()->GetFeatures().profile_menu_coordinator();
  ASSERT_NE(coordinator, nullptr);
  EXPECT_TRUE(coordinator->IsShowing());
  EXPECT_TRUE(avatar->GetText().empty());
  // Once the history sync opt-in entry point collapses, the button action
  // should be reset to the default behavior.
  CoreAccountId primary_account_id =
      GetIdentityManager()->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
  switch (GetParam()) {
    case FeaturePromoType::kHistorySyncPromo:
      // Clicking the history sync button in the profile menu should trigger the
      // history sync dialog with the correct access point
      // (`kHistorySyncOptinExpansionPillOnStartup`).
      EXPECT_CALL(
          mock_signin_ui_delegate_,
          ShowHistorySyncOptinUI(browser()->profile(), primary_account_id,
                                 signin_metrics::AccessPoint::
                                     kHistorySyncOptinExpansionPillOnStartup));
      break;
    case FeaturePromoType::kSyncPromo:
      // Clicking the sync button in the profile menu should trigger the sync
      // dialog with the correct access point
      // (`kHistorySyncOptinExpansionPillOnStartup`).
      EXPECT_CALL(mock_signin_ui_delegate_,
                  ShowTurnSyncOnUI(
                      browser()->profile(),
                      signin_metrics::AccessPoint::
                          kHistorySyncOptinExpansionPillOnStartup,
                      signin_metrics::PromoAction::PROMO_ACTION_WITH_DEFAULT,
                      primary_account_id,
                      TurnSyncOnHelper::SigninAbortedMode::KEEP_ACCOUNT,
                      /*is_sync_promo=*/false,
                      /*user_already_signed_in=*/true));
      break;
  }
  ASSERT_NO_FATAL_FAILURE(
      ClickSyncButton(coordinator->GetProfileMenuViewBaseForTesting()));
}

TEST_WITH_SIGNED_IN_FROM_PRE(IN_PROC_BROWSER_TEST_P,
                             MAYBE_AvatarToolbarButtonPromoClickBrowserTest,
                             HistorySyncOptinNotShownIfUsedLimitReached) {
  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser());
  ASSERT_EQ(avatar->GetText(),
            l10n_util::GetStringFUTF16(IDS_AVATAR_BUTTON_GREETING,
                                       test_given_name()));
  avatar->ClearActiveStateForTesting();
  // The greeting should be followed by the history sync opt-in entry point.
  EXPECT_EQ(avatar->GetText(), GetExpectedPromoText());
  // The button action should be overridden.
  Click(avatar);
  // The button comes back to the normal state.
  EXPECT_TRUE(avatar->GetText().empty());
  int used_count = 1;
  for (; used_count < user_education::features::GetNewBadgeFeatureUsedCount();
       ++used_count) {
    avatar->ForceShowingPromoForTesting();
    EXPECT_EQ(avatar->GetText(), GetExpectedPromoText());
    Click(avatar);
    // The button comes back to the normal state.
    EXPECT_TRUE(avatar->GetText().empty());
  }
  avatar->ForceShowingPromoForTesting();
  // The history sync opt-in entry point should NOT be shown even after forcing
  // it to show if the max used count has been reached.
  EXPECT_TRUE(avatar->GetText().empty());

  Signout();
  const std::u16string account_name_2(u"Account name 2");
  SigninWithImage(/*email=*/u"test2@gmail.com", account_name_2);
  switch (GetParam()) {
    case FeaturePromoType::kHistorySyncPromo:
      ASSERT_EQ(avatar->GetText(), l10n_util::GetStringUTF16(
                                       IDS_AVATAR_BUTTON_MAKING_CHROME_YOURS));
      avatar->ClearActiveStateForTesting();
      avatar->ForceShowingPromoForTesting();
      break;
    case FeaturePromoType::kSyncPromo:
      ASSERT_EQ(avatar->GetText(),
                l10n_util::GetStringFUTF16(IDS_AVATAR_BUTTON_GREETING,
                                           account_name_2));
      avatar->ClearActiveStateForTesting();
      break;
  }
  // The promo should be shown for the new account (rate limiting is per
  // account).
  EXPECT_EQ(avatar->GetText(), GetExpectedPromoText());
}

TEST_WITH_SIGNED_IN_FROM_PRE(
    IN_PROC_BROWSER_TEST_P,
    MAYBE_AvatarToolbarButtonPromoClickBrowserTest,
    TriggersAndCollapsesConsistentlyAcrossMultipleBrowsers) {
  // Make the delay for cross window animation replay zero to avoid flakiness.
  base::AutoReset<std::optional<base::TimeDelta>> delay_override_reset =
      signin_ui_util::
          CreateZeroOverrideDelayForCrossWindowAnimationReplayForTesting();
  base::HistogramTester histogram_tester;
  Profile* profile = browser()->profile();
  Browser* browser_1 = browser();
  AvatarToolbarButton* avatar_1 = GetAvatarToolbarButton(browser_1);
  ASSERT_EQ(avatar_1->GetText(),
            l10n_util::GetStringFUTF16(IDS_AVATAR_BUTTON_GREETING,
                                       test_given_name()));
  avatar_1->ClearActiveStateForTesting();

  // The greeting should be followed by the history sync opt-in.
  EXPECT_EQ(avatar_1->GetText(), GetExpectedPromoText());
  // Open the second browser before the history sync opt-in collapses.
  Browser* browser_2 = CreateBrowser(profile);
  AvatarToolbarButton* avatar_2 = GetAvatarToolbarButton(browser_2);
  // The history sync opt-in should be shown in the second browser as well.
  EXPECT_EQ(avatar_2->GetText(), GetExpectedPromoText());
  // `Signin.AvatarPillPromo.Shown` histogram should be recorded only
  // once.
  histogram_tester.ExpectBucketCount("Signin.AvatarPillPromo.Shown",
                                     GetAvatarPromoType(GetParam()),
                                     /*expected_count=*/1);
  avatar_1->ClearActiveStateForTesting();
  // The button in both browsers comes back to the normal state.
  EXPECT_TRUE(avatar_1->GetText().empty());
  EXPECT_TRUE(avatar_2->GetText().empty());
}

INSTANTIATE_TEST_SUITE_P(HistorySyncOptinExpansionPillOptions,
                         MAYBE_AvatarToolbarButtonPromoClickBrowserTest,
                         ValuesIn({FeaturePromoType::kHistorySyncPromo,
                                   FeaturePromoType::kSyncPromo}));

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
  AvatarToolbarButtonEnterpriseBadgingBrowserTest() = default;

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

  base::test::ScopedFeatureList scoped_feature_list_{
      syncer::kReplaceSyncPromosWithSignInPromos};
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

  EnableSyncWithImage(u"work@managed.com");
  ASSERT_EQ(avatar_button->GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_MAKING_CHROME_YOURS));
  avatar_button->ClearActiveStateForTesting();

  ASSERT_EQ(avatar_button->GetText(), work_label);

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
                       PRE_ManagedAccountFlowWithDefaultWorkBadge) {
  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser());
  // Normal state.
  ASSERT_TRUE(avatar->GetText().empty());

  SigninWithImage(u"work@managed.com", test_given_name());
  enterprise_util::SetUserAcceptedAccountManagement(browser()->profile(), true);
  EXPECT_EQ(avatar->GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_MAKING_CHROME_YOURS));
}

IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonEnterpriseBadgingBrowserTest,
                       ManagedAccountFlowWithDefaultWorkBadge) {
  ASSERT_TRUE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));

  // Disable the preferences about syncing the tabs and history to make the
  // avatar promo eligible.
  SetHistoryAndTabsSyncingPreference(/*enable_sync=*/false);

  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser());
  // Greeting shown even for Managed users.
  EXPECT_EQ(avatar->GetText(),
            l10n_util::GetStringFUTF16(IDS_AVATAR_BUTTON_GREETING,
                                       test_given_name()));
  avatar->ClearActiveStateForTesting();
  // The greeting is followed by the history sync opt-in.
  EXPECT_EQ(avatar->GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SYNC_HISTORY));
  avatar->ClearActiveStateForTesting();
  EXPECT_EQ(avatar->GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_WORK));
}

// TODO(crbug.com/331746545): Check flaky test issue on windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_PRE_SignedInWithNewSessionKeepCustomWorkBadge \
  DISABLED_PRE_SignedInWithNewSessionKeepCustomWorkBadge
#define MAYBE_SignedInWithNewSessionKeepCustomWorkBadge \
  DISABLED_SignedInWithNewSessionKeepCustomWorkBadge
#else
#define MAYBE_PRE_SignedInWithNewSessionKeepCustomWorkBadge \
  PRE_SignedInWithNewSessionKeepCustomWorkBadge
#define MAYBE_SignedInWithNewSessionKeepCustomWorkBadge \
  SignedInWithNewSessionKeepCustomWorkBadge
#endif
// Tests the flow for a managed sign-in.
IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonEnterpriseBadgingBrowserTest,
                       MAYBE_PRE_SignedInWithNewSessionKeepCustomWorkBadge) {
  // Sign in.
  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser());
  AccountInfo account_info =
      SigninWithImage(u"work@managed.com", test_given_name());

  // Accept management and prepare work badge.
  enterprise_util::SetUserAcceptedAccountManagement(browser()->profile(), true);
  browser()->profile()->GetPrefs()->SetString(
      prefs::kEnterpriseCustomLabelForProfile, base::UTF16ToUTF8(work_badge()));

  EXPECT_EQ(avatar->GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_MAKING_CHROME_YOURS));
}

// Test that the work badge remains upon restart for a user that is managed.
IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonEnterpriseBadgingBrowserTest,
                       MAYBE_SignedInWithNewSessionKeepCustomWorkBadge) {
  ASSERT_TRUE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  ASSERT_TRUE(
      enterprise_util::UserAcceptedAccountManagement(browser()->profile()));

  // Disable the preferences about syncing the tabs and history to make the
  // avatar promo eligible.
  SetHistoryAndTabsSyncingPreference(/*enable_sync=*/false);

  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser());
  EXPECT_EQ(avatar->GetText(),
            l10n_util::GetStringFUTF16(IDS_AVATAR_BUTTON_GREETING,
                                       test_given_name()));
  avatar->ClearActiveStateForTesting();
  // The greeting is followed by the history sync opt-in.
  EXPECT_EQ(avatar->GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SYNC_HISTORY));
  avatar->ClearActiveStateForTesting();

  // Once the promo is not shown anymore, we expect the work badge to be shown.
  EXPECT_EQ(avatar->GetText(), work_badge());
  EXPECT_EQ(GetProfileAttributesEntry(browser()->profile())
                ->GetEnterpriseProfileLabel(),
            work_badge());
  EXPECT_EQ(
      GetProfileAttributesEntry(browser()->profile())->GetLocalProfileName(),
      work_badge());

  // Previously added image on signin should still be shown in the new session.
  EXPECT_TRUE(IsSignedInImageUsed());
}

// TODO(b/331746545): Check flaky test issue on windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_SigninPendingFromExternalErrorThenReauth \
  DISABLED_SigninPendingFromExternalErrorThenReauth
#else
#define MAYBE_SigninPendingFromExternalErrorThenReauth \
  SigninPendingFromExternalErrorThenReauth
#endif
IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonBrowserTest,
                       MAYBE_SigninPendingFromExternalErrorThenReauth) {
  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser());
  SigninWithImageAndClearGreetingAndSyncPromo(avatar, u"test@gmail.com");
  ASSERT_EQ(avatar->GetText(), std::u16string());

  // Browser opened before the error.
  Browser* opened_browser = CreateBrowser(browser()->profile());
  AvatarToolbarButton* opened_browser_avatar_button =
      GetAvatarToolbarButton(opened_browser);
  ASSERT_EQ(opened_browser_avatar_button->GetText(), std::u16string());

  SimulateSigninPending(/*web_sign_out=*/false);
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

  ClearSigninPending();
  EXPECT_EQ(avatar->GetText(), std::u16string());
  EXPECT_EQ(opened_browser_avatar_button->GetText(), std::u16string());
  EXPECT_EQ(new_browser_avatar_button->GetText(), std::u16string());
}

// TODO(b/331746545): Check flaky test issue on windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_SigninPendingFromWebSignout DISABLED_SigninPendingFromWebSignout
#else
#define MAYBE_SigninPendingFromWebSignout SigninPendingFromWebSignout
#endif
IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonBrowserTest,
                       MAYBE_SigninPendingFromWebSignout) {
  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser());

  SigninWithImageAndClearGreetingAndSyncPromo(avatar, u"test@gmail.com");
  ASSERT_EQ(avatar->GetText(), std::u16string());

  // Browser opened before the error.
  Browser* opened_browser = CreateBrowser(browser()->profile());
  AvatarToolbarButton* opened_browser_avatar_button =
      GetAvatarToolbarButton(opened_browser);
  ASSERT_EQ(opened_browser_avatar_button->GetText(), std::u16string());

  SimulateSigninPending(/*web_sign_out=*/true);
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
  avatar->ClearActiveStateForTesting();
  opened_browser_avatar_button->ClearActiveStateForTesting();
  new_browser_avatar_button->ClearActiveStateForTesting();

  EXPECT_EQ(avatar->GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SIGNIN_PAUSED));
  EXPECT_EQ(opened_browser_avatar_button->GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SIGNIN_PAUSED));
  EXPECT_EQ(new_browser_avatar_button->GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SIGNIN_PAUSED));

  ClearSigninPending();
  EXPECT_EQ(avatar->GetText(), std::u16string());
  EXPECT_EQ(opened_browser_avatar_button->GetText(), std::u16string());
  EXPECT_EQ(new_browser_avatar_button->GetText(), std::u16string());
}

// TODO(b/331746545): Check flaky test issue on windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_SigninPendingThenSignout DISABLED_SigninPendingThenSignout
#else
#define MAYBE_SigninPendingThenSignout SigninPendingThenSignout
#endif
IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonBrowserTest,
                       MAYBE_SigninPendingThenSignout) {
  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser());
  SigninWithImageAndClearGreetingAndSyncPromo(avatar, u"test@gmail.com");
  ASSERT_EQ(avatar->GetText(), std::u16string());

  SimulateSigninPending(/*web_sign_out=*/false);

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
  SimulateSigninPending(/*web_sign_out=*/false);

  EXPECT_EQ(accessibility.GetCachedName(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SIGNIN_PAUSED));
  EXPECT_EQ(accessibility.GetCachedDescription(),
            l10n_util::GetStringUTF16(
                IDS_AVATAR_BUTTON_SIGNIN_PENDING_ACCESSIBILITY_LABEL));

  ClearSigninPending();

  EXPECT_EQ(accessibility.GetCachedName(), expected_profile_name_with_account);
  EXPECT_EQ(accessibility.GetCachedDescription(), std::u16string());

  // This will not trigger the immediate button content text change.
  // Accessibility text should adapt as well.
  SimulateSigninPending(/*web_sign_out=*/true);

  EXPECT_EQ(accessibility.GetCachedName(),
            l10n_util::GetStringUTF16(
                IDS_AVATAR_BUTTON_SIGNIN_PENDING_ACCESSIBILITY_LABEL));
  EXPECT_EQ(accessibility.GetCachedDescription(),
            expected_profile_name_with_account);

  ClearSigninPending();

  EXPECT_EQ(accessibility.GetCachedName(), expected_profile_name_with_account);
  EXPECT_EQ(accessibility.GetCachedDescription(), std::u16string());

  Signout();

  EXPECT_EQ(accessibility.GetCachedName(), profile_name);
  EXPECT_EQ(accessibility.GetCachedDescription(), std::u16string());
}

// TODO(crbug.com/359995696): Flaky on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_PassphraseErrorSignedIn DISABLED_PassphraseErrorSignedIn
#else
#define MAYBE_PassphraseErrorSignedIn PassphraseErrorSignedIn
#endif
IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonBrowserTest,
                       MAYBE_PassphraseErrorSignedIn) {
  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser());
  SigninWithImageAndClearGreetingAndSyncPromo(avatar, u"test@gmail.com");
  ASSERT_EQ(avatar->GetText(), std::u16string());
  SimulatePassphraseError();
  EXPECT_EQ(avatar->GetText(),
            l10n_util::GetStringUTF16(
                IDS_SYNC_STATUS_NEEDS_PASSWORD_BUTTON_MAYBE_TITLE_CASE));
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
  EXPECT_EQ(avatar->GetText(),
            l10n_util::GetStringUTF16(
                IDS_SYNC_STATUS_NEEDS_PASSWORD_BUTTON_MAYBE_TITLE_CASE));
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
            l10n_util::GetStringUTF16(IDS_SYNC_UPGRADE_CLIENT_BUTTON));
}

#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

// TODO(crbug.com/331746545): Check the flaky test issue on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_ShowMakingChromeYoursOnSignin \
  DISABLED_ShowMakingChromeYoursOnSignin
#else
#define MAYBE_ShowMakingChromeYoursOnSignin ShowMakingChromeYoursOnSignin
#endif
IN_PROC_BROWSER_TEST_F(
    AvatarToolbarButtonReplaceSyncPromosWithSignInPromosBrowserTest,
    MAYBE_ShowMakingChromeYoursOnSignin) {
  AvatarToolbarButton* avatar_toolbar_button =
      GetAvatarToolbarButton(browser());
  ASSERT_NE(avatar_toolbar_button, nullptr);
  // Normal state.
  ASSERT_TRUE(avatar_toolbar_button->GetText().empty());
  SigninWithImage(/*email=*/u"test@gmail.com", /*name=*/u"Account");
  EXPECT_EQ(avatar_toolbar_button->GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_MAKING_CHROME_YOURS));
  avatar_toolbar_button->ClearActiveStateForTesting();
  EXPECT_TRUE(avatar_toolbar_button->GetText().empty());

  // A new browser within the same session should not show any text as well.
  // Specifically not showing the greeting.
  Browser* second_browser = CreateBrowser(browser()->profile());
  EXPECT_TRUE(GetAvatarToolbarButton(second_browser)->GetText().empty());
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// TODO(crbug.com/331746545): Check the flaky test issue on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_ClearMakingChromeYoursOnSignout \
  DISABLED_ClearMakingChromeYoursOnSignout
#else
#define MAYBE_ClearMakingChromeYoursOnSignout ClearMakingChromeYoursOnSignout
#endif
IN_PROC_BROWSER_TEST_F(
    AvatarToolbarButtonReplaceSyncPromosWithSignInPromosBrowserTest,
    MAYBE_ClearMakingChromeYoursOnSignout) {
  AvatarToolbarButton* avatar_toolbar_button =
      GetAvatarToolbarButton(browser());
  ASSERT_NE(avatar_toolbar_button, nullptr);
  // Normal state.
  ASSERT_TRUE(avatar_toolbar_button->GetText().empty());
  SigninWithImage(/*email=*/u"test@gmail.com", /*name=*/u"Account");
  EXPECT_EQ(avatar_toolbar_button->GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_MAKING_CHROME_YOURS));

  Signout();
  EXPECT_TRUE(avatar_toolbar_button->GetText().empty());
}

// TODO(crbug.com/331746545): Check the flaky test issue on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_ShowMakingChromeYoursOnSigninThenClick \
  DISABLED_ShowMakingChromeYoursOnSigninThenClick
#else
#define MAYBE_ShowMakingChromeYoursOnSigninThenClick \
  ShowMakingChromeYoursOnSigninThenClick
#endif
IN_PROC_BROWSER_TEST_F(
    AvatarToolbarButtonReplaceSyncPromosWithSignInPromosBrowserTest,
    MAYBE_ShowMakingChromeYoursOnSigninThenClick) {
  AvatarToolbarButton* avatar_toolbar_button =
      GetAvatarToolbarButton(browser());
  ASSERT_NE(avatar_toolbar_button, nullptr);
  // Normal state.
  ASSERT_TRUE(avatar_toolbar_button->GetText().empty());
  SigninWithImage(/*email=*/u"test@gmail.com", /*name=*/u"Account");
  EXPECT_EQ(avatar_toolbar_button->GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_MAKING_CHROME_YOURS));

  // A new browser should also show the message.
  Browser* second_browser = CreateBrowser(browser()->profile());
  AvatarToolbarButton* second_avatar_toolbar_button =
      GetAvatarToolbarButton(second_browser);
  EXPECT_EQ(second_avatar_toolbar_button->GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_MAKING_CHROME_YOURS));

  // Clicking on either avatar buttons should clear both messages.
  Click(avatar_toolbar_button);
  EXPECT_TRUE(avatar_toolbar_button->GetText().empty());
  EXPECT_TRUE(second_avatar_toolbar_button->GetText().empty());
}

// TODO(crbug.com/331746545): Check the flaky test issue on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_ShowMakingChromeYoursOnSigninBeforeBrowserWindow \
  DISABLED_ShowMakingChromeYoursOnSigninBeforeBrowserWindow
#else
#define MAYBE_ShowMakingChromeYoursOnSigninBeforeBrowserWindow \
  ShowMakingChromeYoursOnSigninBeforeBrowserWindow
#endif
IN_PROC_BROWSER_TEST_F(
    AvatarToolbarButtonReplaceSyncPromosWithSignInPromosBrowserTest,
    MAYBE_ShowMakingChromeYoursOnSigninBeforeBrowserWindow) {
  // Create a new profile and sign in.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ASSERT_NE(profile_manager, nullptr);
  Profile& profile = profiles::testing::CreateProfileSync(
      profile_manager, profile_manager->GenerateNextProfileDirectoryPath());
  signin::MakePrimaryAccountAvailable(
      IdentityManagerFactory::GetForProfile(&profile), "test@gmail.com",
      signin::ConsentLevel::kSignin);

  // Create a new browser window for the new profile.
  Browser* browser = CreateBrowser(&profile);
  AvatarToolbarButton* avatar_toolbar_button = GetAvatarToolbarButton(browser);
  ASSERT_NE(avatar_toolbar_button, nullptr);

  // The on sign-in state should be shown after the the browser window is
  // created if the sign-in event happened before the browser window was
  // created.
  EXPECT_EQ(avatar_toolbar_button->GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_MAKING_CHROME_YOURS));
  avatar_toolbar_button->ClearActiveStateForTesting();
  // The button should return to the normal state.
  EXPECT_TRUE(avatar_toolbar_button->GetText().empty());
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

// TODO(crbug.com/331746545): Check the flaky test issue on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_ShowMakingChromeYoursOnSigninAndSync \
  DISABLED_ShowMakingChromeYoursOnSigninAndSync
#else
#define MAYBE_ShowMakingChromeYoursOnSigninAndSync \
  ShowMakingChromeYoursOnSigninAndSync
#endif
IN_PROC_BROWSER_TEST_F(
    AvatarToolbarButtonReplaceSyncPromosWithSignInPromosBrowserTest,
    MAYBE_ShowMakingChromeYoursOnSigninAndSync) {
  AvatarToolbarButton* avatar_toolbar_button =
      GetAvatarToolbarButton(browser());
  ASSERT_NE(avatar_toolbar_button, nullptr);
  // Normal state.
  ASSERT_TRUE(avatar_toolbar_button->GetText().empty());
  EnableSync(/*email=*/u"test@gmail.com", /*name=*/u"Account");
  EXPECT_EQ(avatar_toolbar_button->GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_MAKING_CHROME_YOURS));
  avatar_toolbar_button->ClearActiveStateForTesting();
  EXPECT_TRUE(avatar_toolbar_button->GetText().empty());
}

// TODO(crbug.com/331746545): Check the flaky test issue on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_MakingChromeYoursThenExplicitState \
  DISABLED_MakingChromeYoursThenExplicitState
#else
#define MAYBE_MakingChromeYoursThenExplicitState \
  MakingChromeYoursThenExplicitState
#endif
IN_PROC_BROWSER_TEST_F(
    AvatarToolbarButtonReplaceSyncPromosWithSignInPromosBrowserTest,
    MAYBE_MakingChromeYoursThenExplicitState) {
  AvatarToolbarButton* avatar_toolbar_button =
      GetAvatarToolbarButton(browser());
  ASSERT_NE(avatar_toolbar_button, nullptr);
  // Normal state.
  ASSERT_TRUE(avatar_toolbar_button->GetText().empty());
  Signin(/*email=*/u"test@gmail.com", /*name=*/u"Account");
  EXPECT_EQ(avatar_toolbar_button->GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_MAKING_CHROME_YOURS));

  const std::u16string explicit_state_text(u"Explicit State");
  base::ScopedClosureRunner hide_callback =
      avatar_toolbar_button->SetExplicitButtonState(
          explicit_state_text, /*accessibility_label=*/std::nullopt,
          /*explicit_action=*/std::nullopt);
  EXPECT_EQ(avatar_toolbar_button->GetText(), explicit_state_text);
  hide_callback.RunAndReset();

  // The on sign-in state is hidden.
  EXPECT_EQ(avatar_toolbar_button->GetText(), std::u16string());
}

// TODO(crbug.com/331746545): Check the flaky test issue on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_MakingChromeYoursThenSyncError \
  DISABLED_MakingChromeYoursThenSyncError
#else
#define MAYBE_MakingChromeYoursThenSyncError MakingChromeYoursThenSyncError
#endif
IN_PROC_BROWSER_TEST_F(
    AvatarToolbarButtonReplaceSyncPromosWithSignInPromosBrowserTest,
    MAYBE_MakingChromeYoursThenSyncError) {
  AvatarToolbarButton* avatar_toolbar_button =
      GetAvatarToolbarButton(browser());
  ASSERT_NE(avatar_toolbar_button, nullptr);
  // Normal state.
  ASSERT_TRUE(avatar_toolbar_button->GetText().empty());
  EnableSync(/*email=*/u"test@gmail.com", /*name=*/u"Account");
  EXPECT_EQ(avatar_toolbar_button->GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_MAKING_CHROME_YOURS));
  SimulateSyncError();
  // On sign-in state is higher priority than any sync error state.
  EXPECT_EQ(avatar_toolbar_button->GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_MAKING_CHROME_YOURS));
  avatar_toolbar_button->ClearActiveStateForTesting();
  // Once the sign-in state is cleared, the sync error state is shown.
  EXPECT_EQ(avatar_toolbar_button->GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SYNC_ERROR));
}

// TODO(crbug.com/331746545): Check the flaky test issue on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_PRE_DoesNotShowOnBrowserRestart \
  DISABLED_PRE_DoesNotShowOnBrowserRestart
#define MAYBE_DoesNotShowOnBrowserRestart DISABLED_DoesNotShowOnBrowserRestart
#else
#define MAYBE_PRE_DoesNotShowOnBrowserRestart PRE_DoesNotShowOnBrowserRestart
#define MAYBE_DoesNotShowOnBrowserRestart DoesNotShowOnBrowserRestart
#endif
IN_PROC_BROWSER_TEST_F(
    AvatarToolbarButtonReplaceSyncPromosWithSignInPromosBrowserTest,
    MAYBE_PRE_DoesNotShowOnBrowserRestart) {
  AvatarToolbarButton* avatar_toolbar_button =
      GetAvatarToolbarButton(browser());
  ASSERT_NE(avatar_toolbar_button, nullptr);
  // Normal state.
  ASSERT_TRUE(avatar_toolbar_button->GetText().empty());
  SigninWithImage(/*email=*/u"test@gmail.com", /*name=*/u"Account");
  EXPECT_EQ(avatar_toolbar_button->GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_MAKING_CHROME_YOURS));
  avatar_toolbar_button->ClearActiveStateForTesting();
  EXPECT_TRUE(avatar_toolbar_button->GetText().empty());
}

IN_PROC_BROWSER_TEST_F(
    AvatarToolbarButtonReplaceSyncPromosWithSignInPromosBrowserTest,
    MAYBE_DoesNotShowOnBrowserRestart) {
  AvatarToolbarButton* avatar_toolbar_button =
      GetAvatarToolbarButton(browser());
  ASSERT_NE(avatar_toolbar_button, nullptr);
  // The greetings are shown after the restart.
  EXPECT_EQ(avatar_toolbar_button->GetText(),
            l10n_util::GetStringFUTF16(IDS_AVATAR_BUTTON_GREETING, u"Account"));
  avatar_toolbar_button->ClearActiveStateForTesting();
  // The button should return to the normal state.
  EXPECT_TRUE(avatar_toolbar_button->GetText().empty());
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// TODO(crbug.com/331746545): Check flaky test issue on windows.
#if !BUILDFLAG(IS_WIN)
TEST_WITH_SIGNED_IN_FROM_PRE(
    IN_PROC_BROWSER_TEST_F,
    AvatarToolbarButtonReplaceSyncPromosWithSignInPromosBrowserTest,
    ShowBatchUploadBookmarksPromo) {
  const GaiaId primary_account_gaia_id =
      GetIdentityManager()
          ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .gaia;
  ASSERT_FALSE(primary_account_gaia_id.empty());
  SetHistoryAndTabsSyncingPreference(/*enable_sync=*/false);
  browser()->profile()->GetPrefs()->SetString(
      prefs::kGoogleServicesLastSyncingGaiaId,
      primary_account_gaia_id.ToString());
  batch_upload_test_helper().SetReturnDescriptions(syncer::BOOKMARKS,
                                                   /*item_count=*/5);

  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser());
  ASSERT_EQ(avatar->GetText(),
            l10n_util::GetStringFUTF16(IDS_AVATAR_BUTTON_GREETING,
                                       test_given_name()));
  avatar->ClearActiveStateForTesting();

  ASSERT_EQ(
      avatar->GetText(),
      l10n_util::GetStringUTF16(
          IDS_AVATAR_BUTTON_BATCH_UPLOAD_PROMO_WITH_BOOKMARK_CLEANUP_PROMO));
  avatar->ClearActiveStateForTesting();

  // Once the greeting and promo are not shown anymore, we expect no text.
  EXPECT_EQ(avatar->GetText(), std::u16string());
}
#endif  // !BUILDFLAG(IS_WIN)

// TODO(crbug.com/331746545): Check flaky test issue on windows.
#if !BUILDFLAG(IS_WIN)
TEST_WITH_SIGNED_IN_FROM_PRE(
    IN_PROC_BROWSER_TEST_F,
    AvatarToolbarButtonReplaceSyncPromosWithSignInPromosBrowserTest,
    ShowBatchUploadPromo) {
  ASSERT_TRUE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  SetHistoryAndTabsSyncingPreference(/*enable_sync=*/true);
  batch_upload_test_helper().SetReturnDescriptions(syncer::PASSWORDS,
                                                   /*item_count=*/5);

  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser());
  ASSERT_EQ(avatar->GetText(),
            l10n_util::GetStringFUTF16(IDS_AVATAR_BUTTON_GREETING,
                                       test_given_name()));
  avatar->ClearActiveStateForTesting();

  ASSERT_EQ(avatar->GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_BATCH_UPLOAD_PROMO));
  avatar->ClearActiveStateForTesting();

  // Once the greeting and promo are not shown anymore, we expect no text.
  EXPECT_EQ(avatar->GetText(), std::u16string());
}
#endif  // !BUILDFLAG(IS_WIN)

class AvatarToolbarButtonWithWindows10DepreciationBrowserTest
    : public AvatarToolbarButtonBrowserTest {
 public:
  AvatarToolbarButtonWithWindows10DepreciationBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{syncer::kReplaceSyncPromosWithSignInPromos,
                              switches::
                                  kSigninWindows10DepreciationStateForTesting},
        /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// TODO(crbug.com/331746545): Check flaky test issue on windows.
#if !BUILDFLAG(IS_WIN)
TEST_WITH_SIGNED_IN_FROM_PRE(
    IN_PROC_BROWSER_TEST_F,
    AvatarToolbarButtonWithWindows10DepreciationBrowserTest,
    ShowBatchUploadWindowsDepreciationPromo) {
  ASSERT_TRUE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  SetHistoryAndTabsSyncingPreference(/*enable_sync=*/false);
  batch_upload_test_helper().SetReturnDescriptions(syncer::PASSWORDS,
                                                   /*item_count=*/5);

  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser());
  ASSERT_EQ(avatar->GetText(),
            l10n_util::GetStringFUTF16(IDS_AVATAR_BUTTON_GREETING,
                                       test_given_name()));
  avatar->ClearActiveStateForTesting();

  ASSERT_EQ(avatar->GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SYNC_PROMO));
  avatar->ClearActiveStateForTesting();

  // Once the greeting and promo are not shown anymore, we expect no text.
  EXPECT_EQ(avatar->GetText(), std::u16string());
}
#endif  // !BUILDFLAG(IS_WIN)

// TODO(crbug.com/331746545): Check flaky test issue on windows.
#if !BUILDFLAG(IS_WIN)
TEST_WITH_SIGNED_IN_FROM_PRE(
    IN_PROC_BROWSER_TEST_F,
    AvatarToolbarButtonReplaceSyncPromosWithSignInPromosBrowserTest,
    NoPromoShownUntilSyncServiceIsInitialized) {
  const GaiaId primary_account_gaia_id =
      GetIdentityManager()
          ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .gaia;
  ASSERT_FALSE(primary_account_gaia_id.empty());
  SetHistoryAndTabsSyncingPreference(/*enable_sync=*/false);
  browser()->profile()->GetPrefs()->SetString(
      ::prefs::kGoogleServicesLastSyncingGaiaId,
      primary_account_gaia_id.ToString());
  batch_upload_test_helper().SetReturnDescriptions(syncer::BOOKMARKS,
                                                   /*item_count=*/5);
  SetSyncServiceTransportState(
      syncer::SyncService::TransportState::INITIALIZING);

  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser());
  ASSERT_EQ(avatar->GetText(),
            l10n_util::GetStringFUTF16(IDS_AVATAR_BUTTON_GREETING,
                                       test_given_name()));
  avatar->ClearActiveStateForTesting();
  // No Promo shown as long as the sync service is not active.
  ASSERT_EQ(avatar->GetText(), std::u16string());

  // Check crbug.com/454927990.
  SetSyncServiceTransportState(
      syncer::SyncService::TransportState::CONFIGURING);
  // No Promo shown as long as the sync service is not active.
  ASSERT_EQ(avatar->GetText(), std::u16string());

  SetSyncServiceTransportState(syncer::SyncService::TransportState::ACTIVE);
  ASSERT_EQ(
      avatar->GetText(),
      l10n_util::GetStringUTF16(
          IDS_AVATAR_BUTTON_BATCH_UPLOAD_PROMO_WITH_BOOKMARK_CLEANUP_PROMO));
  avatar->ClearActiveStateForTesting();

  // Once the greeting and promo are not shown anymore, we expect no text.
  EXPECT_EQ(avatar->GetText(), std::u16string());
}
#endif  // !BUILDFLAG(IS_WIN)

#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
class AvatarToolbarButtonSignInBenefitsIphBrowserTest
    : public InteractiveFeaturePromoTestMixin<AvatarToolbarButtonBrowserTest> {
 public:
  AvatarToolbarButtonSignInBenefitsIphBrowserTest()
      : InteractiveFeaturePromoTestMixin(UseDefaultTrackerAllowingPromos(
            {feature_engagement::kIPHSignInBenefitsFeature})) {
    // Disable the migration feature flag for PRE tests. This allows simulating
    // users signing in before the sync-to-signin migration.
    if (content::IsPreTest()) {
      feature_list_.InitAndDisableFeature(
          syncer::kReplaceSyncPromosWithSignInPromos);
    } else {
      feature_list_.InitAndEnableFeature(
          syncer::kReplaceSyncPromosWithSignInPromos);
    }
  }

  bool WillShowPromo() {
    auto* const user_education = BrowserUserEducationInterface::From(browser());
    return user_education->IsFeaturePromoActive(
               feature_engagement::kIPHSignInBenefitsFeature) ||
           user_education->IsFeaturePromoQueued(
               feature_engagement::kIPHSignInBenefitsFeature);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonSignInBenefitsIphBrowserTest,
                       PRE_ShownForUsersSignedInBeforeMigration) {
  // Sign in a user before the sync-to-signin migration.
  Signin(/*email=*/u"test@gmail.com", /*name=*/u"Account");

  PrefService* prefs = browser()->profile()->GetPrefs();
  ASSERT_FALSE(
      prefs->GetBoolean(prefs::kPrimaryAccountSetAfterSigninMigration));
}

// Tests that the IPH bubble for signin benefits is shown for users who have
// signed in before the sync-to-signin migration.
IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonSignInBenefitsIphBrowserTest,
                       ShownForUsersSignedInBeforeMigration) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTabContents);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTabContents);

  RunTestSequence(
      InstrumentTab(kFirstTabContents, 0),
      WaitForPromo(feature_engagement::kIPHSignInBenefitsFeature),
      PressNonDefaultPromoButton(), InstrumentTab(kSecondTabContents, 1),
      WaitForWebContentsReady(kSecondTabContents,
                              GURL(chrome::kChromeUIAccountSettingsURL)),
      CheckPromoActive(feature_engagement::kIPHSignInBenefitsFeature, false));
}

// Tests that the IPH bubble for signin benefits is not shown for users who have
// signed in after the sync-to-signin migration.
IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonSignInBenefitsIphBrowserTest,
                       NotShownForUsersSignedInAfterMigration) {
  // Sign in a user after the sync-to-signin migration.
  Signin(/*email=*/u"test@gmail.com", /*name=*/u"Account");

  AvatarToolbarButton* avatar_toolbar_button =
      GetAvatarToolbarButton(browser());
  ASSERT_NE(avatar_toolbar_button, nullptr);

  // Attempt to show the IPH.
  avatar_toolbar_button->MaybeShowSignInBenefitsIPH();
  EXPECT_FALSE(WillShowPromo());
}

IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonSignInBenefitsIphBrowserTest,
                       PRE_NotShownForUsersMigratedFromDice) {
  // Sign in a user before the sync-to-signin migration.
  Signin(/*email=*/u"test@gmail.com", /*name=*/u"Account");

  PrefService* prefs = browser()->profile()->GetPrefs();
  ASSERT_FALSE(
      prefs->GetBoolean(prefs::kPrimaryAccountSetAfterSigninMigration));

  // Simulate user having migrated from DICe.
  prefs->SetBoolean(kDiceMigrationMigrated, true);
}

// Tests that the IPH bubble for signin benefits is not shown for users who have
// migrated from DICe.
IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonSignInBenefitsIphBrowserTest,
                       NotShownForUsersMigratedFromDice) {
  AvatarToolbarButton* avatar_toolbar_button =
      GetAvatarToolbarButton(browser());
  ASSERT_NE(avatar_toolbar_button, nullptr);

  // Attempt to show the IPH.
  avatar_toolbar_button->MaybeShowSignInBenefitsIPH();
  EXPECT_FALSE(WillShowPromo());
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#if !BUILDFLAG(IS_CHROMEOS)
class MockPasskeyUnlockManager : public webauthn::PasskeyUnlockManager {
 public:
  MOCK_METHOD(bool, ShouldDisplayErrorUi, (), (const, override));
};

class AvatarToolbarButtonPasskeyUnlockErrorBrowserTest
    : public AvatarToolbarButtonBrowserTest {
 public:
  AvatarToolbarButtonPasskeyUnlockErrorBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{device::kPasskeyUnlockErrorUi,
                              device::kPasskeyUnlockManager,
                              device::kWebAuthnOpportunisticRetrieval},
        /*disabled_features=*/{});
  }

  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    AvatarToolbarButtonBrowserTest::SetUpBrowserContextKeyedServices(context);
    webauthn::PasskeyUnlockManagerFactory::GetInstance()
        ->SetTestingFactoryAndUse(
            context, base::BindRepeating(
                         &AvatarToolbarButtonPasskeyUnlockErrorBrowserTest::
                             CreateMockPasskeyUnlockManager,
                         base::Unretained(this)));
  }

  void TearDownOnMainThread() override {
    mock_passkey_unlock_manager_ = nullptr;
    AvatarToolbarButtonBrowserTest::TearDownOnMainThread();
  }

 protected:
  MockPasskeyUnlockManager* passkey_unlock_manager() {
    return mock_passkey_unlock_manager_;
  }

  static constexpr const char kPasskeyUnlockErrorUiEventHistogram[] =
      "WebAuthentication.PasskeyUnlock.ErrorUi.Event";

 private:
  std::unique_ptr<KeyedService> CreateMockPasskeyUnlockManager(
      content::BrowserContext* context) {
    auto passkey_unlock_manager =
        std::make_unique<testing::NiceMock<MockPasskeyUnlockManager>>();
    mock_passkey_unlock_manager_ = passkey_unlock_manager.get();
    ON_CALL(*passkey_unlock_manager, ShouldDisplayErrorUi())
        .WillByDefault(testing::Return(false));
    return passkey_unlock_manager;
  }

  raw_ptr<MockPasskeyUnlockManager> mock_passkey_unlock_manager_ = nullptr;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonPasskeyUnlockErrorBrowserTest,
                       PasskeyUnlockError) {
  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser());
  SigninWithImageAndClearGreetingAndSyncPromo(avatar, u"test@gmail.com");

  base::HistogramTester histogram_tester;
  // Simulate the error appearing.
  ON_CALL(*passkey_unlock_manager(), ShouldDisplayErrorUi())
      .WillByDefault(testing::Return(true));
  passkey_unlock_manager()->NotifyObserversForTesting();

  EXPECT_EQ(avatar->GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_PASSKEYS_ERROR_VERIFY));
  histogram_tester.ExpectBucketCount(
      kPasskeyUnlockErrorUiEventHistogram,
      webauthn::PasskeyUnlockManager::ErrorUIEventType::kAvatarUIDisplayed, 1);

  // Click the avatar button.
  Click(avatar);
  histogram_tester.ExpectBucketCount(
      kPasskeyUnlockErrorUiEventHistogram,
      webauthn::PasskeyUnlockManager::ErrorUIEventType::kAvatarButtonPressed,
      1);

  // Simulate the error disappearing.
  ON_CALL(*passkey_unlock_manager(), ShouldDisplayErrorUi())
      .WillByDefault(testing::Return(false));
  EXPECT_CALL(*passkey_unlock_manager(), ShouldDisplayErrorUi())
      .Times(testing::AtLeast(1));
  passkey_unlock_manager()->NotifyObserversForTesting();

  EXPECT_EQ(avatar->GetText(), std::u16string());
  histogram_tester.ExpectBucketCount(
      kPasskeyUnlockErrorUiEventHistogram,
      webauthn::PasskeyUnlockManager::ErrorUIEventType::kAvatarUIHidden, 1);
}
#endif  // !BUILDFLAG(IS_CHROMEOS)
