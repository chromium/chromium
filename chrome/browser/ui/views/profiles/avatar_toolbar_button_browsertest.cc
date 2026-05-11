// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"

#include <optional>
#include <string>
#include <string_view>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
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
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/signin/signin_promo_util.h"
#include "chrome/browser/signin/signin_ui_delegate.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/profiles/profile_colors_util.h"
#include "chrome/browser/ui/signin/dice_migration_service.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button_state_manager.h"
#include "chrome/browser/ui/views/profiles/profile_menu_coordinator.h"
#include "chrome/browser/ui/views/profiles/profile_menu_view_base.h"
#include "chrome/browser/ui/views/toolbar/avatar_toolbar_button_interface.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/toolbar/webui_test_utils.h"
#include "chrome/browser/ui/waap/initial_web_ui_manager.h"
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
#include "components/feature_engagement/public/feature_constants.h"
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
#include "device/fido/public/features.h"
#include "google_apis/gaia/core_account_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/mojom/themes.mojom.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/scoped_animation_duration_scale_mode.h"

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

// Test implementation that listens to the IdentityManager and adapts its state.
// This is useful to ensure reactions are part of the same chain of observers
// and allows to test more realistic scenarios.
// Note: this does not react to all scenarios yet - enrich as needed.
class TestSyncServiceWithIdentityManagerReaction
    : public syncer::TestSyncService,
      public signin::IdentityManager::Observer {
 public:
  explicit TestSyncServiceWithIdentityManagerReaction(
      signin::IdentityManager* identity_manager) {
    scoped_observation_.Observe(identity_manager);
  }

  //  signin::IdentityManager::Observer
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override {
    switch (event_details.GetEventTypeFor(signin::ConsentLevel::kSignin)) {
      case signin::PrimaryAccountChangeEvent::Type::kCleared:
        // Clear all potential temp states in `syncer::TestSyncService`.
        ClearSyncError();
        SetSignedOut();
        break;
      case signin::PrimaryAccountChangeEvent::Type::kSet:
      case signin::PrimaryAccountChangeEvent::Type::kNone:
        // No special treatment yet.
    }
  }

  void OnIdentityManagerShutdown(
      signin::IdentityManager* identity_manager) override {
    scoped_observation_.Reset();
  }

  void ClearSyncError() {
    SetTrustedVaultKeyRequired(false);
    FireStateChanged();
  }

 private:
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      scoped_observation_{this};
};

std::unique_ptr<KeyedService> TestingSyncFactoryFunction(
    signin::IdentityManager* identity_manager,
    content::BrowserContext* context) {
  return std::make_unique<TestSyncServiceWithIdentityManagerReaction>(
      identity_manager);
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
              (Profile*, const CoreAccountId&, signin_metrics::AccessPoint),
              (override));
};

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
class MockBatchUploadDelegate : public BatchUploadDelegate {
 public:
  MockBatchUploadDelegate() {
    // Make sure to simulate closing the dialog  when it opens (to ensure that
    // it happens before the browser closes). This is needed because the dialog
    // is never actually opened, and the browser is not aware of it. In
    // production `complete_callback` would be called in the destructor of the
    // dialog, which would clear the `BatchUploadService` resources tied to the
    // dialog. Since this a mock, we have to explicitly call
    // `complete_callback`, for simplicity we use an empty map, which simulates
    // a "Cancel" event.
    ON_CALL(*this, ShowBatchUploadDialog)
        .WillByDefault(
            [&](Browser* browser,
                const std::vector<syncer::LocalDataDescription>&
                    local_data_description_list,
                BatchUploadService::EntryPoint entry_point,
                BatchUploadSelectedDataTypeItemsCallback complete_callback) {
              std::move(complete_callback).Run({});
            });
  }

  MOCK_METHOD(void,
              ShowBatchUploadDialog,
              (Browser*,
               std::vector<syncer::LocalDataDescription>,
               BatchUploadService::EntryPoint,
               BatchUploadSelectedDataTypeItemsCallback),
              (override));
};
#endif

}  // namespace

class AvatarToolbarButtonInterfaceBaseBrowserTest {
 public:
  AvatarToolbarButtonInterfaceBaseBrowserTest()
      : dependency_manager_subscription_(
            BrowserContextDependencyManager::GetInstance()
                ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                    &AvatarToolbarButtonInterfaceBaseBrowserTest::
                        SetTestingFactories,
                    base::Unretained(this)))) {
    // By default make all delays infinite to avoid flakiness. The tests that
    // needs to test bypass the delay effects will have to enforce timing out
    // the delays using
    // `AvatarToolbarButtonInterface::ClearActiveStateForTesting()` or
    // StateProvider methods/events. This allows to properly test the behavior
    // pre/post delay without being time dependent.
    SetInfiniteAvatarDelay(AvatarDelayType::kNameGreeting);
    SetInfiniteAvatarDelay(AvatarDelayType::kOnSignin);
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
    SetInfiniteAvatarDelay(AvatarDelayType::kSigninPendingText);
    SetInfiniteAvatarDelay(AvatarDelayType::kPromo);
    SetInfiniteAvatarDelay(AvatarDelayType::kSignedOutPromo);
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
  }

  AvatarToolbarButtonInterfaceBaseBrowserTest(
      const AvatarToolbarButtonInterfaceBaseBrowserTest&) = delete;
  AvatarToolbarButtonInterfaceBaseBrowserTest& operator=(
      const AvatarToolbarButtonInterfaceBaseBrowserTest&) = delete;
  ~AvatarToolbarButtonInterfaceBaseBrowserTest() = default;

  AvatarToolbarButtonInterface* GetAvatarToolbarButtonInterface(
      Browser* browser) {
    return BrowserView::GetBrowserViewForBrowser(browser)
        ->toolbar_button_provider()
        ->GetAvatarToolbarButtonInterface();
  }

  virtual Browser* GetBrowser() const = 0;

  // Allows overriding the delay of different events that have a timing
  // duration. Sets the delay to infinite in order to be able to test the
  // behavior while the delay is happening. In order to clear the current state,
  // use `AvatarToolbarButtonInterface::ClearActiveStateForTesting()` at any
  // point.
  void SetInfiniteAvatarDelay(AvatarDelayType delay_type) {
    delay_resets_.push_back(
        AvatarToolbarButtonInterface::
            CreateScopedInfiniteDelayOverrideForTesting(delay_type));
  }

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  // Special override for the `AvatarDelayType::kSigninPendingText` delay to set
  // it to 0 given that the start time is stored as a ProfileUserData, which can
  // remain even if no browser exist. Setting it to 0 allows testing the
  // behavior where the delay is elapsed and then opening a new browser (while
  // no browser existed already).
  void SetZeroAvatarDelayForSigninPendingText() {
    delay_resets_.push_back(
        AvatarToolbarButtonInterface::
            CreateScopedZeroDelayOverrideSigninPendingTextForTesting());
  }
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

  // Returns the window count in avatar button text, if it exists.
  std::optional<int> GetWindowCountInAvatarButtonText(
      BrowserWindowInterface* browser) {
    const std::u16string button_text(
        AvatarToolbarButtonTestAccessor(browser).GetText());

    size_t before_number = button_text.find('(');
    if (before_number == std::u16string_view::npos) {
      return std::optional<int>();
    }

    size_t after_number = button_text.find(')');
    EXPECT_NE(std::u16string_view::npos, after_number);

    const std::u16string number_text =
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

  signin::IdentityManager* GetIdentityManager(Profile* profile) {
    return IdentityManagerFactory::GetForProfile(profile);
  }
  signin::IdentityManager* GetIdentityManager() {
    return GetIdentityManager(GetBrowser()->profile());
  }

  TestSyncServiceWithIdentityManagerReaction* GetTestSyncService() {
    return static_cast<TestSyncServiceWithIdentityManagerReaction*>(
        SyncServiceFactory::GetForProfile(GetBrowser()->profile()));
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
    gfx::Image current_avatar_icon =
        gfx::Image(AvatarToolbarButtonTestAccessor(GetBrowser())
                       .GetImage(views::Button::ButtonState::STATE_NORMAL));
    int icon_size = GetLayoutConstant(LayoutConstant::kToolbarButtonIconSize);
    gfx::Image adapted_signed_in_image = profiles::GetSizedAvatarIcon(
        account_image, icon_size, icon_size, profiles::SHAPE_CIRCLE);
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
      BrowserWindowInterface* browser,
      AvatarToolbarButtonInterface* avatar,
      const std::u16string& email,
      const std::u16string& name = u"account_name") {
    AccountInfo account_info = SigninWithImage(email, name);
    CHECK(!AvatarToolbarButtonTestAccessor(browser).GetText().empty());
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
        !syncer::IsReplaceSyncPromosWithSignInPromosEnabled()) {
      CHECK(!AvatarToolbarButtonTestAccessor(browser).GetText().empty());
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
  AccountInfo EnableSyncWithImageAndClearGreeting(
      AvatarToolbarButtonInterface* avatar,
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

  void ExpectSyncPaused(BrowserWindowInterface* browser) {
    EXPECT_EQ(AvatarToolbarButtonTestAccessor(browser).GetText(),
              l10n_util::GetStringUTF16(
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
    GetTestSyncService()->ClearSyncError();
  }

  void SimulateBookmarksLimitExceededError() {
    GetTestSyncService()->SetBookmarksLimitExceeded(true);
    GetTestSyncService()->FireStateChanged();
    ASSERT_EQ(
        GetTestSyncService()->GetUserActionableError(),
        syncer::SyncService::UserActionableError::kBookmarksLimitExceeded);
  }

  void ClearBookmarksLimitExceededError() {
    GetTestSyncService()->SetBookmarksLimitExceeded(false);
    GetTestSyncService()->FireStateChanged();
    ASSERT_NE(
        GetTestSyncService()->GetUserActionableError(),
        syncer::SyncService::UserActionableError::kBookmarksLimitExceeded);
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

  MockBatchUploadDelegate* mock_batch_upload_delegate() {
    return mock_batch_upload_delegate_;
  }

  // Sets up the needed information to give priortiy to `promo_type`.
  void SetupRequirementsForPromoType(
      signin::ProfileMenuAvatarButtonPromoInfo::Type promo_type) {
    // By default disable the preferences related to History sync to allow
    // History sync promos. Those are enabled by default in the
    // `syncer::TestSyncService`.
    SetHistoryAndTabsSyncingPreference(/*enable_sync=*/false);

    switch (promo_type) {
      case signin::ProfileMenuAvatarButtonPromoInfo::Type::kHistorySyncPromo:
        break;
      case signin::ProfileMenuAvatarButtonPromoInfo::Type::kBatchUploadPromo:
        // History Sync must be set to give priority to this promo.
        SetHistoryAndTabsSyncingPreference(/*enable_sync=*/true);
        // Set some local data.
        batch_upload_test_helper().SetReturnDescriptions(syncer::PASSWORDS,
                                                         /*item_count=*/5);
        break;
      case signin::ProfileMenuAvatarButtonPromoInfo::Type::
          kBatchUploadBookmarksPromo: {
        // Set local bookmarks.
        batch_upload_test_helper().SetReturnDescriptions(syncer::BOOKMARKS,
                                                         /*item_count=*/5);
        // The user must be previously syncing with the currently signed in
        // account.
        AccountInfo primary_account =
            GetIdentityManager()->FindExtendedAccountInfo(
                GetIdentityManager()->GetPrimaryAccountInfo(
                    signin::ConsentLevel::kSignin));
        CHECK(!primary_account.IsEmpty());
        GetBrowser()->profile()->GetPrefs()->SetString(
            prefs::kGoogleServicesLastSyncingGaiaId,
            primary_account.gaia.ToString());
        break;
      }
      case signin::ProfileMenuAvatarButtonPromoInfo::Type::
          kBatchUploadWindows10DepreciationPromo:
        CHECK(switches::IsSigninWindows10DepreciationState());
        // Set some local data.
        batch_upload_test_helper().SetReturnDescriptions(syncer::PASSWORDS,
                                                         /*item_count=*/5);
        break;
      case signin::ProfileMenuAvatarButtonPromoInfo::Type::kSyncPromo:
        CHECK(switches::IsAvatarSyncPromoFeatureEnabled());
        break;
      case signin::ProfileMenuAvatarButtonPromoInfo::Type::kSigninPromo:
        // TODO(crbug.com/486109449): Adapt the tests to support this promo.
        NOTREACHED() << "Test for this promo is not supported yet.";
    }
  }
#endif

 protected:
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  void ClearMockBatchUploadDelegate() { mock_batch_upload_delegate_ = nullptr; }
#endif

 private:
  void SetTestingFactories(content::BrowserContext* context) {
    SyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        context, base::BindRepeating(
                     &TestingSyncFactoryFunction,
                     GetIdentityManager(Profile::FromBrowserContext(context))));

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
    auto mock_batch_upload_delegate =
        std::make_unique<MockBatchUploadDelegate>();
    mock_batch_upload_delegate_ = mock_batch_upload_delegate.get();

    batch_upload_test_helper_.SetupBatchUploadTestingFactoryInProfile(
        Profile::FromBrowserContext(context), /*identity_manager=*/nullptr,
        std::move(mock_batch_upload_delegate));
#endif
  }

  base::CallbackListSubscription dependency_manager_subscription_;
  std::vector<base::AutoReset<std::optional<base::TimeDelta>>> delay_resets_;
  gfx::ScopedAnimationDurationScaleMode zero_duration_mode_ =
      gfx::ScopedAnimationDurationScaleMode(
          gfx::ScopedAnimationDurationScaleMode::ZERO_DURATION);

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  BatchUploadServiceTestHelper batch_upload_test_helper_;
  raw_ptr<MockBatchUploadDelegate> mock_batch_upload_delegate_ = nullptr;
#endif
};

class AvatarToolbarButtonBrowserTest
    : public InProcessBrowserTest,
      public AvatarToolbarButtonInterfaceBaseBrowserTest {
 protected:
  // AvatarToolbarButtonInterfaceBaseBrowserTest:
  Browser* GetBrowser() const override { return browser(); }

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    if (GetIdentityManager()) {
      // Puts `IdentityManager` in a known good state to avoid flakiness.
      signin::WaitForRefreshTokensLoaded(GetIdentityManager());
    }
  }

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  void TearDownOnMainThread() override {
    InProcessBrowserTest::TearDownOnMainThread();
    ClearMockBatchUploadDelegate();
  }
#endif
};

IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonBrowserTest, IncognitoWindowCount) {
  Profile* profile = browser()->profile();
  Browser* browser1 = CreateIncognitoBrowser(profile);
  AvatarToolbarButtonTestAccessor avatar_accessor1(browser1);
  EXPECT_TRUE(avatar_accessor1.GetEnabled());
  EXPECT_TRUE(avatar_accessor1.GetVisible());
  EXPECT_FALSE(GetWindowCountInAvatarButtonText(browser1).has_value());

  Browser* browser2 = CreateIncognitoBrowser(profile);
  EXPECT_EQ(2, *GetWindowCountInAvatarButtonText(browser1));
  EXPECT_EQ(2, *GetWindowCountInAvatarButtonText(browser2));

  CloseBrowserSynchronously(browser2);
  EXPECT_FALSE(GetWindowCountInAvatarButtonText(browser1).has_value());
}

#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonBrowserTest, GuestWindowCount) {
  Browser* browser1 = CreateGuestBrowser();
  AvatarToolbarButtonTestAccessor avatar_accessor1(browser1);
  EXPECT_TRUE(avatar_accessor1.GetEnabled());
  EXPECT_TRUE(avatar_accessor1.GetVisible());
  EXPECT_FALSE(GetWindowCountInAvatarButtonText(browser1).has_value());

  Browser* browser2 = CreateGuestBrowser();
  EXPECT_EQ(2, *GetWindowCountInAvatarButtonText(browser1));
  EXPECT_EQ(2, *GetWindowCountInAvatarButtonText(browser2));

  CloseBrowserSynchronously(browser2);
  EXPECT_FALSE(GetWindowCountInAvatarButtonText(browser1).has_value());
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

  EXPECT_TRUE(AvatarToolbarButtonTestAccessor(browser()).GetVisible());
  EXPECT_FALSE(AvatarToolbarButtonTestAccessor(browser()).GetEnabled());

  EXPECT_EQ(AvatarToolbarButtonTestAccessor(browser()).GetText(),
            l10n_util::GetPluralStringFUTF16(IDS_AVATAR_BUTTON_GUEST, 1));

  Browser* browser_2 = CreateBrowser(guest_profile);
  EXPECT_TRUE(AvatarToolbarButtonTestAccessor(browser_2).GetVisible());
  EXPECT_FALSE(AvatarToolbarButtonTestAccessor(browser_2).GetEnabled());

  // Browser count is not taken into consideration on purpose for Ash Guest
  // windows since the button is not enabled, both buttons still show the same
  // text as if it was a single window, which is different from other platforms.
  EXPECT_EQ(AvatarToolbarButtonTestAccessor(browser()).GetText(),
            l10n_util::GetPluralStringFUTF16(IDS_AVATAR_BUTTON_GUEST, 1));
  EXPECT_EQ(AvatarToolbarButtonTestAccessor(browser_2).GetText(),
            l10n_util::GetPluralStringFUTF16(IDS_AVATAR_BUTTON_GUEST, 1));
}
#endif

IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonBrowserTest, DefaultBrowser) {
  AvatarToolbarButtonTestAccessor avatar_accessor(browser());
#if BUILDFLAG(IS_CHROMEOS)
  // No avatar button is shown in normal Ash windows.
  EXPECT_FALSE(avatar_accessor.GetVisible());
#else
  EXPECT_TRUE(avatar_accessor.GetVisible());
  EXPECT_TRUE(avatar_accessor.GetEnabled());
#endif
}

IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonBrowserTest, IncognitoBrowser) {
  Browser* browser1 = CreateIncognitoBrowser(browser()->profile());
  AvatarToolbarButtonTestAccessor avatar_accessor1(browser1);
  // Incognito browsers always show an enabled avatar button.
  EXPECT_TRUE(avatar_accessor1.GetVisible());
  EXPECT_TRUE(avatar_accessor1.GetEnabled());
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
  AvatarToolbarButtonTestAccessor avatar_accessor1(browser1);
  // On ChromeOS, captive portal signin windows show a
  // disabled avatar button to indicate that the window is incognito.
  EXPECT_TRUE(avatar_accessor1.GetVisible());
  EXPECT_FALSE(avatar_accessor1.GetEnabled());
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
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{
            syncer::kReplaceSyncPromosWithSignInPromos,
            syncer::kReplaceSyncPromosWithSigninPromosNewSignin});
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
// Note: this oes not scale well with tests that expect `PRE_` in their main
// test definition.
#define TEST_WITH_SIGNED_IN_FROM_PRE(test_type, test_suite, test_name)       \
  test_type(test_suite, PRE_##test_name) {                                   \
    AvatarToolbarButtonInterface* avatar =                                   \
        GetAvatarToolbarButtonInterface(browser());                          \
    AvatarToolbarButtonTestAccessor avatar_accessor(browser());              \
    ASSERT_TRUE(avatar_accessor.GetText().empty());                          \
                                                                             \
    SigninWithImage(test_email(), test_given_name());                        \
    if (base::FeatureList::IsEnabled(                                        \
            syncer::kReplaceSyncPromosWithSignInPromos)) {                   \
      ASSERT_EQ(                                                             \
          avatar_accessor.GetText(),                                         \
          l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_MAKING_CHROME_YOURS)); \
      avatar->ClearActiveStateForTesting();                                  \
      EXPECT_TRUE(avatar_accessor.GetText().empty());                        \
    } else {                                                                 \
      ASSERT_EQ(avatar_accessor.GetText(),                                   \
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

  AvatarToolbarButtonInterface* avatar =
      GetAvatarToolbarButtonInterface(browser());
  AvatarToolbarButtonTestAccessor avatar_accessor(browser());
  ASSERT_EQ(avatar_accessor.GetText(),
            l10n_util::GetStringFUTF16(IDS_AVATAR_BUTTON_GREETING,
                                       test_given_name()));
  avatar->ClearActiveStateForTesting();

  ASSERT_EQ(avatar_accessor.GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SYNC_HISTORY));
  avatar->ClearActiveStateForTesting();

  // Once the greeting and promo are not shown anymore, we expect no text.
  EXPECT_EQ(avatar_accessor.GetText(), std::u16string());
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
  AvatarToolbarButtonInterface* avatar =
      GetAvatarToolbarButtonInterface(browser());
  AvatarToolbarButtonTestAccessor avatar_accessor(browser());
  // Normal state.
  ASSERT_TRUE(avatar_accessor.GetText().empty());

  std::u16string email(u"test@gmail.com");
  std::u16string name(u"TestName");
  AccountInfo account_info = EnableSync(email, name);
  // The button is in a waiting for image state, the name is not yet displayed.
  EXPECT_EQ(avatar_accessor.GetText(), std::u16string());

  // The greeting will only show when the image is loaded.
  AddSignedInImage(account_info.account_id);
  EXPECT_EQ(avatar_accessor.GetText(),
            l10n_util::GetStringFUTF16(IDS_AVATAR_BUTTON_GREETING, name));

  avatar->ClearActiveStateForTesting();
  // Once the name is not shown anymore, we expect no text.
  EXPECT_EQ(avatar_accessor.GetText(), std::u16string());
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

  AvatarToolbarButtonTestAccessor avatar_accessor(browser());
  EXPECT_EQ(avatar_accessor.GetText(),
            l10n_util::GetStringFUTF16(IDS_AVATAR_BUTTON_GREETING,
                                       test_given_name()));

  // Creating a new browser while the refresh tokens are already loaded and the
  // name showing should not break/crash.
  Browser* new_browser = CreateBrowser(browser()->profile());
  AvatarToolbarButtonTestAccessor new_avatar_accessor(new_browser);
  // Name is expected to be shown while it is still shown on the first browser.
  ASSERT_EQ(avatar_accessor.GetText(),
            l10n_util::GetStringFUTF16(IDS_AVATAR_BUTTON_GREETING,
                                       test_given_name()));
  EXPECT_EQ(new_avatar_accessor.GetText(),
            l10n_util::GetStringFUTF16(IDS_AVATAR_BUTTON_GREETING,
                                       test_given_name()));
}

IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonWithSyncBrowserTest, SyncPaused) {
  AvatarToolbarButtonInterface* avatar_button =
      GetAvatarToolbarButtonInterface(browser());
  AvatarToolbarButtonTestAccessor avatar_accessor(browser());
  // Normal state.
  ASSERT_TRUE(avatar_accessor.GetText().empty());

  AccountInfo account_info =
      EnableSyncWithImageAndClearGreeting(avatar_button, u"test@gmail.com");
  SimulateSyncPaused();
  ExpectSyncPaused(browser());

  ClearSyncPaused();
  EXPECT_EQ(avatar_accessor.GetText(), std::u16string());
}

// Checks that "Sync paused" has higher priority than passphrase errors.
// Regression test for https://crbug.com/368997513
IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonWithSyncBrowserTest,
                       SyncPausedWithPassphraseError) {
  AvatarToolbarButtonInterface* avatar_button =
      GetAvatarToolbarButtonInterface(browser());
  AvatarToolbarButtonTestAccessor avatar_accessor(browser());
  ASSERT_TRUE(avatar_accessor.GetText().empty());

  AccountInfo account_info =
      EnableSyncWithImageAndClearGreeting(avatar_button, u"test@gmail.com");
  SimulatePassphraseError();
  SimulateSyncPaused();
  ExpectSyncPaused(browser());
}

#if !BUILDFLAG(IS_CHROMEOS)
// Checks that "Signin pending" has higher priority than passphrase errors.
// Adapted regression test for https://crbug.com/368997513
IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonBrowserTest,
                       SigninPendingWithPassphraseError) {
  AvatarToolbarButtonInterface* avatar_button =
      GetAvatarToolbarButtonInterface(browser());
  AvatarToolbarButtonTestAccessor avatar_accessor(browser());
  ASSERT_TRUE(avatar_accessor.GetText().empty());

  AccountInfo account_info = SigninWithImageAndClearGreetingAndSyncPromo(
      browser(), avatar_button, u"test@gmail.com");
  SimulatePassphraseError();
  SimulateSigninPending(/*web_sign_out=*/false);
  EXPECT_EQ(avatar_accessor.GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SIGNIN_PAUSED));
}
#endif

IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonWithSyncBrowserTest, SyncError) {
  AvatarToolbarButtonInterface* avatar_button =
      GetAvatarToolbarButtonInterface(browser());
  AvatarToolbarButtonTestAccessor avatar_accessor(browser());
  // Normal state.
  ASSERT_TRUE(avatar_accessor.GetText().empty());

  EnableSyncWithImageAndClearGreeting(avatar_button, u"test@gmail.com");
  SimulateSyncError();
  EXPECT_EQ(avatar_accessor.GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SYNC_ERROR));

  ClearSyncError();
  EXPECT_EQ(avatar_accessor.GetText(), std::u16string());
}

IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonWithSyncBrowserTest,
                       BookmarksLimitExceededErrorForSyncingUser) {
  AvatarToolbarButtonInterface* avatar_button =
      GetAvatarToolbarButtonInterface(browser());
  AvatarToolbarButtonTestAccessor avatar_accessor(browser());
  // Normal state.
  ASSERT_TRUE(avatar_accessor.GetText().empty());

  EnableSyncWithImageAndClearGreeting(avatar_button, u"test@gmail.com");
  SimulateBookmarksLimitExceededError();
  EXPECT_EQ(avatar_accessor.GetText(),
            l10n_util::GetStringUTF16(
                IDS_AVATAR_BUTTON_SYNC_ERROR_BOOKMARKS_LIMIT_EXCEEDED));

  ClearBookmarksLimitExceededError();
  EXPECT_TRUE(avatar_accessor.GetText().empty());
}

// Avatar button is not shown on Ash. No need to perform those tests as the info
// checked might not be adapted.
#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonWithSyncBrowserTest,
                       BookmarksLimitExceededErrorOpensProfileMenu) {
  AvatarToolbarButtonInterface* avatar_button =
      GetAvatarToolbarButtonInterface(browser());
  EnableSyncWithImageAndClearGreeting(avatar_button, u"test@gmail.com");
  SimulateBookmarksLimitExceededError();

  EXPECT_FALSE(
      browser()->GetFeatures().profile_menu_coordinator()->IsShowing());
  avatar_button->ButtonPressed(/*is_source_accelerator=*/false);
  // TODO(crbug.com/478780706) Verifying the presence and functionality of error
  // cards within the profile menu is not easily testable. Consider implementing
  // a test harness for this purpose.
  EXPECT_TRUE(browser()->GetFeatures().profile_menu_coordinator()->IsShowing());
}
#endif

IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonBrowserTest,
                       BookmarksLimitExceededErrorForSignedInUser) {
  AvatarToolbarButtonInterface* avatar_button =
      GetAvatarToolbarButtonInterface(browser());
  AvatarToolbarButtonTestAccessor avatar_accessor(browser());
  // Normal state.
  ASSERT_TRUE(avatar_accessor.GetText().empty());

  SigninWithImageAndClearGreetingAndSyncPromo(browser(), avatar_button,
                                              u"test@gmail.com");
  SimulateBookmarksLimitExceededError();
  EXPECT_EQ(avatar_accessor.GetText(),
            l10n_util::GetStringUTF16(
                IDS_AVATAR_BUTTON_SYNC_ERROR_BOOKMARKS_LIMIT_EXCEEDED));

  ClearBookmarksLimitExceededError();
  EXPECT_TRUE(avatar_accessor.GetText().empty());
}

IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonWithSyncBrowserTest,
                       SyncPausedThenExplicitText) {
  AvatarToolbarButtonInterface* avatar_button =
      GetAvatarToolbarButtonInterface(browser());
  AvatarToolbarButtonTestAccessor avatar_accessor(browser());
  // Normal state.
  ASSERT_TRUE(avatar_accessor.GetText().empty());

  EnableSyncWithImageAndClearGreeting(avatar_button, u"test@gmail.com");
  SimulateSyncPaused();
  ExpectSyncPaused(browser());

  std::u16string profile_switch_text(u"Profile Switch?");
  base::ScopedClosureRunner hide_callback =
      avatar_button->SetExplicitButtonState(
          profile_switch_text, /*accessibility_label=*/std::nullopt,
          /*explicit_action=*/std::nullopt);
  EXPECT_EQ(avatar_accessor.GetText(), profile_switch_text);

  // Clearing explicit text should go back to Sync Pause.
  hide_callback.RunAndReset();
  ExpectSyncPaused(browser());
}

// Explicit text over sync paused/error.
IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonWithSyncBrowserTest,
                       ExplicitTextThenSyncPaused) {
  AvatarToolbarButtonInterface* avatar_button =
      GetAvatarToolbarButtonInterface(browser());
  AvatarToolbarButtonTestAccessor avatar_accessor(browser());
  // Normal state.
  ASSERT_TRUE(avatar_accessor.GetText().empty());

  EnableSyncWithImageAndClearGreeting(avatar_button, u"test@gmail.com");
  std::u16string profile_switch_text(u"Profile Switch?");
  base::ScopedClosureRunner hide_callback =
      avatar_button->SetExplicitButtonState(
          profile_switch_text, /*accessibility_label=*/std::nullopt,
          /*explicit_action=*/std::nullopt);
  EXPECT_EQ(avatar_accessor.GetText(), profile_switch_text);

  SimulateSyncPaused();
  // Explicit text should still be shown even if Sync is now Paused.
  EXPECT_EQ(avatar_accessor.GetText(), profile_switch_text);

  // Clearing explicit text should go back to Sync Pause.
  hide_callback.RunAndReset();
  ExpectSyncPaused(browser());
}

IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonBrowserTest,
                       ShowExplicitTextAndHide) {
  AvatarToolbarButtonInterface* avatar =
      GetAvatarToolbarButtonInterface(browser());
  AvatarToolbarButtonTestAccessor avatar_accessor(browser());
  ASSERT_EQ(avatar_accessor.GetText(), std::u16string());

  std::u16string new_text(u"Some New Text");
  base::ScopedClosureRunner hide_callback = avatar->SetExplicitButtonState(
      new_text, /*accessibility_label=*/std::nullopt,
      /*explicit_action=*/std::nullopt);
  EXPECT_EQ(avatar_accessor.GetText(), new_text);
  hide_callback.RunAndReset();
  EXPECT_EQ(avatar_accessor.GetText(), std::u16string());
}

IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonBrowserTest,
                       ShowExplicitTextAndDefaultHide) {
  AvatarToolbarButtonInterface* avatar =
      GetAvatarToolbarButtonInterface(browser());
  AvatarToolbarButtonTestAccessor avatar_accessor(browser());
  ASSERT_EQ(avatar_accessor.GetText(), std::u16string());

  // Simulates a stack that enforces the change of text, but never explicitly
  // call the hide callback. It should still be done on explicitly destroying
  // the caller.
  {
    std::u16string new_text(u"Some New Text");
    base::ScopedClosureRunner hide_callback = avatar->SetExplicitButtonState(
        new_text, /*accessibility_label=*/std::nullopt,
        /*explicit_action=*/std::nullopt);
    EXPECT_EQ(avatar_accessor.GetText(), new_text);
  }

  EXPECT_EQ(avatar_accessor.GetText(), std::u16string());
}

IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonBrowserTest,
                       ShowExplicitTextWithExplicitAction) {
  AvatarToolbarButtonInterface* avatar =
      GetAvatarToolbarButtonInterface(browser());
  AvatarToolbarButtonTestAccessor avatar_accessor(browser());
  ASSERT_EQ(avatar_accessor.GetText(), std::u16string());
  ASSERT_FALSE(avatar->HasExplicitButtonState());

  const std::u16string text_1(u"Some New Text 1");
  base::MockCallback<base::RepeatingCallback<void(bool)>> mock_callback_1;
  base::ScopedClosureRunner reset_callback_1 = avatar->SetExplicitButtonState(
      text_1, /*accessibility_label=*/std::nullopt, mock_callback_1.Get());
  EXPECT_EQ(avatar_accessor.GetText(), text_1);
  EXPECT_TRUE(avatar->HasExplicitButtonState());
  EXPECT_CALL(mock_callback_1, Run).Times(1);
  avatar->ButtonPressed(/*is_source_accelerator=*/false);

  const std::u16string text_2(u"Some New Text 2");
  base::MockCallback<base::RepeatingCallback<void(bool)>> mock_callback_2;
  base::ScopedClosureRunner reset_callback_2 = avatar->SetExplicitButtonState(
      text_2, /*accessibility_label=*/std::nullopt, mock_callback_2.Get());
  EXPECT_EQ(avatar_accessor.GetText(), text_2);
  EXPECT_TRUE(avatar->HasExplicitButtonState());
  EXPECT_CALL(mock_callback_2, Run).Times(1);
  avatar->ButtonPressed(/*is_source_accelerator=*/false);

  // Calling the first reset callback should do nothing after the second call
  // to `SetExplicitButtonState`.
  reset_callback_1.RunAndReset();
  EXPECT_EQ(avatar_accessor.GetText(), text_2);
  EXPECT_TRUE(avatar->HasExplicitButtonState());

  // Calling the second reset callback should reset the text and the action.
  reset_callback_2.RunAndReset();
  EXPECT_EQ(avatar_accessor.GetText(), std::u16string());
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
  AvatarToolbarButtonInterface* avatar =
      GetAvatarToolbarButtonInterface(browser());
  AvatarToolbarButtonTestAccessor avatar_accessor(browser());
  ASSERT_EQ(avatar_accessor.GetText(), std::u16string());

  const std::u16string account_name(u"Account name");
  AccountInfo account_info = Signin(u"test@gmail.com", account_name);

  AddSignedInImage(account_info.account_id);

  EXPECT_EQ(avatar_accessor.GetRenderedTooltipText(gfx::Point()), account_name);

  avatar->ClearActiveStateForTesting();

  // Tooltip is the same after hiding the name.
  EXPECT_EQ(avatar_accessor.GetRenderedTooltipText(gfx::Point()), account_name);
}

// TODO(b/331746545): Check flaky test issue on windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_EnableSyncWithSyncDisabled DISABLED_EnableSyncWithSyncDisabled
#else
#define MAYBE_EnableSyncWithSyncDisabled EnableSyncWithSyncDisabled
#endif
IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonBrowserTest,
                       MAYBE_EnableSyncWithSyncDisabled) {
  AvatarToolbarButtonInterface* avatar =
      GetAvatarToolbarButtonInterface(browser());
  AvatarToolbarButtonTestAccessor avatar_accessor(browser());
  ASSERT_EQ(avatar_accessor.GetText(), std::u16string());

  EnableSyncWithImageAndClearGreeting(avatar, u"test@gmail.com");
  EXPECT_EQ(avatar_accessor.GetText(), std::u16string());

  SimulateDisableSyncByPolicyWithError();

  EXPECT_EQ(avatar_accessor.GetText(), std::u16string());

  Browser* new_browser = CreateBrowser(browser()->profile());
  EXPECT_EQ(AvatarToolbarButtonTestAccessor(new_browser).GetText(),
            std::u16string());
}

#endif

#if BUILDFLAG(ENABLE_DICE_SUPPORT)

IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonBrowserTest, SigninWithSyncError) {
  AvatarToolbarButtonInterface* avatar_button =
      GetAvatarToolbarButtonInterface(browser());
  AvatarToolbarButtonTestAccessor avatar_accessor(browser());
  // Normal state.
  ASSERT_TRUE(avatar_accessor.GetText().empty());

  SigninWithImageAndClearGreetingAndSyncPromo(browser(), avatar_button,
                                              u"test@gmail.com");
  SimulateSyncError();
  EXPECT_EQ(avatar_accessor.GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SIGNIN_PAUSED));

  ClearSyncError();
  EXPECT_EQ(avatar_accessor.GetText(), std::u16string());
}

// Regression test for crbug.com/500671552.
IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonBrowserTest,
                       SyncErrorWithPromoThenSignout) {
  AvatarToolbarButtonInterface* avatar_button =
      GetAvatarToolbarButtonInterface(browser());
  AvatarToolbarButtonTestAccessor avatar_accessor(browser());
  // Normal state.
  ASSERT_TRUE(avatar_accessor.GetText().empty());

  SigninWithImageAndClearGreetingAndSyncPromo(browser(), avatar_button,
                                              u"test@gmail.com");
  SimulateSyncError();
  EXPECT_EQ(avatar_accessor.GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SIGNIN_PAUSED));

  // Activates the promo state provider after already showing a higher priority
  // state. This scenario is actually possible on startup.
  SetupRequirementsForPromoType(
      signin::ProfileMenuAvatarButtonPromoInfo::Type::kHistorySyncPromo);
  avatar_button->ForceShowingPromoForTesting();
  // Sync Error still has priority and is showing.
  EXPECT_EQ(avatar_accessor.GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SIGNIN_PAUSED));

  // This will also clear the Sync Error through
  // `TestSyncServiceWithIdentityManagerReaction::OnPrimaryAccountChanged()`.
  Signout();
  EXPECT_EQ(avatar_accessor.GetText(), std::u16string());
}

IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonBrowserTest,
                       SigninPendingThenExplicitText) {
  AvatarToolbarButtonInterface* avatar_button =
      GetAvatarToolbarButtonInterface(browser());
  AvatarToolbarButtonTestAccessor avatar_accessor(browser());
  // Normal state.
  ASSERT_TRUE(avatar_accessor.GetText().empty());

  SigninWithImageAndClearGreetingAndSyncPromo(browser(), avatar_button,
                                              u"test@gmail.com");
  SimulateSigninPending(/*web_sign_out=*/false);
  ASSERT_EQ(avatar_accessor.GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SIGNIN_PAUSED));

  const std::u16string profile_switch_text(u"Profile Switch?");
  base::ScopedClosureRunner hide_callback =
      avatar_button->SetExplicitButtonState(
          profile_switch_text, /*accessibility_label=*/std::nullopt,
          /*explicit_action=*/std::nullopt);
  ASSERT_EQ(avatar_accessor.GetText(), profile_switch_text);

  // Clearing explicit text should go back to Signin Pending.
  hide_callback.RunAndReset();
  EXPECT_EQ(avatar_accessor.GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SIGNIN_PAUSED));
}

// Explicit text over signin pending.
IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonBrowserTest,
                       ExplicitTextThenSigninPending) {
  AvatarToolbarButtonInterface* avatar_button =
      GetAvatarToolbarButtonInterface(browser());
  AvatarToolbarButtonTestAccessor avatar_accessor(browser());
  // Normal state.
  ASSERT_TRUE(avatar_accessor.GetText().empty());

  SigninWithImageAndClearGreetingAndSyncPromo(browser(), avatar_button,
                                              u"test@gmail.com");
  const std::u16string profile_switch_text(u"Profile Switch?");
  base::ScopedClosureRunner hide_callback =
      avatar_button->SetExplicitButtonState(
          profile_switch_text, /*accessibility_label=*/std::nullopt,
          /*explicit_action=*/std::nullopt);
  ASSERT_EQ(avatar_accessor.GetText(), profile_switch_text);

  SimulateSigninPending(/*web_sign_out=*/false);
  // Explicit text should still be shown even if Signin Pending.
  ASSERT_EQ(avatar_accessor.GetText(), profile_switch_text);

  // Clearing explicit text should go back to Signin Pending.
  hide_callback.RunAndReset();
  EXPECT_EQ(avatar_accessor.GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SIGNIN_PAUSED));
}

IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonBrowserTest,
                       PRE_SigninPendingFromWebSignoutThenRestartChrome) {
  AvatarToolbarButtonInterface* avatar =
      GetAvatarToolbarButtonInterface(browser());
  SigninWithImageAndClearGreetingAndSyncPromo(browser(), avatar, test_email(),
                                              test_given_name());
  SimulateSigninPending(/*web_sign_out=*/true);
  AvatarToolbarButtonTestAccessor avatar_accessor(browser());
  ASSERT_EQ(avatar_accessor.GetText(), std::u16string());
}

IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonBrowserTest,
                       SigninPendingFromWebSignoutThenRestartChrome) {
  AvatarToolbarButtonInterface* avatar =
      GetAvatarToolbarButtonInterface(browser());
  AvatarToolbarButtonTestAccessor avatar_accessor(browser());
  // The greetings are shown after the restart.
  EXPECT_EQ(avatar_accessor.GetText(),
            l10n_util::GetStringFUTF16(IDS_AVATAR_BUTTON_GREETING,
                                       test_given_name()));

  avatar->ClearActiveStateForTesting();
  // The error text is expected to be shown even if the error delay has not
  // reached yet.
  EXPECT_EQ(avatar_accessor.GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SIGNIN_PAUSED));
}

// Regression test for https://crbug.com/348587566
IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonBrowserTest,
                       SigninPendingDelayEndedNoBrowser) {
  ASSERT_EQ(1u, GlobalBrowserCollection::GetInstance()->GetSize());
  Profile* profile = browser()->profile();
  {
    AvatarToolbarButtonInterface* avatar =
        GetAvatarToolbarButtonInterface(browser());
    AvatarToolbarButtonTestAccessor avatar_accessor(browser());

    SigninWithImageAndClearGreetingAndSyncPromo(browser(), avatar,
                                                u"test@gmail.com", u"TestName");
    SimulateSigninPending(/*web_sign_out=*/true);
    ASSERT_TRUE(avatar_accessor.GetText().empty());

    // Close the browser before the delay ends, but keep the profile and Chrome
    // alive by opening an incognito browser.
    CreateIncognitoBrowser(profile);
  }
  CloseBrowserSynchronously(browser());

  // This simulates the delay expiry for the next browser. Instead of advancing
  // time, we set the expected delay to 0, making the elapsed time greater than
  // the delay for sure - simulating the delay expiry.
  SetZeroAvatarDelayForSigninPendingText();

  // Open a new browser, this should not crash.
  Browser* new_browser = CreateBrowser(profile);
  EXPECT_EQ(AvatarToolbarButtonTestAccessor(new_browser).GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SIGNIN_PAUSED));
}

#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

class AvatarToolbarButtonWithInteractiveFeaturePromoBrowserTest
    : public InteractiveFeaturePromoTest,
      public AvatarToolbarButtonInterfaceBaseBrowserTest {
 protected:
  AvatarToolbarButtonWithInteractiveFeaturePromoBrowserTest()
      : InteractiveFeaturePromoTest(UseDefaultTrackerAllowingPromos({})) {}

  // AvatarToolbarButtonInterfaceBaseBrowserTest:
  Browser* GetBrowser() const override { return browser(); }

  // InteractiveFeaturePromoTest:
  void SetUpOnMainThread() override {
    InteractiveFeaturePromoTest::SetUpOnMainThread();
    if (GetIdentityManager()) {
      // Puts `IdentityManager` in a known good state to avoid flakiness.
      signin::WaitForRefreshTokensLoaded(GetIdentityManager());
    }
  }

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  void TearDownOnMainThread() override {
    InProcessBrowserTest::TearDownOnMainThread();
    ClearMockBatchUploadDelegate();
  }
#endif
};

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// The tests relying on this base class can test the different Promos to be
// shown on the AvatarToolbarButton, more specifically the promo types in
// `signin::ProfileMenuAvatarButtonPromoInfo::Type`.
// One special scenario is when testing the SyncPromo (related to
// `switches::IsAvatarSyncPromoFeatureEnabled()`), it is important to ensure
// that specific feature flags are not enabled at the same time, since some
// features are not compatible (SyncPromo have a higher priority than
// HistorySync), this is handled in the constructor.
// TODO(crbug.com/331746545): Check the flaky test suite issue on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_AvatarToolbarButtonPromoBrowserTest \
  DISABLED_AvatarToolbarButtonPromoBrowserTest
#else
#define MAYBE_AvatarToolbarButtonPromoBrowserTest \
  AvatarToolbarButtonPromoBrowserTest
#endif
class MAYBE_AvatarToolbarButtonPromoBrowserTest
    : public AvatarToolbarButtonWithInteractiveFeaturePromoBrowserTest,
      public testing::WithParamInterface<
          signin::ProfileMenuAvatarButtonPromoInfo::Type> {
 protected:
  MAYBE_AvatarToolbarButtonPromoBrowserTest() {
    switch (GetAvatarPromoType()) {
      case signin::ProfileMenuAvatarButtonPromoInfo::Type::kHistorySyncPromo:
      case signin::ProfileMenuAvatarButtonPromoInfo::Type::kBatchUploadPromo:
      case signin::ProfileMenuAvatarButtonPromoInfo::Type::
          kBatchUploadBookmarksPromo:
        feature_list_.InitWithFeatureStates(
            {{syncer::kReplaceSyncPromosWithSignInPromos, true},
             {switches::kAvatarButtonSyncPromoForTesting, false},
             // Ensure to ignore the feature for Windows 10 bots not indirectly
             // triggering it.
             {switches::kSigninWindows10DepreciationStateBypassForTesting,
              true}});
        break;
      case signin::ProfileMenuAvatarButtonPromoInfo::Type::
          kBatchUploadWindows10DepreciationPromo:
        feature_list_.InitWithFeatureStates(
            {{syncer::kReplaceSyncPromosWithSignInPromos, true},
             {switches::kAvatarButtonSyncPromoForTesting, false},
             // Ensure to force the feature for testing the promo type.
             {switches::kSigninWindows10DepreciationStateForTesting, true}});
        break;
      case signin::ProfileMenuAvatarButtonPromoInfo::Type::kSyncPromo:
        feature_list_.InitWithFeatureStates(
            // `enable_replace_sync_with_signin` is ignored.
            {{syncer::kReplaceSyncPromosWithSignInPromos, false},
             {syncer::kReplaceSyncPromosWithSigninPromosNewSignin, false},
             {switches::kAvatarButtonSyncPromoForTesting, true}});
        break;
      case signin::ProfileMenuAvatarButtonPromoInfo::Type::kSigninPromo:
        // TODO(crbug.com/486109449): Adapt the tests to support this promo.
        NOTREACHED() << "Test for this promo is not supported yet.";
    }
  }

  signin::ProfileMenuAvatarButtonPromoInfo::Type GetAvatarPromoType() {
    return GetParam();
  }

  std::u16string GetExpectedPromoText() {
    switch (GetAvatarPromoType()) {
      case signin::ProfileMenuAvatarButtonPromoInfo::Type::kHistorySyncPromo:
        return l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SYNC_HISTORY);
      case signin::ProfileMenuAvatarButtonPromoInfo::Type::kBatchUploadPromo:
        return l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_BATCH_UPLOAD_PROMO);
      case signin::ProfileMenuAvatarButtonPromoInfo::Type::
          kBatchUploadBookmarksPromo:
        return l10n_util::GetStringUTF16(
            IDS_AVATAR_BUTTON_BATCH_UPLOAD_PROMO_WITH_BOOKMARK_CLEANUP_PROMO);
      case signin::ProfileMenuAvatarButtonPromoInfo::Type::
          kBatchUploadWindows10DepreciationPromo:
        // This string does not mention "Sync".
        return l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SYNC_PROMO);
      case signin::ProfileMenuAvatarButtonPromoInfo::Type::kSyncPromo:
        return l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SYNC_PROMO);
      case signin::ProfileMenuAvatarButtonPromoInfo::Type::kSigninPromo:
        // TODO(crbug.com/486109449): Adapt the tests to support this promo.
        NOTREACHED() << "Test for this promo is not supported yet.";
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(MAYBE_AvatarToolbarButtonPromoBrowserTest,
                       PRE_PromoNotShownIfGreetingNotShown) {
  AvatarToolbarButtonTestAccessor avatar_accessor(browser());
  // Explicitly sign in without an image.
  Signin(test_email(), test_given_name());
  switch (GetAvatarPromoType()) {
    case signin::ProfileMenuAvatarButtonPromoInfo::Type::kHistorySyncPromo:
    case signin::ProfileMenuAvatarButtonPromoInfo::Type::kBatchUploadPromo:
    case signin::ProfileMenuAvatarButtonPromoInfo::Type::
        kBatchUploadBookmarksPromo:
    case signin::ProfileMenuAvatarButtonPromoInfo::Type::
        kBatchUploadWindows10DepreciationPromo:
      EXPECT_EQ(
          avatar_accessor.GetText(),
          l10n_util ::GetStringUTF16(IDS_AVATAR_BUTTON_MAKING_CHROME_YOURS));
      break;
    case signin::ProfileMenuAvatarButtonPromoInfo::Type::kSyncPromo:
      EXPECT_EQ(avatar_accessor.GetText(), std::u16string());
      break;
    case signin::ProfileMenuAvatarButtonPromoInfo::Type::kSigninPromo:
      // TODO(crbug.com/486109449): Adapt the tests to support this promo.
      NOTREACHED() << "Test for this promo is not supported yet.";
  }
}

IN_PROC_BROWSER_TEST_P(MAYBE_AvatarToolbarButtonPromoBrowserTest,
                       PromoNotShownIfGreetingNotShown) {
  ASSERT_TRUE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));

  AvatarToolbarButtonInterface* avatar =
      GetAvatarToolbarButtonInterface(browser());
  AvatarToolbarButtonTestAccessor avatar_accessor(browser());
  // The button is in a waiting for image state, the greeting is not yet
  // displayed, hence the promo should not be shown.
  EXPECT_EQ(avatar_accessor.GetText(), std::u16string());

  SetupRequirementsForPromoType(GetAvatarPromoType());

  // Only after adding the image that the greeting is shown.
  AddSignedInImage(
      GetIdentityManager()->GetPrimaryAccountId(signin::ConsentLevel::kSignin));
  ASSERT_EQ(avatar_accessor.GetText(),
            l10n_util::GetStringFUTF16(IDS_AVATAR_BUTTON_GREETING,
                                       test_given_name()));
  avatar->ClearActiveStateForTesting();

  // Then the promo.
  ASSERT_EQ(avatar_accessor.GetText(), GetExpectedPromoText());
  avatar->ClearActiveStateForTesting();

  // Then normal state.
  EXPECT_EQ(avatar_accessor.GetText(), std::u16string());
}

TEST_WITH_SIGNED_IN_FROM_PRE(IN_PROC_BROWSER_TEST_P,
                             MAYBE_AvatarToolbarButtonPromoBrowserTest,
                             NoPromoShownUntilSyncServiceIsInitialized) {
  SetupRequirementsForPromoType(GetAvatarPromoType());

  SetSyncServiceTransportState(
      syncer::SyncService::TransportState::INITIALIZING);

  AvatarToolbarButtonInterface* avatar =
      GetAvatarToolbarButtonInterface(browser());
  AvatarToolbarButtonTestAccessor avatar_accessor(browser());
  ASSERT_EQ(avatar_accessor.GetText(),
            l10n_util::GetStringFUTF16(IDS_AVATAR_BUTTON_GREETING,
                                       test_given_name()));
  avatar->ClearActiveStateForTesting();
  // No Promo shown as long as the sync service is not active.
  ASSERT_EQ(avatar_accessor.GetText(), std::u16string());

  // Check crbug.com/454927990.
  SetSyncServiceTransportState(
      syncer::SyncService::TransportState::CONFIGURING);
  // No Promo shown as long as the sync service is not active.
  ASSERT_EQ(avatar_accessor.GetText(), std::u16string());

  SetSyncServiceTransportState(syncer::SyncService::TransportState::ACTIVE);
  ASSERT_EQ(avatar_accessor.GetText(), GetExpectedPromoText());
  avatar->ClearActiveStateForTesting();

  // Once the greeting and promo are not shown anymore, we expect no text.
  EXPECT_EQ(avatar_accessor.GetText(), std::u16string());
}

TEST_WITH_SIGNED_IN_FROM_PRE(IN_PROC_BROWSER_TEST_P,
                             MAYBE_AvatarToolbarButtonPromoBrowserTest,
                             PromoThenSyncError) {
  SetupRequirementsForPromoType(GetAvatarPromoType());

  AvatarToolbarButtonInterface* avatar =
      GetAvatarToolbarButtonInterface(browser());
  AvatarToolbarButtonTestAccessor avatar_accessor(browser());
  ASSERT_EQ(avatar_accessor.GetText(),
            l10n_util::GetStringFUTF16(IDS_AVATAR_BUTTON_GREETING,
                                       test_given_name()));
  avatar->ClearActiveStateForTesting();

  EXPECT_EQ(avatar_accessor.GetText(), GetExpectedPromoText());

  SimulateSyncError();
  // The sync error should be shown.
  EXPECT_EQ(avatar_accessor.GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SIGNIN_PAUSED));
  ClearSyncError();
  // After clearing the sync error, the promo should NOT be shown anymore.
  EXPECT_TRUE(avatar_accessor.GetText().empty());
}

#if !BUILDFLAG(IS_LINUX)
TEST_WITH_SIGNED_IN_FROM_PRE(IN_PROC_BROWSER_TEST_P,
                             MAYBE_AvatarToolbarButtonPromoBrowserTest,
                             PromoNotShownWhenPromotionsDisabled) {
  SetupRequirementsForPromoType(GetAvatarPromoType());

  TestingBrowserProcess::GetGlobal()->local_state()->SetBoolean(
      prefs::kPromotionsEnabled, false);
  AvatarToolbarButtonInterface* avatar =
      GetAvatarToolbarButtonInterface(browser());
  AvatarToolbarButtonTestAccessor avatar_accessor(browser());
  ASSERT_EQ(avatar_accessor.GetText(),
            l10n_util::GetStringFUTF16(IDS_AVATAR_BUTTON_GREETING,
                                       test_given_name()));
  avatar->ClearActiveStateForTesting();
  // The greeting should NOT be followed by any promo if promotions are
  // disabled.
  EXPECT_TRUE(avatar_accessor.GetText().empty());
}
#endif

TEST_WITH_SIGNED_IN_FROM_PRE(IN_PROC_BROWSER_TEST_P,
                             MAYBE_AvatarToolbarButtonPromoBrowserTest,
                             PromoNotShownWhenSyncNotAllowed) {
  SetupRequirementsForPromoType(GetAvatarPromoType());
  SimulateDisableSyncByPolicyWithError();

  AvatarToolbarButtonInterface* avatar =
      GetAvatarToolbarButtonInterface(browser());
  AvatarToolbarButtonTestAccessor avatar_accessor(browser());
  // Normal state.
  ASSERT_EQ(avatar_accessor.GetText(),
            l10n_util::GetStringFUTF16(IDS_AVATAR_BUTTON_GREETING,
                                       test_given_name()));
  avatar->ClearActiveStateForTesting();
  // The greeting should NOT be followed by the promo if sync is not allowed.
  EXPECT_TRUE(avatar_accessor.GetText().empty());
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
  AvatarToolbarButtonInterface* avatar =
      GetAvatarToolbarButtonInterface(browser());
  AvatarToolbarButtonTestAccessor avatar_accessor(browser());
  ASSERT_EQ(avatar_accessor.GetText(),
            l10n_util::GetStringFUTF16(IDS_AVATAR_BUTTON_GREETING,
                                       test_given_name()));
  avatar->ClearActiveStateForTesting();
  // The greeting should NOT be followed by the history sync opt-in entry point
  // if sync is not allowed.
  EXPECT_TRUE(avatar_accessor.GetText().empty());
}

INSTANTIATE_TEST_SUITE_P(HistorySyncOptinManagedType,
                         AvatarToolbarButtonHistorySyncOptinManagedTypeTest,
                         ValuesIn(kHistorySyncOptinSyncManagedTypeTestCases));
#endif

TEST_WITH_SIGNED_IN_FROM_PRE(IN_PROC_BROWSER_TEST_P,
                             MAYBE_AvatarToolbarButtonPromoBrowserTest,
                             PromoThenPassphraseError) {
  SetupRequirementsForPromoType(GetAvatarPromoType());

  AvatarToolbarButtonInterface* avatar =
      GetAvatarToolbarButtonInterface(browser());
  AvatarToolbarButtonTestAccessor avatar_accessor(browser());
  ASSERT_EQ(avatar_accessor.GetText(),
            l10n_util::GetStringFUTF16(IDS_AVATAR_BUTTON_GREETING,
                                       test_given_name()));
  avatar->ClearActiveStateForTesting();
  // The greeting should be followed by the the promo.
  EXPECT_EQ(avatar_accessor.GetText(), GetExpectedPromoText());
  SimulatePassphraseError();
  // The promo should be replaced by the passphrase error message.
  EXPECT_EQ(avatar_accessor.GetText(),
            l10n_util::GetStringUTF16(
                IDS_SYNC_STATUS_NEEDS_PASSWORD_BUTTON_MAYBE_TITLE_CASE));
  ClearPassphraseError();
  // After clearing the passphrase error, the promo should NOT be shown.
  EXPECT_TRUE(avatar_accessor.GetText().empty());
}

TEST_WITH_SIGNED_IN_FROM_PRE(IN_PROC_BROWSER_TEST_P,
                             MAYBE_AvatarToolbarButtonPromoBrowserTest,
                             PromoThenClientUpgradeError) {
  SetupRequirementsForPromoType(GetAvatarPromoType());

  AvatarToolbarButtonInterface* avatar =
      GetAvatarToolbarButtonInterface(browser());
  AvatarToolbarButtonTestAccessor avatar_accessor(browser());
  ASSERT_EQ(avatar_accessor.GetText(),
            l10n_util::GetStringFUTF16(IDS_AVATAR_BUTTON_GREETING,
                                       test_given_name()));
  avatar->ClearActiveStateForTesting();
  // The greeting should be followed by the promo.
  EXPECT_EQ(avatar_accessor.GetText(), GetExpectedPromoText());
  SimulateUpgradeClientError();
  // The promo should be replaced by the passphrase error message.
  EXPECT_EQ(avatar_accessor.GetText(),
            l10n_util::GetStringUTF16(IDS_SYNC_UPGRADE_CLIENT_BUTTON));
  ClearUpgradeClientError();
  // After clearing the passphrase error, the promo should NOT be shown.
  EXPECT_TRUE(avatar_accessor.GetText().empty());
}

TEST_WITH_SIGNED_IN_FROM_PRE(IN_PROC_BROWSER_TEST_P,
                             MAYBE_AvatarToolbarButtonPromoBrowserTest,
                             PromoThenSigninPending) {
  SetupRequirementsForPromoType(GetAvatarPromoType());

  AvatarToolbarButtonInterface* avatar =
      GetAvatarToolbarButtonInterface(browser());
  AvatarToolbarButtonTestAccessor avatar_accessor(browser());
  ASSERT_EQ(avatar_accessor.GetText(),
            l10n_util::GetStringFUTF16(IDS_AVATAR_BUTTON_GREETING,
                                       test_given_name()));
  avatar->ClearActiveStateForTesting();
  // The greeting should be followed by the promo.
  EXPECT_EQ(avatar_accessor.GetText(), GetExpectedPromoText());
  SimulateSigninPending(/*web_sign_out=*/false);
  // The promo should be replaced by the signin pending message.
  EXPECT_EQ(avatar_accessor.GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SIGNIN_PAUSED));
  ClearSigninPending();
  // After clearing the sign in error, the promo should NOT be shown.
  EXPECT_TRUE(avatar_accessor.GetText().empty());
}

TEST_WITH_SIGNED_IN_FROM_PRE(IN_PROC_BROWSER_TEST_P,
                             MAYBE_AvatarToolbarButtonPromoBrowserTest,
                             PromoThenExplicitText) {
  SetupRequirementsForPromoType(GetAvatarPromoType());

  AvatarToolbarButtonInterface* avatar =
      GetAvatarToolbarButtonInterface(browser());
  AvatarToolbarButtonTestAccessor avatar_accessor(browser());
  ASSERT_EQ(avatar_accessor.GetText(),
            l10n_util::GetStringFUTF16(IDS_AVATAR_BUTTON_GREETING,
                                       test_given_name()));
  avatar->ClearActiveStateForTesting();
  // The greeting should be followed by the promo.
  EXPECT_EQ(avatar_accessor.GetText(), GetExpectedPromoText());
  const std::u16string explicit_text(u"Explicit Text");
  base::ScopedClosureRunner hide_callback = avatar->SetExplicitButtonState(
      explicit_text, /*accessibility_label=*/std::nullopt,
      /*explicit_action=*/std::nullopt);
  // The promo should be replaced by the explicit text message.
  EXPECT_EQ(avatar_accessor.GetText(), explicit_text);
  hide_callback.RunAndReset();
  // After clearing the explicit text, the promo should NOT be shown.
  EXPECT_TRUE(avatar_accessor.GetText().empty());
}

TEST_WITH_SIGNED_IN_FROM_PRE(IN_PROC_BROWSER_TEST_P,
                             MAYBE_AvatarToolbarButtonPromoBrowserTest,
                             PromoNotShownIfErrorBeforeGreetingTimesOut) {
  SetupRequirementsForPromoType(GetAvatarPromoType());

  AvatarToolbarButtonInterface* avatar =
      GetAvatarToolbarButtonInterface(browser());
  AvatarToolbarButtonTestAccessor avatar_accessor(browser());
  ASSERT_EQ(avatar_accessor.GetText(),
            l10n_util::GetStringFUTF16(IDS_AVATAR_BUTTON_GREETING,
                                       test_given_name()));
  SimulatePassphraseError();
  avatar->ClearActiveStateForTesting();
  // No promo should be shown if the error is shown before the greeting times
  // out.
  EXPECT_EQ(avatar_accessor.GetText(),
            l10n_util::GetStringUTF16(
                IDS_SYNC_STATUS_NEEDS_PASSWORD_BUTTON_MAYBE_TITLE_CASE));
  ClearPassphraseError();
  // After clearing the passphrase error, the promo should NOT be shown.
  EXPECT_TRUE(avatar_accessor.GetText().empty());
}

TEST_WITH_SIGNED_IN_FROM_PRE(IN_PROC_BROWSER_TEST_P,
                             MAYBE_AvatarToolbarButtonPromoBrowserTest,
                             CollapsesOnPromoNoLongerElligible) {
  SetupRequirementsForPromoType(GetAvatarPromoType());

  AvatarToolbarButtonInterface* avatar =
      GetAvatarToolbarButtonInterface(browser());
  AvatarToolbarButtonTestAccessor avatar_accessor(browser());
  EXPECT_EQ(avatar_accessor.GetText(),
            l10n_util::GetStringFUTF16(IDS_AVATAR_BUTTON_GREETING,
                                       test_given_name()));
  avatar->ClearActiveStateForTesting();
  // The greeting should be followed by the promo.
  EXPECT_EQ(avatar_accessor.GetText(), GetExpectedPromoText());

  switch (GetAvatarPromoType()) {
    case signin::ProfileMenuAvatarButtonPromoInfo::Type::kHistorySyncPromo:
      // Enabling history should suppress the promo.
      SetHistoryAndTabsSyncingPreference(/*enable_sync=*/true);
      GetTestSyncService()->FireStateChanged();
      break;
    case signin::ProfileMenuAvatarButtonPromoInfo::Type::kBatchUploadPromo:
    case signin::ProfileMenuAvatarButtonPromoInfo::Type::
        kBatchUploadBookmarksPromo:
    case signin::ProfileMenuAvatarButtonPromoInfo::Type::
        kBatchUploadWindows10DepreciationPromo:
      // Removing the local data should suppress the promo.
      batch_upload_test_helper().ClearReturnDescriptions();
      GetTestSyncService()->FireStateChanged();
      break;
    case signin::ProfileMenuAvatarButtonPromoInfo::Type::kSyncPromo:
      EnableSync(test_email(), test_given_name());
      break;
    case signin::ProfileMenuAvatarButtonPromoInfo::Type::kSigninPromo:
      // TODO(crbug.com/486109449): Adapt the tests to support this promo.
      NOTREACHED() << "Test for this promo is not supported yet.";
  }

  // The button should return to the normal state.
  EXPECT_TRUE(avatar_accessor.GetText().empty());
}

TEST_WITH_SIGNED_IN_FROM_PRE(IN_PROC_BROWSER_TEST_P,
                             MAYBE_AvatarToolbarButtonPromoBrowserTest,
                             CollapsesOnSignOut) {
  SetupRequirementsForPromoType(GetAvatarPromoType());

  ASSERT_TRUE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));

  AvatarToolbarButtonInterface* avatar =
      GetAvatarToolbarButtonInterface(browser());
  AvatarToolbarButtonTestAccessor avatar_accessor(browser());
  EXPECT_EQ(avatar_accessor.GetText(),
            l10n_util::GetStringFUTF16(IDS_AVATAR_BUTTON_GREETING,
                                       test_given_name()));
  avatar->ClearActiveStateForTesting();
  // The greeting should be followed by the promo.
  EXPECT_EQ(avatar_accessor.GetText(), GetExpectedPromoText());
  Signout();
  // Once the user signs out, the button should return to the normal state.
  EXPECT_TRUE(avatar_accessor.GetText().empty());
}

TEST_WITH_SIGNED_IN_FROM_PRE(IN_PROC_BROWSER_TEST_P,
                             MAYBE_AvatarToolbarButtonPromoBrowserTest,
                             PromoNotShownIfMaxShownCountReached) {
  SetupRequirementsForPromoType(GetAvatarPromoType());

  AvatarToolbarButtonInterface* avatar =
      GetAvatarToolbarButtonInterface(browser());
  AvatarToolbarButtonTestAccessor avatar_accessor(browser());
  ASSERT_EQ(avatar_accessor.GetText(),
            l10n_util::GetStringFUTF16(IDS_AVATAR_BUTTON_GREETING,
                                       test_given_name()));
  avatar->ClearActiveStateForTesting();
  // The greeting should be followed by the promo.
  EXPECT_EQ(avatar_accessor.GetText(), GetExpectedPromoText());
  int shown_count = 1;
  avatar->ClearActiveStateForTesting();
  // The button comes back to the normal state.
  EXPECT_TRUE(avatar_accessor.GetText().empty());
  for (; shown_count < user_education::features::GetNewBadgeShowCount();
       ++shown_count) {
    avatar->ForceShowingPromoForTesting();
    EXPECT_EQ(avatar_accessor.GetText(), GetExpectedPromoText());
    avatar->ClearActiveStateForTesting();
    // The button comes back to the normal state.
    EXPECT_TRUE(avatar_accessor.GetText().empty());
  }
  avatar->ForceShowingPromoForTesting();
  // The promo should NOT be shown even after forcing it to show if the max
  // shown count has been reached.
  EXPECT_TRUE(avatar_accessor.GetText().empty());
}

INSTANTIATE_TEST_SUITE_P(
    ,
    MAYBE_AvatarToolbarButtonPromoBrowserTest,
    ValuesIn({signin::ProfileMenuAvatarButtonPromoInfo::Type::kHistorySyncPromo,
              signin::ProfileMenuAvatarButtonPromoInfo::Type::kSyncPromo,
              signin::ProfileMenuAvatarButtonPromoInfo::Type::kBatchUploadPromo,
              signin::ProfileMenuAvatarButtonPromoInfo::Type::
                  kBatchUploadBookmarksPromo,
              signin::ProfileMenuAvatarButtonPromoInfo::Type::
                  kBatchUploadWindows10DepreciationPromo}));

// TODO(crbug.com/331746545): Check the flaky test suite issue on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_AvatarToolbarButtonPromoClickBrowserTest \
  DISABLED_AvatarToolbarButtonPromoClickBrowserTest
#else
#define MAYBE_AvatarToolbarButtonPromoClickBrowserTest \
  AvatarToolbarButtonPromoClickBrowserTest
#endif
class MAYBE_AvatarToolbarButtonPromoClickBrowserTest
    : public MAYBE_AvatarToolbarButtonPromoBrowserTest {
 protected:
  MAYBE_AvatarToolbarButtonPromoClickBrowserTest()
      : delegate_auto_reset_(signin_ui_util::SetSigninUiDelegateForTesting(
            &mock_signin_ui_delegate_)) {}

  void ClickIdentityButton(ProfileMenuViewBase* profile_menu_view) {
    ASSERT_NE(profile_menu_view, nullptr);
    auto* button = profile_menu_view->GetIdentityButtonForTesting();
    ASSERT_NE(button, nullptr);
    button->AcceleratorPressed(ui::Accelerator());
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

  SetupRequirementsForPromoType(GetAvatarPromoType());

  base::HistogramTester histogram_tester;
  AvatarToolbarButtonInterface* avatar =
      GetAvatarToolbarButtonInterface(browser());
  AvatarToolbarButtonTestAccessor avatar_accessor(browser());
  ASSERT_EQ(avatar_accessor.GetText(),
            l10n_util::GetStringFUTF16(IDS_AVATAR_BUTTON_GREETING,
                                       test_given_name()));
  avatar->ClearActiveStateForTesting();
  // The greeting should be followed by the promo.
  EXPECT_EQ(avatar_accessor.GetText(), GetExpectedPromoText());
  histogram_tester.ExpectBucketCount("Signin.AvatarPillPromo.Shown",
                                     GetAvatarPromoType(),
                                     /*expected_count=*/1);
  // The button action should be overridden.
  histogram_tester.ExpectTotalCount(
      "Signin.AvatarPillPromo.DurationBeforeClick",
      /*expected_count=*/0);
  avatar_accessor.Click();
  histogram_tester.ExpectTotalCount(
      "Signin.AvatarPillPromo.DurationBeforeClick",
      /*expected_count=*/1);
  auto* coordinator = browser()->GetFeatures().profile_menu_coordinator();
  ASSERT_NE(coordinator, nullptr);
  EXPECT_TRUE(coordinator->IsShowing());
  EXPECT_TRUE(avatar_accessor.GetText().empty());
  // Once the promo collapses, the button action should be reset to the default
  // behavior.
  CoreAccountId primary_account_id =
      GetIdentityManager()->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
  switch (GetAvatarPromoType()) {
    case signin::ProfileMenuAvatarButtonPromoInfo::Type::kHistorySyncPromo:
      // Clicking the history sync button in the profile menu should trigger the
      // history sync dialog with the correct access point
      // (`kHistorySyncOptinExpansionPillOnStartup`).
      EXPECT_CALL(
          mock_signin_ui_delegate_,
          ShowHistorySyncOptinUI(browser()->profile(), primary_account_id,
                                 signin_metrics::AccessPoint::
                                     kHistorySyncOptinExpansionPillOnStartup));
      break;
    case signin::ProfileMenuAvatarButtonPromoInfo::Type::kSyncPromo:
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
    case signin::ProfileMenuAvatarButtonPromoInfo::Type::kBatchUploadPromo:
      // Batch Upload dialog should open with the right entry point.
      EXPECT_CALL(*mock_batch_upload_delegate(),
                  ShowBatchUploadDialog(
                      testing::_, testing::_,
                      BatchUploadService::EntryPoint::
                          kProfileMenuPrimaryButtonActionFromAvatarPromo,
                      testing::_));
      break;
    case signin::ProfileMenuAvatarButtonPromoInfo::Type::
        kBatchUploadBookmarksPromo:
      // Batch Upload dialog should open with the right entry point.
      EXPECT_CALL(
          *mock_batch_upload_delegate(),
          ShowBatchUploadDialog(
              testing::_, testing::_,
              BatchUploadService::EntryPoint::
                  kProfileMenuPrimaryButtonWithBookmarksActionFromAvatarPromo,
              testing::_));
      break;
    case signin::ProfileMenuAvatarButtonPromoInfo::Type::
        kBatchUploadWindows10DepreciationPromo:
      // Batch Upload dialog should open with the right entry point.
      EXPECT_CALL(
          *mock_batch_upload_delegate(),
          ShowBatchUploadDialog(
              testing::_, testing::_,
              BatchUploadService::EntryPoint::
                  kProfileMenuPrimaryButtonWithWindows10DepreciationActionFromAvatarPromo,
              testing::_));
      break;
    case signin::ProfileMenuAvatarButtonPromoInfo::Type::kSigninPromo:
      // TODO(crbug.com/486109449): Adapt the tests to support this promo.
      NOTREACHED() << "Test for this promo is not supported yet.";
  }
  ASSERT_NO_FATAL_FAILURE(
      ClickIdentityButton(coordinator->GetProfileMenuViewBaseForTesting()));
}

TEST_WITH_SIGNED_IN_FROM_PRE(IN_PROC_BROWSER_TEST_P,
                             MAYBE_AvatarToolbarButtonPromoClickBrowserTest,
                             PromoNotShownIfUsedLimitReached) {
  SetupRequirementsForPromoType(GetAvatarPromoType());

  AvatarToolbarButtonInterface* avatar =
      GetAvatarToolbarButtonInterface(browser());
  AvatarToolbarButtonTestAccessor avatar_accessor(browser());
  ASSERT_EQ(avatar_accessor.GetText(),
            l10n_util::GetStringFUTF16(IDS_AVATAR_BUTTON_GREETING,
                                       test_given_name()));
  avatar->ClearActiveStateForTesting();
  // The greeting should be followed by the promo.
  EXPECT_EQ(avatar_accessor.GetText(), GetExpectedPromoText());
  // The button action should be overridden.
  avatar_accessor.Click();
  // The button comes back to the normal state.
  EXPECT_TRUE(avatar_accessor.GetText().empty());
  int used_count = 1;
  for (; used_count < user_education::features::GetNewBadgeFeatureUsedCount();
       ++used_count) {
    avatar->ForceShowingPromoForTesting();
    EXPECT_EQ(avatar_accessor.GetText(), GetExpectedPromoText());
    avatar_accessor.Click();
    // The button comes back to the normal state.
    EXPECT_TRUE(avatar_accessor.GetText().empty());
  }
  avatar->ForceShowingPromoForTesting();
  // The promo should NOT be shown even after forcing it to show if the max used
  // count has been reached.
  EXPECT_TRUE(avatar_accessor.GetText().empty());

  Signout();
  const std::u16string account_name_2(u"Account name 2");
  SigninWithImage(/*email=*/u"test2@gmail.com", account_name_2);
  SetupRequirementsForPromoType(GetAvatarPromoType());

  switch (GetAvatarPromoType()) {
    case signin::ProfileMenuAvatarButtonPromoInfo::Type::kHistorySyncPromo:
    case signin::ProfileMenuAvatarButtonPromoInfo::Type::kBatchUploadPromo:
    case signin::ProfileMenuAvatarButtonPromoInfo::Type::
        kBatchUploadBookmarksPromo:
    case signin::ProfileMenuAvatarButtonPromoInfo::Type::
        kBatchUploadWindows10DepreciationPromo:
      ASSERT_EQ(
          avatar_accessor.GetText(),
          l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_MAKING_CHROME_YOURS));
      avatar->ClearActiveStateForTesting();
      avatar->ForceShowingPromoForTesting();
      break;
    case signin::ProfileMenuAvatarButtonPromoInfo::Type::kSyncPromo:
      ASSERT_EQ(avatar_accessor.GetText(),
                l10n_util::GetStringFUTF16(IDS_AVATAR_BUTTON_GREETING,
                                           account_name_2));
      avatar->ClearActiveStateForTesting();
      break;
    case signin::ProfileMenuAvatarButtonPromoInfo::Type::kSigninPromo:
      // TODO(crbug.com/486109449): Adapt the tests to support this promo.
      NOTREACHED() << "Test for this promo is not supported yet.";
  }
  // The promo should be shown for the new account (rate limiting is per
  // account).
  EXPECT_EQ(avatar_accessor.GetText(), GetExpectedPromoText());
}

TEST_WITH_SIGNED_IN_FROM_PRE(
    IN_PROC_BROWSER_TEST_P,
    MAYBE_AvatarToolbarButtonPromoClickBrowserTest,
    TriggersAndCollapsesConsistentlyAcrossMultipleBrowsers) {
  SetupRequirementsForPromoType(GetAvatarPromoType());

  // Make the delay for cross window animation replay zero to avoid flakiness.
  base::AutoReset<std::optional<base::TimeDelta>> delay_override_reset =
      signin_ui_util::
          CreateZeroOverrideDelayForCrossWindowAnimationReplayForTesting();
  base::HistogramTester histogram_tester;
  Profile* profile = browser()->profile();
  Browser* browser_1 = browser();
  AvatarToolbarButtonInterface* avatar_1 =
      GetAvatarToolbarButtonInterface(browser_1);
  AvatarToolbarButtonTestAccessor avatar_accessor1(browser_1);
  ASSERT_EQ(avatar_accessor1.GetText(),
            l10n_util::GetStringFUTF16(IDS_AVATAR_BUTTON_GREETING,
                                       test_given_name()));
  avatar_1->ClearActiveStateForTesting();

  // The greeting should be followed by the promo.
  EXPECT_EQ(avatar_accessor1.GetText(), GetExpectedPromoText());
  // Open the second browser before the promo collapses.
  Browser* browser_2 = CreateBrowser(profile);
  AvatarToolbarButtonTestAccessor avatar_accessor2(browser_2);
  // The promo should be shown in the second browser as well.
  EXPECT_EQ(avatar_accessor2.GetText(), GetExpectedPromoText());
  // `Signin.AvatarPillPromo.Shown` histogram should be recorded only once.
  histogram_tester.ExpectBucketCount("Signin.AvatarPillPromo.Shown",
                                     GetAvatarPromoType(),
                                     /*expected_count=*/1);
  avatar_1->ClearActiveStateForTesting();
  // The button in both browsers comes back to the normal state.
  EXPECT_TRUE(avatar_accessor1.GetText().empty());
  EXPECT_TRUE(avatar_accessor2.GetText().empty());
}

INSTANTIATE_TEST_SUITE_P(
    ,
    MAYBE_AvatarToolbarButtonPromoClickBrowserTest,
    ValuesIn({signin::ProfileMenuAvatarButtonPromoInfo::Type::kHistorySyncPromo,
              signin::ProfileMenuAvatarButtonPromoInfo::Type::kSyncPromo,
              signin::ProfileMenuAvatarButtonPromoInfo::Type::kBatchUploadPromo,
              signin::ProfileMenuAvatarButtonPromoInfo::Type::
                  kBatchUploadBookmarksPromo,
              signin::ProfileMenuAvatarButtonPromoInfo::Type::
                  kBatchUploadWindows10DepreciationPromo}));

// TODO(crbug.com/331746545): Check flaky test issue on windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_AvatarToolbarButtonSignedOutPromoBrowserTest \
  DISABLED_AvatarToolbarButtonSignedOutPromoBrowserTest
#else
#define MAYBE_AvatarToolbarButtonSignedOutPromoBrowserTest \
  AvatarToolbarButtonSignedOutPromoBrowserTest
#endif
class MAYBE_AvatarToolbarButtonSignedOutPromoBrowserTest
    : public AvatarToolbarButtonWithInteractiveFeaturePromoBrowserTest {
 public:
  MAYBE_AvatarToolbarButtonSignedOutPromoBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{syncer::kReplaceSyncPromosWithSignInPromos,
                              switches::kSigninPromoOnAvatarPill},
        /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(MAYBE_AvatarToolbarButtonSignedOutPromoBrowserTest,
                       SignedOutPromoTriggeredOnStartupAfterDelayExpired) {
  AvatarToolbarButtonInterface* avatar =
      GetAvatarToolbarButtonInterface(browser());
  AvatarToolbarButtonTestAccessor avatar_accessor(browser());
  ASSERT_EQ(avatar_accessor.GetText(), std::u16string());

  ASSERT_TRUE(avatar->GetStateAndFireSignedOutTriggerDelayTimerForTesting());
  EXPECT_EQ(avatar_accessor.GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SIGNIN_PROMO));

  Browser* new_browser = CreateBrowser(browser()->GetProfile());
  EXPECT_FALSE(avatar->GetStateAndFireSignedOutTriggerDelayTimerForTesting());
  EXPECT_EQ(AvatarToolbarButtonTestAccessor(new_browser).GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SIGNIN_PROMO));
}

// TODO(crbug.com/331746545): Check flaky test issue on windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_AvatarToolbarButtonSignedOutPromoOverriddenIdentityManagerBrowserTest \
  DISABLED_AvatarToolbarButtonSignedOutPromoOverriddenIdentityManagerBrowserTest
#else
#define MAYBE_AvatarToolbarButtonSignedOutPromoOverriddenIdentityManagerBrowserTest \
  AvatarToolbarButtonSignedOutPromoOverriddenIdentityManagerBrowserTest
#endif
// This test setup does not load the RefreshTokens until explicitly dnoe through
// the `LoadRefreshTokens()`.
class
    MAYBE_AvatarToolbarButtonSignedOutPromoOverriddenIdentityManagerBrowserTest
    : public AvatarToolbarButtonInterfaceBaseBrowserTest,
      public InProcessBrowserTest {
 public:
  MAYBE_AvatarToolbarButtonSignedOutPromoOverriddenIdentityManagerBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{syncer::kReplaceSyncPromosWithSignInPromos,
                              switches::kSigninPromoOnAvatarPill},
        /*disabled_features=*/{});
  }

  // AvatarToolbarButtonInterfaceBaseBrowserTest
  Browser* GetBrowser() const override { return browser(); }

  // InProcessBrowserTest
  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    IdentityTestEnvironmentProfileAdaptor::
        SetIdentityTestEnvironmentFactoriesOnBrowserContext(context);

    // Sets up the identity test environment and reset the refresh token
    // loading, before a browser is created.
    CHECK(!browser());
    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(
            Profile::FromBrowserContext(context));
    identity_test_env()->ResetToAccountsNotYetLoadedFromDiskState();
  }

  void TearDownOnMainThread() override {
    ClearMockBatchUploadDelegate();
    identity_test_env_adaptor_.reset();
  }

  void LoadRefreshTokens() {
    identity_test_env()->ReloadAccountsFromDisk();
    signin::WaitForRefreshTokensLoaded(GetIdentityManager());
  }

 private:
  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_adaptor_->identity_test_env();
  }

  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    MAYBE_AvatarToolbarButtonSignedOutPromoOverriddenIdentityManagerBrowserTest,
    SignedOutPromoTriggerDelayTimerStartAfterRefreshTokensAreLoaded) {
  ASSERT_FALSE(GetIdentityManager()->AreRefreshTokensLoaded());
  AvatarToolbarButtonInterface* avatar =
      GetAvatarToolbarButtonInterface(browser());
  AvatarToolbarButtonTestAccessor avatar_accessor(browser());
  ASSERT_EQ(avatar_accessor.GetText(), std::u16string());
  // Timer is not started as long as the refresh tokens are not loaded.
  EXPECT_FALSE(avatar->GetStateAndFireSignedOutTriggerDelayTimerForTesting());

  LoadRefreshTokens();
  ASSERT_TRUE(GetIdentityManager()->AreRefreshTokensLoaded());
  // Timer is now started and the promo computation happens.
  EXPECT_TRUE(avatar->GetStateAndFireSignedOutTriggerDelayTimerForTesting());
  EXPECT_EQ(avatar_accessor.GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SIGNIN_PROMO));
}

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

  void TearDownOnMainThread() override {
    AvatarToolbarButtonWithInteractiveFeaturePromoBrowserTest::
        TearDownOnMainThread();
    scoped_browser_management_.reset();
  }

 protected:
  testing::NiceMock<policy::MockConfigurationPolicyProvider> provider_;
  std::unique_ptr<policy::ScopedManagementServiceOverrideForTesting>
      scoped_browser_management_;

  base::test::ScopedFeatureList scoped_feature_list_{
      syncer::kReplaceSyncPromosWithSignInPromos};
};

IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonEnterpriseBadgingBrowserTest,
                       WorkProfileTextBadging) {
  AvatarToolbarButtonInterface* avatar_button =
      GetAvatarToolbarButtonInterface(browser());
  AvatarToolbarButtonTestAccessor avatar_accessor(browser());
  // Ensure enterprise badging can be shown.
  std::u16string work_label = u"Work";

  {
    enterprise_util::SetUserAcceptedAccountManagement(browser()->profile(),
                                                      true);
    EXPECT_EQ(avatar_accessor.GetText(), work_label);
    auto clear_closure = avatar_button->SetExplicitButtonState(
        u"Explicit text", /*accessibility_label=*/std::nullopt,
        /*explicit_action=*/std::nullopt);
    EXPECT_NE(avatar_accessor.GetText(), work_label);
    clear_closure.RunAndReset();
    EXPECT_EQ(avatar_accessor.GetText(), work_label);
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
    EXPECT_NE(avatar_accessor.GetText(), work_label);
    base::ScopedClosureRunner clear_closure =
        avatar_button->SetExplicitButtonState(
            u"Explicit text", /*accessibility_label=*/std::nullopt,
            /*explicit_action=*/std::nullopt);
    EXPECT_NE(avatar_accessor.GetText(), work_label);
    clear_closure.RunAndReset();
    EXPECT_NE(avatar_accessor.GetText(), work_label);
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
  AvatarToolbarButtonTestAccessor avatar_accessor(browser());
  {
    policy::ScopedManagementServiceOverrideForTesting profile_management{
        policy::ManagementServiceFactory::GetForProfile(browser()->profile()),
        policy::EnterpriseManagementAuthority::CLOUD};
    policy::ManagementServiceFactory::GetForProfile(browser()->profile())
        ->TriggerPolicyStatusChangedForTesting();
    EXPECT_EQ(avatar_accessor.GetText(), u"Work");
  }
  {
    policy::ScopedManagementServiceOverrideForTesting profile_management{
        policy::ManagementServiceFactory::GetForProfile(browser()->profile()),
        policy::EnterpriseManagementAuthority::NONE};
    policy::ManagementServiceFactory::GetForProfile(browser()->profile())
        ->TriggerPolicyStatusChangedForTesting();
    EXPECT_EQ(avatar_accessor.GetText(), std::u16string());
  }
}

IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonEnterpriseBadgingBrowserTest,
                       DefaultBadgeDisabledbyPolicy) {
  std::u16string work_label = u"Work";
  AvatarToolbarButtonTestAccessor avatar_accessor(browser());
  browser()->profile()->GetPrefs()->SetInteger(
      prefs::kEnterpriseProfileBadgeToolbarSettings, 1);

  enterprise_util::SetUserAcceptedAccountManagement(browser()->profile(), true);

  // There should be no text because the policy fully disables badging.
  EXPECT_EQ(avatar_accessor.GetText(), std::u16string());
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

  AvatarToolbarButtonTestAccessor avatar_accessor(browser());

  enterprise_util::SetUserAcceptedAccountManagement(browser()->profile(), true);

  // There should be no text because the policy fully disables badging.
  EXPECT_EQ(avatar_accessor.GetText(), std::u16string());
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

  AvatarToolbarButtonTestAccessor avatar_accessor(browser());

  enterprise_util::SetUserAcceptedAccountManagement(browser()->profile(), true);
  // The text should be tuncated to 16 characters followed by "...".
  EXPECT_EQ(avatar_accessor.GetText(), u"Custom Label Can…");
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
  AvatarToolbarButtonTestAccessor second_browser_avatar_accessor(
      second_browser);
  EXPECT_EQ(second_browser_avatar_accessor.GetText(), u"Custom Label");

  browser()->profile()->GetPrefs()->SetString(
      prefs::kEnterpriseCustomLabelForProfile, "Updated Label");
  EXPECT_EQ(second_browser_avatar_accessor.GetText(), u"Updated Label");
}

IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonEnterpriseBadgingBrowserTest,
                       WorkNewBrowserShowsBadge) {
  std::u16string work_label = u"Work";
  enterprise_util::SetUserAcceptedAccountManagement(browser()->profile(), true);

  Browser* second_browser = CreateBrowser(browser()->profile());
  AvatarToolbarButtonTestAccessor second_browser_avatar_accessor(
      second_browser);
  EXPECT_EQ(second_browser_avatar_accessor.GetText(), work_label);
}

// Sync Pause/Error has priority over WorkBadge.
IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonEnterpriseBadgingBrowserTest,
                       WorkBadgeAndSyncPaused) {
  AvatarToolbarButtonInterface* avatar_button =
      GetAvatarToolbarButtonInterface(browser());
  AvatarToolbarButtonTestAccessor avatar_accessor(browser());
  ASSERT_TRUE(avatar_accessor.GetText().empty());

  std::u16string work_label = u"Work";
  enterprise_util::SetUserAcceptedAccountManagement(browser()->profile(), true);
  EXPECT_EQ(avatar_accessor.GetText(), work_label);

  EnableSyncWithImage(u"work@managed.com");
  ASSERT_EQ(avatar_accessor.GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_MAKING_CHROME_YOURS));
  avatar_button->ClearActiveStateForTesting();

  ASSERT_EQ(avatar_accessor.GetText(), work_label);

  SimulateSyncPaused();
  // Sync Paused has priority over the Work badge.
  ExpectSyncPaused(browser());

  ClearSyncPaused();
  // Non transient mode should permanently show the work badge by default.
  // TODO(b/324018028): This test result might change with the ongoing changes.
  // At the end, the exact behavior could be set again. To review.
  EXPECT_EQ(avatar_accessor.GetText(), work_label);
}

IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonEnterpriseBadgingBrowserTest,
                       DecliningManagementShouldRemoveWorkBadge) {
  AvatarToolbarButtonTestAccessor avatar_accessor(browser());
  ASSERT_TRUE(avatar_accessor.GetText().empty());

  std::u16string work_label = u"Work";
  enterprise_util::SetUserAcceptedAccountManagement(browser()->profile(), true);
  EXPECT_EQ(avatar_accessor.GetText(), work_label);

  enterprise_util::SetUserAcceptedAccountManagement(browser()->profile(),
                                                    false);
  EXPECT_EQ(avatar_accessor.GetText(), std::u16string());
}

IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonEnterpriseBadgingBrowserTest,
                       PRE_ManagedAccountFlowWithDefaultWorkBadge) {
  AvatarToolbarButtonTestAccessor avatar_accessor(browser());
  // Normal state.
  ASSERT_TRUE(avatar_accessor.GetText().empty());

  SigninWithImage(u"work@managed.com", test_given_name());
  enterprise_util::SetUserAcceptedAccountManagement(browser()->profile(), true);
  EXPECT_EQ(avatar_accessor.GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_MAKING_CHROME_YOURS));
}

IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonEnterpriseBadgingBrowserTest,
                       ManagedAccountFlowWithDefaultWorkBadge) {
  ASSERT_TRUE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));

  // Disable the preferences about syncing the tabs and history to make the
  // avatar promo eligible.
  SetHistoryAndTabsSyncingPreference(/*enable_sync=*/false);

  AvatarToolbarButtonInterface* avatar =
      GetAvatarToolbarButtonInterface(browser());
  AvatarToolbarButtonTestAccessor avatar_accessor(browser());
  // Greeting shown even for Managed users.
  EXPECT_EQ(avatar_accessor.GetText(),
            l10n_util::GetStringFUTF16(IDS_AVATAR_BUTTON_GREETING,
                                       test_given_name()));
  avatar->ClearActiveStateForTesting();
  // The greeting is followed by the history sync opt-in.
  EXPECT_EQ(avatar_accessor.GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SYNC_HISTORY));
  avatar->ClearActiveStateForTesting();
  EXPECT_EQ(avatar_accessor.GetText(),
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
  AvatarToolbarButtonTestAccessor avatar_accessor(browser());
  AccountInfo account_info =
      SigninWithImage(u"work@managed.com", test_given_name());

  // Accept management and prepare work badge.
  enterprise_util::SetUserAcceptedAccountManagement(browser()->profile(), true);
  browser()->profile()->GetPrefs()->SetString(
      prefs::kEnterpriseCustomLabelForProfile, base::UTF16ToUTF8(work_badge()));

  EXPECT_EQ(avatar_accessor.GetText(),
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

  AvatarToolbarButtonInterface* avatar =
      GetAvatarToolbarButtonInterface(browser());
  AvatarToolbarButtonTestAccessor avatar_accessor(browser());
  EXPECT_EQ(avatar_accessor.GetText(),
            l10n_util::GetStringFUTF16(IDS_AVATAR_BUTTON_GREETING,
                                       test_given_name()));
  avatar->ClearActiveStateForTesting();
  // The greeting is followed by the history sync opt-in.
  EXPECT_EQ(avatar_accessor.GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SYNC_HISTORY));
  avatar->ClearActiveStateForTesting();

  // Once the promo is not shown anymore, we expect the work badge to be shown.
  EXPECT_EQ(avatar_accessor.GetText(), work_badge());
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
  AvatarToolbarButtonInterface* avatar =
      GetAvatarToolbarButtonInterface(browser());
  AvatarToolbarButtonTestAccessor avatar_accessor(browser());
  SigninWithImageAndClearGreetingAndSyncPromo(browser(), avatar,
                                              u"test@gmail.com");
  ASSERT_EQ(avatar_accessor.GetText(), std::u16string());

  // Browser opened before the error.
  Browser* opened_browser = CreateBrowser(browser()->profile());
  AvatarToolbarButtonTestAccessor opened_browser_avatar_accessor(
      opened_browser);
  ASSERT_EQ(opened_browser_avatar_accessor.GetText(), std::u16string());

  SimulateSigninPending(/*web_sign_out=*/false);
  EXPECT_EQ(avatar_accessor.GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SIGNIN_PAUSED));
  EXPECT_EQ(opened_browser_avatar_accessor.GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SIGNIN_PAUSED));

  // New browser opened after the error -- error should be shown directly.
  Browser* new_browser = CreateBrowser(browser()->profile());
  AvatarToolbarButtonTestAccessor new_browser_avatar_accessor(new_browser);
  EXPECT_EQ(new_browser_avatar_accessor.GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SIGNIN_PAUSED));

  ClearSigninPending();
  EXPECT_EQ(avatar_accessor.GetText(), std::u16string());
  EXPECT_EQ(new_browser_avatar_accessor.GetText(), std::u16string());
  EXPECT_EQ(new_browser_avatar_accessor.GetText(), std::u16string());
}

// TODO(b/331746545): Check flaky test issue on windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_SigninPendingFromWebSignout DISABLED_SigninPendingFromWebSignout
#else
#define MAYBE_SigninPendingFromWebSignout SigninPendingFromWebSignout
#endif
IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonBrowserTest,
                       MAYBE_SigninPendingFromWebSignout) {
  AvatarToolbarButtonInterface* avatar =
      GetAvatarToolbarButtonInterface(browser());
  AvatarToolbarButtonTestAccessor avatar_accessor(browser());

  SigninWithImageAndClearGreetingAndSyncPromo(browser(), avatar,
                                              u"test@gmail.com");
  ASSERT_EQ(avatar_accessor.GetText(), std::u16string());

  // Browser opened before the error.
  Browser* opened_browser = CreateBrowser(browser()->profile());
  AvatarToolbarButtonInterface* opened_browser_avatar_button =
      GetAvatarToolbarButtonInterface(opened_browser);
  AvatarToolbarButtonTestAccessor opened_browser_avatar_accessor(
      opened_browser);
  ASSERT_EQ(opened_browser_avatar_accessor.GetText(), std::u16string());

  SimulateSigninPending(/*web_sign_out=*/true);
  // Text does not appear directly after a web sign out, a timer is started.
  EXPECT_EQ(avatar_accessor.GetText(), std::u16string());
  EXPECT_EQ(opened_browser_avatar_accessor.GetText(), std::u16string());

  // New browser opened after the error and before timer ends -- error is not
  // shown directly.
  Browser* new_browser = CreateBrowser(browser()->profile());
  AvatarToolbarButtonInterface* new_browser_avatar_button =
      GetAvatarToolbarButtonInterface(new_browser);
  AvatarToolbarButtonTestAccessor new_browser_avatar_accessor(new_browser);
  EXPECT_EQ(new_browser_avatar_accessor.GetText(), std::u16string());

  // Simulate all the timer ends.
  avatar->ClearActiveStateForTesting();
  opened_browser_avatar_button->ClearActiveStateForTesting();
  new_browser_avatar_button->ClearActiveStateForTesting();

  EXPECT_EQ(avatar_accessor.GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SIGNIN_PAUSED));
  EXPECT_EQ(opened_browser_avatar_accessor.GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SIGNIN_PAUSED));
  EXPECT_EQ(new_browser_avatar_accessor.GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SIGNIN_PAUSED));

  ClearSigninPending();
  EXPECT_EQ(avatar_accessor.GetText(), std::u16string());
  EXPECT_EQ(opened_browser_avatar_accessor.GetText(), std::u16string());
  EXPECT_EQ(new_browser_avatar_accessor.GetText(), std::u16string());
}

// TODO(b/331746545): Check flaky test issue on windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_SigninPendingThenSignout DISABLED_SigninPendingThenSignout
#else
#define MAYBE_SigninPendingThenSignout SigninPendingThenSignout
#endif
IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonBrowserTest,
                       MAYBE_SigninPendingThenSignout) {
  AvatarToolbarButtonInterface* avatar =
      GetAvatarToolbarButtonInterface(browser());
  AvatarToolbarButtonTestAccessor avatar_accessor(browser());
  SigninWithImageAndClearGreetingAndSyncPromo(browser(), avatar,
                                              u"test@gmail.com");
  ASSERT_EQ(avatar_accessor.GetText(), std::u16string());

  SimulateSigninPending(/*web_sign_out=*/false);

  EXPECT_EQ(avatar_accessor.GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SIGNIN_PAUSED));

  Signout();

  EXPECT_EQ(avatar_accessor.GetText(), std::u16string());
}

IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonBrowserTest, AccessibilityLabels) {
  AvatarToolbarButtonInterface* avatar =
      GetAvatarToolbarButtonInterface(browser());

  const std::u16string profile_name(u"new_profile_name");
  profiles::UpdateProfileName(browser()->profile(), profile_name);

  const views::ViewAccessibility& accessibility =
      static_cast<AvatarToolbarButton*>(avatar)->GetViewAccessibility();

  EXPECT_EQ(accessibility.GetCachedName(), profile_name);
  EXPECT_EQ(accessibility.GetCachedDescription(), std::u16string());

  const std::u16string account_name(u"Test Name");
  SigninWithImageAndClearGreetingAndSyncPromo(browser(), avatar,
                                              u"test@gmail.com", account_name);

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
  AvatarToolbarButtonInterface* avatar =
      GetAvatarToolbarButtonInterface(browser());
  AvatarToolbarButtonTestAccessor avatar_accessor(browser());
  SigninWithImageAndClearGreetingAndSyncPromo(browser(), avatar,
                                              u"test@gmail.com");
  ASSERT_EQ(avatar_accessor.GetText(), std::u16string());
  SimulatePassphraseError();
  EXPECT_EQ(avatar_accessor.GetText(),
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
  AvatarToolbarButtonInterface* avatar =
      GetAvatarToolbarButtonInterface(browser());
  AvatarToolbarButtonTestAccessor avatar_accessor(browser());
  EnableSyncWithImageAndClearGreeting(avatar, u"test@gmail.com");
  ASSERT_EQ(avatar_accessor.GetText(), std::u16string());
  SimulatePassphraseError();
  EXPECT_EQ(avatar_accessor.GetText(),
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
  AvatarToolbarButtonInterface* avatar =
      GetAvatarToolbarButtonInterface(browser());
  AvatarToolbarButtonTestAccessor avatar_accessor(browser());
  EnableSyncWithImageAndClearGreeting(avatar, u"test@gmail.com");
  ASSERT_EQ(avatar_accessor.GetText(), std::u16string());
  SimulateUpgradeClientError();
  EXPECT_EQ(avatar_accessor.GetText(),
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
  AvatarToolbarButtonInterface* avatar_toolbar_button =
      GetAvatarToolbarButtonInterface(browser());
  AvatarToolbarButtonTestAccessor avatar_accessor(browser());
  ASSERT_NE(avatar_toolbar_button, nullptr);
  // Normal state.
  ASSERT_TRUE(avatar_accessor.GetText().empty());
  SigninWithImage(/*email=*/u"test@gmail.com", /*name=*/u"Account");
  EXPECT_EQ(avatar_accessor.GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_MAKING_CHROME_YOURS));
  avatar_toolbar_button->ClearActiveStateForTesting();
  EXPECT_TRUE(avatar_accessor.GetText().empty());

  // A new browser within the same session should not show any text as well.
  // Specifically not showing the greeting.
  Browser* second_browser = CreateBrowser(browser()->profile());
  EXPECT_TRUE(
      AvatarToolbarButtonTestAccessor(second_browser).GetText().empty());
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
  AvatarToolbarButtonTestAccessor avatar_accessor(browser());
  // Normal state.
  ASSERT_TRUE(avatar_accessor.GetText().empty());
  SigninWithImage(/*email=*/u"test@gmail.com", /*name=*/u"Account");
  EXPECT_EQ(avatar_accessor.GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_MAKING_CHROME_YOURS));

  Signout();
  EXPECT_TRUE(avatar_accessor.GetText().empty());
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
  AvatarToolbarButtonTestAccessor avatar_accessor(browser());
  // Normal state.
  ASSERT_TRUE(avatar_accessor.GetText().empty());
  SigninWithImage(/*email=*/u"test@gmail.com", /*name=*/u"Account");
  EXPECT_EQ(avatar_accessor.GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_MAKING_CHROME_YOURS));

  // A new browser should also show the message.
  Browser* second_browser = CreateBrowser(browser()->profile());
  AvatarToolbarButtonTestAccessor second_avatar_accessor(second_browser);
  EXPECT_EQ(second_avatar_accessor.GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_MAKING_CHROME_YOURS));

  // Clicking on either avatar buttons should clear both messages.
  avatar_accessor.Click();
  EXPECT_TRUE(avatar_accessor.GetText().empty());
  EXPECT_TRUE(second_avatar_accessor.GetText().empty());
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
  AvatarToolbarButtonInterface* avatar_toolbar_button =
      GetAvatarToolbarButtonInterface(browser);
  AvatarToolbarButtonTestAccessor avatar_accessor(browser);

  // The on sign-in state should be shown after the the browser window is
  // created if the sign-in event happened before the browser window was
  // created.
  EXPECT_EQ(avatar_accessor.GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_MAKING_CHROME_YOURS));
  avatar_toolbar_button->ClearActiveStateForTesting();
  // The button should return to the normal state.
  EXPECT_TRUE(avatar_accessor.GetText().empty());
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
  AvatarToolbarButtonInterface* avatar_toolbar_button =
      GetAvatarToolbarButtonInterface(browser());
  AvatarToolbarButtonTestAccessor avatar_accessor(browser());
  ASSERT_NE(avatar_toolbar_button, nullptr);
  // Normal state.
  ASSERT_TRUE(avatar_accessor.GetText().empty());
  EnableSync(/*email=*/u"test@gmail.com", /*name=*/u"Account");
  EXPECT_EQ(avatar_accessor.GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_MAKING_CHROME_YOURS));
  avatar_toolbar_button->ClearActiveStateForTesting();
  EXPECT_TRUE(avatar_accessor.GetText().empty());
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
  AvatarToolbarButtonInterface* avatar_toolbar_button =
      GetAvatarToolbarButtonInterface(browser());
  AvatarToolbarButtonTestAccessor avatar_accessor(browser());
  ASSERT_NE(avatar_toolbar_button, nullptr);
  // Normal state.
  ASSERT_TRUE(avatar_accessor.GetText().empty());
  Signin(/*email=*/u"test@gmail.com", /*name=*/u"Account");
  EXPECT_EQ(avatar_accessor.GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_MAKING_CHROME_YOURS));

  const std::u16string explicit_state_text(u"Explicit State");
  base::ScopedClosureRunner hide_callback =
      avatar_toolbar_button->SetExplicitButtonState(
          explicit_state_text, /*accessibility_label=*/std::nullopt,
          /*explicit_action=*/std::nullopt);
  EXPECT_EQ(avatar_accessor.GetText(), explicit_state_text);
  hide_callback.RunAndReset();

  // The on sign-in state is hidden.
  EXPECT_EQ(avatar_accessor.GetText(), std::u16string());
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
  AvatarToolbarButtonInterface* avatar_toolbar_button =
      GetAvatarToolbarButtonInterface(browser());
  AvatarToolbarButtonTestAccessor avatar_accessor(browser());
  ASSERT_NE(avatar_toolbar_button, nullptr);
  // Normal state.
  ASSERT_TRUE(avatar_accessor.GetText().empty());
  EnableSync(/*email=*/u"test@gmail.com", /*name=*/u"Account");
  EXPECT_EQ(avatar_accessor.GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_MAKING_CHROME_YOURS));
  SimulateSyncError();
  // On sign-in state is higher priority than any sync error state.
  EXPECT_EQ(avatar_accessor.GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_MAKING_CHROME_YOURS));
  avatar_toolbar_button->ClearActiveStateForTesting();
  // Once the sign-in state is cleared, the sync error state is shown.
  EXPECT_EQ(avatar_accessor.GetText(),
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
  AvatarToolbarButtonInterface* avatar_toolbar_button =
      GetAvatarToolbarButtonInterface(browser());
  AvatarToolbarButtonTestAccessor avatar_accessor(browser());
  ASSERT_NE(avatar_toolbar_button, nullptr);
  // Normal state.
  ASSERT_TRUE(avatar_accessor.GetText().empty());
  SigninWithImage(/*email=*/u"test@gmail.com", /*name=*/u"Account");
  EXPECT_EQ(avatar_accessor.GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_MAKING_CHROME_YOURS));
  avatar_toolbar_button->ClearActiveStateForTesting();
  EXPECT_TRUE(avatar_accessor.GetText().empty());
}

IN_PROC_BROWSER_TEST_F(
    AvatarToolbarButtonReplaceSyncPromosWithSignInPromosBrowserTest,
    MAYBE_DoesNotShowOnBrowserRestart) {
  AvatarToolbarButtonInterface* avatar_toolbar_button =
      GetAvatarToolbarButtonInterface(browser());
  AvatarToolbarButtonTestAccessor avatar_accessor(browser());
  ASSERT_NE(avatar_toolbar_button, nullptr);
  // The greetings are shown after the restart.
  EXPECT_EQ(avatar_accessor.GetText(),
            l10n_util::GetStringFUTF16(IDS_AVATAR_BUTTON_GREETING, u"Account"));
  avatar_toolbar_button->ClearActiveStateForTesting();
  // The button should return to the normal state.
  EXPECT_TRUE(avatar_accessor.GetText().empty());
}

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
      feature_list_.InitWithFeatures(
          /*enabled_features=*/{},
          /*disabled_features=*/{
              syncer::kReplaceSyncPromosWithSignInPromos,
              syncer::kReplaceSyncPromosWithSigninPromosNewSignin});
    } else {
      feature_list_.InitWithFeatures(
          /*enabled_features=*/
          {syncer::kReplaceSyncPromosWithSignInPromos,
           syncer::kReplaceSyncPromosWithSigninPromosNewSignin},
          /*disabled_features=*/{});
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

  AvatarToolbarButtonInterface* avatar_toolbar_button =
      GetAvatarToolbarButtonInterface(browser());
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
  AvatarToolbarButtonInterface* avatar_toolbar_button =
      GetAvatarToolbarButtonInterface(browser());
  ASSERT_NE(avatar_toolbar_button, nullptr);

  // Attempt to show the IPH.
  avatar_toolbar_button->MaybeShowSignInBenefitsIPH();
  EXPECT_FALSE(WillShowPromo());
}

class AvatarToolbarButtonSignInBenefitsNewSigninIphBrowserTest
    : public InteractiveFeaturePromoTestMixin<AvatarToolbarButtonBrowserTest> {
 public:
  AvatarToolbarButtonSignInBenefitsNewSigninIphBrowserTest()
      : InteractiveFeaturePromoTestMixin(UseDefaultTrackerAllowingPromos(
            {feature_engagement::kIPHSignInBenefitsNewSigninFeature})) {
    if (content::IsPreTest()) {
      feature_list_.InitWithFeatures(
          /*enabled_features=*/{},
          /*disabled_features=*/{
              syncer::kReplaceSyncPromosWithSignInPromos,
              syncer::kReplaceSyncPromosWithSigninPromosNewSignin});
    } else {
      feature_list_.InitWithFeatures(
          /*enabled_features=*/
          {syncer::kReplaceSyncPromosWithSigninPromosNewSignin,
           feature_engagement::kIPHSignInBenefitsNewSigninFeature},
          /*disabled_features=*/{
              syncer::kReplaceSyncPromosWithSignInPromos,
              feature_engagement::kIPHSignInBenefitsFeature});
    }
  }

  bool WillShowPromo() {
    auto* const user_education = BrowserUserEducationInterface::From(browser());
    return user_education->IsFeaturePromoActive(
               feature_engagement::kIPHSignInBenefitsNewSigninFeature) ||
           user_education->IsFeaturePromoQueued(
               feature_engagement::kIPHSignInBenefitsNewSigninFeature);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonSignInBenefitsNewSigninIphBrowserTest,
                       PRE_ShownForUsersSignedInBeforeMigration) {
  Signin(/*email=*/u"test@gmail.com", /*name=*/u"Account");
}

IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonSignInBenefitsNewSigninIphBrowserTest,
                       ShownForUsersSignedInBeforeMigration) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTabContents);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTabContents);

  RunTestSequence(
      InstrumentTab(kFirstTabContents, 0),
      WaitForPromo(feature_engagement::kIPHSignInBenefitsNewSigninFeature),
      PressNonDefaultPromoButton(), InstrumentTab(kSecondTabContents, 1),
      WaitForWebContentsReady(kSecondTabContents,
                              GURL(chrome::kChromeUIAccountSettingsURL)),
      CheckPromoActive(feature_engagement::kIPHSignInBenefitsNewSigninFeature,
                       false));
}

// Parameterized test for the new sign-in benefits IPH.
// It verifies that the new IPH is shown only if the legacy IPH was not shown.
// - PRE_PRE_: Sign-in with features disabled (requirement for any IPH to show).
// - PRE_: Restart Chrome with or without showing the legacy IPH
// (parameterized).
// - Main: Check that the new IPH is only shown if the legacy one was not.
class AvatarToolbarButtonSignInBenefitsNewSigninIphParameterizedTest
    : public InteractiveFeaturePromoTestMixin<AvatarToolbarButtonBrowserTest>,
      public testing::WithParamInterface<bool> {
 public:
  AvatarToolbarButtonSignInBenefitsNewSigninIphParameterizedTest()
      : InteractiveFeaturePromoTestMixin(UseDefaultTrackerAllowingPromos(
            {feature_engagement::kIPHSignInBenefitsFeature,
             feature_engagement::kIPHSignInBenefitsNewSigninFeature})) {
    std::string test_name =
        ::testing::UnitTest::GetInstance()->current_test_info()->name();
    bool is_pre_pre = test_name.find("PRE_PRE_") != std::string::npos;
    bool is_pre = !is_pre_pre && test_name.find("PRE_") != std::string::npos;

    std::vector<base::test::FeatureRef> enabled;
    std::vector<base::test::FeatureRef> disabled;

    const std::vector<base::test::FeatureRef> kDisabledMigrationFeatures = {
        syncer::kReplaceSyncPromosWithSignInPromos,
        syncer::kReplaceSyncPromosWithSigninPromosNewSignin};

    if (is_pre_pre) {
      // Disable all features to avoid any startup promos while signing in.
      disabled = kDisabledMigrationFeatures;
    } else if (is_pre) {
      if (ShouldShowLegacyIphInPre()) {
        enabled = {syncer::kReplaceSyncPromosWithSignInPromos,
                   feature_engagement::kIPHSignInBenefitsFeature};
        disabled = {syncer::kReplaceSyncPromosWithSigninPromosNewSignin};
      } else {
        // The PRE_ test should have the same feature config as the PRE_PRE_
        // when the legacy IPH should not be shown, because we conceptually want
        // this step to do nothing in this case. It will behave just as if
        // Chrome was restarted without configuration change.
        disabled = kDisabledMigrationFeatures;
      }
    } else {
      // Main test: enable new features and disable old ones to test suppression
      // check.
      enabled = {syncer::kReplaceSyncPromosWithSigninPromosNewSignin,
                 feature_engagement::kIPHSignInBenefitsNewSigninFeature};
      disabled = {syncer::kReplaceSyncPromosWithSignInPromos,
                  feature_engagement::kIPHSignInBenefitsFeature};
    }

    feature_list_.InitWithFeatures(enabled, disabled);
    delay_override_.emplace(
        AvatarToolbarButtonInterface::
            SetScopedIPHMinDelayAfterCreationForTesting(base::TimeDelta()));
  }

  bool ShouldShowLegacyIphInPre() const { return GetParam(); }

  bool WillShowPromo() {
    auto* const user_education = BrowserUserEducationInterface::From(browser());
    return user_education->IsFeaturePromoActive(
               feature_engagement::kIPHSignInBenefitsNewSigninFeature) ||
           user_education->IsFeaturePromoQueued(
               feature_engagement::kIPHSignInBenefitsNewSigninFeature);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  std::optional<base::AutoReset<base::TimeDelta>> delay_override_;
};

IN_PROC_BROWSER_TEST_P(
    AvatarToolbarButtonSignInBenefitsNewSigninIphParameterizedTest,
    PRE_PRE_NotShownIfLegacyWasShown) {
  Signin(/*email=*/u"test@gmail.com", /*name=*/u"Account");
}

IN_PROC_BROWSER_TEST_P(
    AvatarToolbarButtonSignInBenefitsNewSigninIphParameterizedTest,
    PRE_NotShownIfLegacyWasShown) {
  if (ShouldShowLegacyIphInPre()) {
    RunTestSequence(
        WaitForPromo(feature_engagement::kIPHSignInBenefitsFeature));
  }
}

IN_PROC_BROWSER_TEST_P(
    AvatarToolbarButtonSignInBenefitsNewSigninIphParameterizedTest,
    NotShownIfLegacyWasShown) {
  if (ShouldShowLegacyIphInPre()) {
    // Legacy was shown in PRE_ test, so new IPH should be suppressed.
    EXPECT_FALSE(WillShowPromo());
  } else {
    // Legacy was not shown in PRE_ test, so new IPH should be shown.
    RunTestSequence(
        WaitForPromo(feature_engagement::kIPHSignInBenefitsNewSigninFeature));
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    AvatarToolbarButtonSignInBenefitsNewSigninIphParameterizedTest,
    testing::Bool());
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

// TODO(crbug.com/505530418): The test is flaky on Mac builders.
#if BUILDFLAG(IS_MAC)
#define MAYBE_PasskeyUnlockError DISABLED_PasskeyUnlockError
#else
#define MAYBE_PasskeyUnlockError PasskeyUnlockError
#endif  // BUILDFLAG(IS_MAC)

IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonPasskeyUnlockErrorBrowserTest,
                       MAYBE_PasskeyUnlockError) {
  AvatarToolbarButtonInterface* avatar =
      GetAvatarToolbarButtonInterface(browser());
  AvatarToolbarButtonTestAccessor avatar_accessor(browser());
  ASSERT_TRUE(base::test::RunUntil([browser = browser()]() {
    InitialWebUIManager* manager = InitialWebUIManager::From(browser);
    return !manager || !manager->IsShowPending();
  }));
  SigninWithImageAndClearGreetingAndSyncPromo(browser(), avatar,
                                              u"test@gmail.com");

  base::HistogramTester histogram_tester;
  // Simulate the error appearing.
  ON_CALL(*passkey_unlock_manager(), ShouldDisplayErrorUi())
      .WillByDefault(testing::Return(true));
  passkey_unlock_manager()->NotifyObserversForTesting();

  EXPECT_EQ(avatar_accessor.GetText(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_PASSKEYS_ERROR_VERIFY));
  // TODO(crbug.com/506022005): We use EXPECT_GE() instead of EXPECT_EQ(..., 1)
  // because the 'doubled' counts happen as InitialWebUI instantiates a second
  // UI frontend (the WebUI toolbar) which also observes the manager. Since the
  // histogram is recorded when the UI reacts to the manager's state change,
  // both the Views and WebUI components trigger the metric, leading to the
  // doubled count in tests. We should fix the overall architecture of the
  // AvatarButton to have a single state manager per browser, then change this
  // to EXPECT_EQ.
  EXPECT_GE(
      histogram_tester.GetBucketCount(
          kPasskeyUnlockErrorUiEventHistogram,
          webauthn::PasskeyUnlockManager::ErrorUIEventType::kAvatarUIDisplayed),
      1);

  // Click the avatar button.
  avatar_accessor.Click();
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

  // Once the error disappeared, the button should return to the normal state.
  EXPECT_EQ(avatar_accessor.GetText(), std::u16string());

  // TODO(crbug.com/506022005): Same as above. We use EXPECT_GE() instead of
  // EXPECT_EQ(..., 1) because the 'doubled' counts happen as InitialWebUI
  // instantiates a second UI frontend.
  EXPECT_GE(
      histogram_tester.GetBucketCount(
          kPasskeyUnlockErrorUiEventHistogram,
          webauthn::PasskeyUnlockManager::ErrorUIEventType::kAvatarUIHidden),
      1);
}
#endif  // !BUILDFLAG(IS_CHROMEOS)
