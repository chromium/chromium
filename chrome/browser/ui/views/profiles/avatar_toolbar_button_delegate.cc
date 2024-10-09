// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/avatar_toolbar_button_delegate.h"

#include <optional>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/scoped_observation.h"
#include "base/strings/utf_string_conversions.h"
#include "base/supports_user_data.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/profiles/profile_colors_util.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/dotted_icon.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_prefs.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_change_event.h"
#include "components/sync/base/features.h"
#include "components/sync/service/sync_service.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/accessibility/view_accessibility.h"

namespace {

// Timings used for testing purposes. Infinite time for the tests to confidently
// test the behaviors while a delay is ongoing.
constexpr base::TimeDelta kInfiniteTimeForTesting = base::TimeDelta::Max();

constexpr float kAvatarIconSigninPendingShrinkRatio = 0.75;

constexpr base::TimeDelta kShowNameDuration = base::Seconds(3);
static std::optional<base::TimeDelta> g_show_name_duration_for_testing;

constexpr base::TimeDelta kShowSigninPendingTextDelay = base::Minutes(50);
static std::optional<base::TimeDelta>
    g_show_signin_pending_text_delay_for_testing;

ProfileAttributesStorage& GetProfileAttributesStorage() {
  return g_browser_process->profile_manager()->GetProfileAttributesStorage();
}

ProfileAttributesEntry* GetProfileAttributesEntry(Profile* profile) {
  return GetProfileAttributesStorage().GetProfileAttributesWithPath(
      profile->GetPath());
}

gfx::Image GetGaiaAccountImage(Profile* profile) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  if (identity_manager &&
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    return identity_manager
        ->FindExtendedAccountInfoByAccountId(
            identity_manager->GetPrimaryAccountId(
                signin::ConsentLevel::kSignin))
        .account_image;
  }
  return gfx::Image();
}

// Expected to be called when Management is set and enterprise badging is
// enabled. Returns:
// - true for Work.
// - false for School.
bool IsManagementWork(Profile* profile) {
  CHECK(enterprise_util::CanShowEnterpriseBadgingForAvatar(profile));
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  auto management_environment = enterprise_util::GetManagementEnvironment(
      profile, identity_manager->FindExtendedAccountInfoByAccountId(
                   identity_manager->GetPrimaryAccountId(
                       signin::ConsentLevel::kSignin)));
  CHECK_NE(management_environment,
           enterprise_util::ManagementEnvironment::kNone);
  return management_environment ==
         enterprise_util::ManagementEnvironment::kWork;
}

}  // namespace

namespace internal {

// States of the button ordered in priority of getting displayed.
// The order of those values is used with the `StateManager` to make sure the
// active state with the highest priority is shown.
// The lower the value of the enum, the higher the priority.
enum class ButtonState {
  kGuestSession,
  kIncognitoProfile,
  kExplicitTextShowing,
  kShowIdentityName,
  kSigninPending,
  kSyncPaused,
  kUpgradeClientError,
  kPassphraseError,
  // Catch-all for remaining errors in sync-the-feature or sync-the-transport.
  kSyncError,
  // Includes Work and School.
  kManagement,
  kNormal
};

namespace {

class StateProvider;
class ExplicitStateProvider;
class SyncErrorStateProvider;
class SigninPendingStateProvider;
class ShowIdentityNameStateProvider;
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
class ManagementStateProvider;
#endif

// Allows getting data from the underlying implementation of a `StateProvider`.
// `StateVisitor::visit()` overrides to be added based on the need.
class StateVisitor {
 public:
  virtual void visit(const ExplicitStateProvider* state_provider) = 0;
  virtual void visit(const SyncErrorStateProvider* state_provider) = 0;
  virtual void visit(const SigninPendingStateProvider* state_provider) = 0;
  virtual void visit(const ShowIdentityNameStateProvider* state_provider) = 0;
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  virtual void visit(const ManagementStateProvider* state_provider) = 0;
#endif
};

class StateObserver {
 public:
  virtual void OnStateProviderUpdateRequest(StateProvider* state_provider) = 0;

  virtual ~StateObserver() = default;
};

// Each implementation of StateProvider should be able to manage itself with the
// appropriate initial values such as a profile and observe/listen to changes in
// order to affect their active status.
class StateProvider {
 public:
  // The constructor should not call any function that would end up calling
  // `RequestUpdate()` as it could end up trying to compute the active state,
  // which is not guaranteed to return a valid state at this point since all the
  // main states might not be created yet.
  // Consider overriding `Init()` if you need to add a potential code to
  // `RequestUpdate()`. The init method will be
  // called right after all the main states are created.
  explicit StateProvider(StateObserver& state_observer)
      : state_observer_(state_observer) {}

  // TODO(b/324018028): Consider changing `IsActive()` to be non-virtual and
  // return a member variable `is_active_` that can be controlled by the derived
  // classes that sets the active/inactive state when needed, also requesting
  // updates on state change. This way we would make sure not to miss updates
  // when a state activation changes.
  virtual bool IsActive() const = 0;

  // This method should be used to initialize anything that could potentially
  // call a `RequestUpdate()` which would end up computing the active state.
  // This method will be called after all main states are created, making sure
  // that an active state will be correctly computed.
  virtual void Init() {}

  // This update request will attempt to update the text shown on the button.
  // The update will only go through if the requesting state was the main button
  // active one and is now inactive or if it is currently the main active one.
  // Therefore every time a `StateProvider` expects a change of internal state
  // it should call this method to attempt to propagate the changes.
  void RequestUpdate() { state_observer_->OnStateProviderUpdateRequest(this); }

  virtual void accept(StateVisitor& visitor) const {}

  virtual ~StateProvider() = default;

 private:
  raw_ref<StateObserver> state_observer_;
};

// Used for Guest and Incognito sessions.
class PrivateStateProvider : public StateProvider, public BrowserListObserver {
 public:
  explicit PrivateStateProvider(StateObserver& state_observer)
      : StateProvider(state_observer) {
    scoped_browser_list_observation_.Observe(BrowserList::GetInstance());
  }
  ~PrivateStateProvider() override = default;

  // This state is always active when the Profile is in private mode, the
  // Profile type is not expected to change.
  bool IsActive() const override { return true; }

  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override { RequestUpdate(); }
  void OnBrowserRemoved(Browser* browser) override { RequestUpdate(); }

 private:
  base::ScopedObservation<BrowserList, BrowserListObserver>
      scoped_browser_list_observation_{this};
};

class ExplicitStateProvider : public StateProvider {
 public:
  explicit ExplicitStateProvider(
      StateObserver& state_observer,
      const std::u16string& explicit_text,
      std::optional<std::u16string> accessibility_label)
      : StateProvider(state_observer),
        explicit_text_(explicit_text),
        accessibility_label_(accessibility_label) {}
  ~ExplicitStateProvider() override = default;

  // StateProvider:
  bool IsActive() const override { return active_; }

  std::u16string GetText() const { return explicit_text_; }
  std::optional<std::u16string> GetAccessibiltyLabel() const {
    return accessibility_label_;
  }

  // Used as the callback closure to the setter of the explicit state,
  // or when overriding the explicit state by another one.
  void Clear() {
    active_ = false;
    RequestUpdate();
  }

  base::WeakPtr<ExplicitStateProvider> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  // StateProvider:
  void accept(StateVisitor& visitor) const override { visitor.visit(this); }

  bool active_ = true;

  const std::u16string explicit_text_;
  const std::optional<std::u16string> accessibility_label_;

  base::WeakPtrFactory<ExplicitStateProvider> weak_ptr_factory_{this};
};

class ShowIdentityNameStateProvider : public StateProvider,
                                      public signin::IdentityManager::Observer,
                                      public AvatarToolbarButton::Observer {
 public:
  explicit ShowIdentityNameStateProvider(
      StateObserver& state_observer,
      Profile& profile,
      AvatarToolbarButton& avatar_toolbar_button)
      : StateProvider(state_observer),
        profile_(profile),
        avatar_toolbar_button_(avatar_toolbar_button) {
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(&profile);
    CHECK(identity_manager);
    identity_manager_observation_.Observe(identity_manager);
    avatar_button_observation_.Observe(&avatar_toolbar_button);
  }

  ~ShowIdentityNameStateProvider() override {
    avatar_button_observation_.Reset();
  }

  // StateProvider:
  bool IsActive() const override { return show_identity_request_count_ > 0; }

  void Init() override {
    if (IdentityManagerFactory::GetForProfile(&profile_.get())
            ->AreRefreshTokensLoaded()) {
      // Will potentially call a `RequestUpdate()`.
      OnRefreshTokensLoaded();
    }
  }

  // IdentityManager::Observer:
  // Needed if the first sync promo account should be displayed.
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event) override {
    if (event.GetEventTypeFor(signin::ConsentLevel::kSignin) !=
        signin::PrimaryAccountChangeEvent::Type::kSet) {
      return;
    }
    OnUserIdentityChanged();
  }

  void OnRefreshTokensLoaded() override {
    // TODO(b/324018028): This check can be removed as `OnRefreshTokensLoaded()`
    // is called when first observing and not as a result of
    // `IdentityManager::OnRefreshTokensLoaded()`. So double call should not
    // happen anymore.
    if (refresh_tokens_loaded_) {
      // This is possible, if `AvatarToolbarButtonDelegate`  constructor  is
      // called within the loop in
      //  `IdentityManager::OnRefreshTokensLoaded()` to notify observers. In
      //  that case, |OnRefreshTokensLoaded| will be called twice, once from
      //  AvatarToolbarButtonDelegate` constructor and another time from the
      //  `IdentityManager`. This happens for new signed in profiles. See
      //  https://crbug.com/1035480
      return;
    }

    refresh_tokens_loaded_ = true;
    if (!signin_ui_util::ShouldShowAnimatedIdentityOnOpeningWindow(
            GetProfileAttributesStorage(), &profile_.get())) {
      return;
    }

    if (!IdentityManagerFactory::GetForProfile(&profile_.get())
             ->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
      return;
    }

    OnUserIdentityChanged();
  }

  void OnIdentityManagerShutdown(signin::IdentityManager*) override {
    identity_manager_observation_.Reset();
  }

  // AvatarToolbarButton::Observer
  void OnMouseExited() override { MaybeHideIdentityAnimation(); }

  void OnBlur() override { MaybeHideIdentityAnimation(); }

  void OnIPHPromoChanged(bool has_promo) override {
    if (has_in_product_help_promo_ == has_promo) {
      return;
    }

    has_in_product_help_promo_ = has_promo;
    // Trigger a new animation, even if the IPH is being removed. This keeps the
    // pill open a little more and avoids jankiness caused by the two animations
    // (IPH and identity pill) happening concurrently.
    // See https://crbug.com/1198907
    ShowIdentityName();
  }

  void OnIconUpdated() override {
    // Try to show the name if we were waiting for an image.
    MaybeShowIdentityName();
  }

  void ForceDelayTimeoutForTesting() { OnIdentityAnimationTimeout(); }

 private:
  // StateProvider:
  void accept(StateVisitor& visitor) const override { visitor.visit(this); }

  // Initiates showing the identity.
  void OnUserIdentityChanged() {
    signin_ui_util::RecordAnimatedIdentityTriggered(&profile_.get());
    // On any following icon update the name will be attempted to be shown when
    // the image is ready.
    waiting_for_image_ = true;
    MaybeShowIdentityName();
  }

  // Should be called when the icon is updated. This may trigger theshowing of
  // the identity name.
  void MaybeShowIdentityName() {
    if (!waiting_for_image_ ||
        ::GetGaiaAccountImage(&profile_.get()).IsEmpty()) {
      return;
    }

    // Check that the user is still signed in. See https://crbug.com/1025674
    if (!IdentityManagerFactory::GetForProfile(&profile_.get())
             ->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
      Clear();
      return;
    }

    ShowIdentityName();
  }

  // Shows the name in the identity pill. If the name is already showing, this
  // extends the duration.
  void ShowIdentityName() {
    ++show_identity_request_count_;
    waiting_for_image_ = false;

    RequestUpdate();

    // Hide the pill after a while.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            &ShowIdentityNameStateProvider::OnIdentityAnimationTimeout,
            weak_ptr_factory_.GetWeakPtr()),
        g_show_name_duration_for_testing.value_or(kShowNameDuration));
  }

  void OnIdentityAnimationTimeout() {
    --show_identity_request_count_;
    MaybeHideIdentityAnimation();
  }

  // Called after the user interacted with the button or after some timeout.
  void MaybeHideIdentityAnimation() {
    if (show_identity_request_count_ > 0) {
      return;
    }

    // Keep identity visible if this button is in use (hovered or has focus) or
    // has an associated In-Product-Help promo. We should not move things around
    // when the user wants to click on `this` or another button in the parent.
    if (avatar_toolbar_button_->IsMouseHovered() ||
        avatar_toolbar_button_->HasFocus() || has_in_product_help_promo_) {
      return;
    }

    Clear();
  }

  // Clears the effects of the state being active.
  void Clear() {
    show_identity_request_count_ = 0;
    waiting_for_image_ = false;
    show_identity_request_count_ = false;
    has_in_product_help_promo_ = false;

    RequestUpdate();
  }

  const raw_ref<Profile> profile_;
  const raw_ref<const AvatarToolbarButton> avatar_toolbar_button_;

  // Count of the show identity pill name timeouts that are currently scheduled.
  // Multiple timeouts are scheduled when multiple show requests triggers happen
  // in a quick sequence (before the first timeout passes). The identity pill
  // tries to close when this reaches 0.
  int show_identity_request_count_ = 0;
  bool waiting_for_image_ = false;
  bool has_in_product_help_promo_ = false;
  bool refresh_tokens_loaded_ = false;

  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};
  base::ScopedObservation<AvatarToolbarButton, AvatarToolbarButton::Observer>
      avatar_button_observation_{this};

  base::WeakPtrFactory<ShowIdentityNameStateProvider> weak_ptr_factory_{this};
};

// This provider observes sync errors (including transport mode). It can be
// configured to listen to a specific error with `sync_error_type`, or to all
// errors by passing nullopt. That way specific `SyncErrorStateProvider`s can
// handle some sync errors, while a generic `SyncErrorStateProvider` with
// lower priority can handle the remaining errors.
class SyncErrorStateProvider : public StateProvider,
                               public syncer::SyncServiceObserver {
 public:
  explicit SyncErrorStateProvider(
      StateObserver& state_observer,
      Profile& profile,
      std::optional<AvatarSyncErrorType> sync_error_type)
      : StateProvider(state_observer),
        profile_(profile),
        sync_error_type_(sync_error_type),
        last_avatar_error_(::GetAvatarSyncErrorType(&profile)) {
    if (auto* sync_service = SyncServiceFactory::GetForProfile(&profile)) {
      sync_service_observation_.Observe(sync_service);
    }
  }

  // StateProvider:
  bool IsActive() const override {
    return SyncServiceFactory::IsSyncAllowed(&profile_.get()) &&
           HasError(last_avatar_error_);
  }

  // Returns the last sync error if it matches the requested type. Returns
  // std::nullopt if there is no error or if the error does not match
  // `sync_error_type_`.
  std::optional<AvatarSyncErrorType> GetLastAvatarSyncErrorType() const {
    return HasError(last_avatar_error_) ? last_avatar_error_ : std::nullopt;
  }

 private:
  // StateProvider:
  void accept(StateVisitor& visitor) const override { visitor.visit(this); }

  // syncer::SyncServiceObserver:
  void OnStateChanged(syncer::SyncService*) override {
    const std::optional<AvatarSyncErrorType> error =
        ::GetAvatarSyncErrorType(&profile_.get());
    if (last_avatar_error_ == error) {
      return;
    }

    bool previous_error_state = HasError(last_avatar_error_);
    bool new_error_state = HasError(error);
    last_avatar_error_ = error;

    if (previous_error_state == new_error_state) {
      return;
    }

    RequestUpdate();
  }

  void OnSyncShutdown(syncer::SyncService*) override {
    sync_service_observation_.Reset();
  }

  // Returns true if `avatar_sync_error` has a value and the value matches
  // `sync_error_type_`. If `sync_error_type_` is std::nullopt then any
  // non-nullopt `avatar_sync_error` is a match.
  bool HasError(
      const std::optional<AvatarSyncErrorType>& avatar_sync_error) const {
    if (!avatar_sync_error) {
      return false;  // No sync error.
    }

    if (sync_error_type_.has_value() && avatar_sync_error != sync_error_type_) {
      return false;  // Error has the wrong type.
    }

    return true;
  }

  raw_ref<Profile> profile_;

  // std::nullopt to be active on all errors.
  const std::optional<AvatarSyncErrorType> sync_error_type_;

  // Caches the value of the last error so the class can detect when it
  // changes and notify changes.
  std::optional<AvatarSyncErrorType> last_avatar_error_;

  base::ScopedObservation<syncer::SyncService, syncer::SyncServiceObserver>
      sync_service_observation_{this};
};

const void* const kSigninPendingTimestampStartKey =
    &kSigninPendingTimestampStartKey;

// Helper struct to store a `base::TimeTicks` as a Profile user data.
struct TimeStampData : public base::SupportsUserData::Data {
  explicit TimeStampData(base::Time time) : time_(time) {}
  base::Time time_;
};

// This state has two modes when active; extended and collapsed. This states is
// active when the Signed in account is in error. Based on the source of the
// error, a mode is active:
// - collapsed: error originates from a web signout action from the user, the
// avatar button will not show a text.
// - extended version: any other error or after 50 minutes past a web signout or
// on Chrome restart, the button will extend to show a "Verify it's you" text.
//
// In both modes, the avatar icon is shrunk slightly and surrounded by a dotted
// circle to show the pending state.
class SigninPendingStateProvider : public StateProvider,
                                   public signin::IdentityManager::Observer {
 public:
  explicit SigninPendingStateProvider(
      StateObserver& state_observer,
      Profile& profile,
      const AvatarToolbarButton& avatar_toolbar_button)
      : StateProvider(state_observer),
        profile_(profile),
        identity_manager_(*IdentityManagerFactory::GetForProfile(&profile)),
        avatar_toolbar_button_(avatar_toolbar_button) {
    identity_manager_observation_.Observe(&identity_manager_.get());

    TimeStampData* signed_in_pending_delay_start = static_cast<TimeStampData*>(
        profile.GetUserData(kSigninPendingTimestampStartKey));
    // If a delay to show the pending state text was already started by another
    // browser, start one with the remaining time.
    if (signed_in_pending_delay_start) {
      base::TimeDelta elapsed_delay_time =
          base::Time::Now() - signed_in_pending_delay_start->time_;
      const base::TimeDelta delay =
          g_show_signin_pending_text_delay_for_testing.value_or(
              kShowSigninPendingTextDelay);
      if (elapsed_delay_time < delay) {
        StartTimerDelay(delay - elapsed_delay_time);
      } else {
        // This can happen if all browsers were closed when the delay expired,
        // and the cleanup task could not be run. Remove the user data now.
        profile_->RemoveUserData(kSigninPendingTimestampStartKey);
      }
    }
  }

  // StateProvider:
  bool IsActive() const override {
    CoreAccountId primary_account_id =
        identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
    if (primary_account_id.empty()) {
      return false;
    }

    return identity_manager_->HasAccountWithRefreshTokenInPersistentErrorState(
        primary_account_id);
  }

  // Only show the text when the delay timer is not running.
  bool ShouldShowText() const { return !display_text_delay_timer_.IsRunning(); }

  void ForceTimerTimeoutForTesting() {
    display_text_delay_timer_.FireNow();
    display_text_delay_timer_.Stop();
  }

 private:
  // StateProvider:
  void accept(StateVisitor& visitor) const override { visitor.visit(this); }

  // signin::IdentityManager::Observer:
  void OnErrorStateOfRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info,
      const GoogleServiceAuthError& error,
      signin_metrics::SourceForRefreshTokenOperation token_operation_source)
      override {
    if (account_info != identity_manager_->GetPrimaryAccountInfo(
                            signin::ConsentLevel::kSignin)) {
      return;
    }

    if (!error.IsPersistentError() && display_text_delay_timer_.IsRunning()) {
      // Clear timer and make it reaches the end. Next update should make the
      // state inactive.
      display_text_delay_timer_.Reset();
      OnTimerDelayReached();
      return;
    }

    if (error.state() ==
            GoogleServiceAuthError::State::INVALID_GAIA_CREDENTIALS &&
        token_operation_source ==
            signin_metrics::SourceForRefreshTokenOperation::
                kDiceResponseHandler_Signout) {
      profile_->SetUserData(kSigninPendingTimestampStartKey,
                            std::make_unique<TimeStampData>(base::Time::Now()));
      StartTimerDelay(g_show_signin_pending_text_delay_for_testing.value_or(
          kShowSigninPendingTextDelay));
    }

    RequestUpdate();
  }

  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override {
    RequestUpdate();
  }

  void OnIdentityManagerShutdown(signin::IdentityManager*) override {
    identity_manager_observation_.Reset();
  }

  void StartTimerDelay(base::TimeDelta delay) {
    display_text_delay_timer_.Start(
        FROM_HERE, delay,
        base::BindOnce(&SigninPendingStateProvider::OnTimerDelayReached,
                       // Unretained is fine here since the object owns the
                       // timer which will not fire if destroyed.
                       base::Unretained(this)));
  }

  void OnTimerDelayReached() {
    profile_->RemoveUserData(kSigninPendingTimestampStartKey);
    RequestUpdate();
  }

  raw_ref<Profile> profile_;
  raw_ref<signin::IdentityManager> identity_manager_;
  const raw_ref<const AvatarToolbarButton> avatar_toolbar_button_;

  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};

  base::OneShotTimer display_text_delay_timer_;
};

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
class ManagementStateProvider : public StateProvider,
                                public ProfileAttributesStorage::Observer,
                                public BrowserListObserver {
 public:
  explicit ManagementStateProvider(
      StateObserver& state_observer,
      Profile& profile,
      const AvatarToolbarButton& avatar_toolbar_button)
      : StateProvider(state_observer),
        profile_(profile),
        avatar_toolbar_button_(avatar_toolbar_button) {
    BrowserList::AddObserver(this);
    profile_observation_.Observe(&GetProfileAttributesStorage());

    profile_pref_change_registrar_.Init(profile_->GetPrefs());
    profile_pref_change_registrar_.Add(
        prefs::kEnterpriseCustomLabelForProfile,
        base::BindRepeating(&ManagementStateProvider::RequestUpdate,
                            weak_ptr_factory_.GetWeakPtr()));
    profile_pref_change_registrar_.Add(
        prefs::kEnterpriseProfileBadgeToolbarSettings,
        base::BindRepeating(&ManagementStateProvider::RequestUpdate,
                            weak_ptr_factory_.GetWeakPtr()));
  }

  ~ManagementStateProvider() override { BrowserList::RemoveObserver(this); }

  // StateProvider:
  bool IsActive() const override {
    return enterprise_util::CanShowEnterpriseBadgingForAvatar(&profile_.get());
  }

 private:
  // StateProvider:
  void accept(StateVisitor& visitor) const override { visitor.visit(this); }

  void OnBrowserAdded(Browser*) override {
    // This is required so that the enterprise text is shown when a profile is
    // opened.
    RequestUpdate();
  }

  // ProfileAttributesStorage::Observer:
  void OnProfileUserManagementAcceptanceChanged(
      const base::FilePath& profile_path) override {
    RequestUpdate();
  }

  raw_ref<Profile> profile_;
  const raw_ref<const AvatarToolbarButton> avatar_toolbar_button_;

  PrefChangeRegistrar profile_pref_change_registrar_;

  base::ScopedObservation<ProfileAttributesStorage,
                          ProfileAttributesStorage::Observer>
      profile_observation_{this};

  base::WeakPtrFactory<ManagementStateProvider> weak_ptr_factory_{this};
};
#endif

// Regular State, should always have the lowest priority.
class NormalStateProvider : public StateProvider {
 public:
  explicit NormalStateProvider(StateObserver& state_observer)
      : StateProvider(state_observer) {}

  // Normal state is always active.
  bool IsActive() const override { return true; }
};

// Allows getting the underlying implementation of `StateProvider` given a
// generic `StateProvider`.
class StateProviderGetter : public StateVisitor {
 public:
  explicit StateProviderGetter(const StateProvider& state_provider) {
    state_provider.accept(*this);
  }

  const ExplicitStateProvider* AsExplicit() { return explicit_state_; }
  const SyncErrorStateProvider* AsSyncError() { return sync_error_state_; }
  const SigninPendingStateProvider* AsSigninPending() {
    return signin_pending_state_;
  }
  const ShowIdentityNameStateProvider* AsShowIdentity() {
    return show_identity_state_;
  }
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  const ManagementStateProvider* AsManagement() { return management_state_; }
#endif

 private:
  void visit(const ExplicitStateProvider* state_provider) override {
    explicit_state_ = state_provider;
  }

  void visit(const SyncErrorStateProvider* state_provider) override {
    sync_error_state_ = state_provider;
  }

  void visit(const SigninPendingStateProvider* state_provider) override {
    signin_pending_state_ = state_provider;
  }
  void visit(const ShowIdentityNameStateProvider* state_provider) override {
    show_identity_state_ = state_provider;
  }
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  void visit(const ManagementStateProvider* state_provider) override {
    management_state_ = state_provider;
  }
#endif

  raw_ptr<const ExplicitStateProvider> explicit_state_ = nullptr;
  raw_ptr<const SyncErrorStateProvider> sync_error_state_ = nullptr;
  raw_ptr<const SigninPendingStateProvider> signin_pending_state_ = nullptr;
  raw_ptr<const ShowIdentityNameStateProvider> show_identity_state_ = nullptr;
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  raw_ptr<const ManagementStateProvider> management_state_ = nullptr;
#endif
};

}  // namespace

// Container of all the states and returns the active state with the highest
// priority.
// All states are initialized at construction based on the Profile type.
// Exception for `ButtonState::kExplicitTextShowing` with
// `ExplicitStateProvider`  which is the only state that can be added
// dynamically and controlled externally. It has to be part of the
// `StateManager` however to properly compute the current active state.
// This class also listens to Profile changes that should affect the global
// state of the button, for chanhges that should occur regardless of the current
// active state for Regular Profiles.
class StateManager : public StateObserver,
                     public signin::IdentityManager::Observer,
                     public ProfileAttributesStorage::Observer {
 public:
  explicit StateManager(AvatarToolbarButton& avatar_toolbar_button,
                        Browser* browser)
      : avatar_toolbar_button_(avatar_toolbar_button) {
    // Creates the main states and listeners.
    CreateStatesAndListeners(browser);
    ComputeButtonActiveState();
  }
  ~StateManager() override = default;

  // This needs to be separated from the constructor since it might call
  // updates, which will try to access the `StateManager`.
  void InitializeStates() {
    // States should initialize here, making sure that this should happen after
    // all main states are created. This would allow the `Init()` functions of
    // state to call `ComputeButtonActiveState()`. If this was done in their
    // constructor there could be a chance that no active state exist yet.
    for (auto& state : states_) {
      state.second->Init();
    }
    ComputeButtonActiveState();
  }

  ButtonState GetButtonActiveState() const {
    return current_active_state_pair_->first;
  }

  // To be used with `StateProviderGetter` to get more useful type.
  const StateProvider* GetActiveStateProvider() const {
    return current_active_state_pair_->second.get();
  }

  // Special setter for the explicit state as it is controlled externally.
  void SetExplicitStateProvider(
      std::unique_ptr<ExplicitStateProvider> explicit_state_provider) {
    if (auto it = states_.find(ButtonState::kExplicitTextShowing);
        it != states_.end()) {
      // Attempt to clear existing states if not already done.
      static_cast<ExplicitStateProvider*>(it->second.get())->Clear();
    }

    // Invalidate the pointer as the map will reorder it's element when adding a
    // new state and the pointer will not be valid anymore. The value will be
    // set later again with `ComputeButtonActiveState()`.
    current_active_state_pair_ = nullptr;
    // Add the new state.
    states_[ButtonState::kExplicitTextShowing] =
        std::move(explicit_state_provider);

    // Recompute the button active state after adding a new state.
    ComputeButtonActiveState();
    UpdateButtonText();
  }

 private:
  // Creates all main states and attach listeners.
  void CreateStatesAndListeners(Browser* browser) {
    // Add each possible state for each Profile type or browser configuration,
    // since this structure is tied to Browser, in which a Profile cannot
    // change, it is correct to initialize the possible fixed states once.

    // Web app has limited toolbar space, thus always show kNormal state.
    if (web_app::AppBrowserController::IsWebApp(browser)) {
      // This state is always active.
      states_[ButtonState::kNormal] =
          std::make_unique<NormalStateProvider>(/*state_observer=*/*this);
      return;
    }

    Profile* profile = browser->profile();
    if (profile->IsRegularProfile()) {
      states_[ButtonState::kShowIdentityName] =
          std::make_unique<ShowIdentityNameStateProvider>(
              /*state_observer=*/*this, *profile, avatar_toolbar_button_.get());

      if (switches::IsImprovedSigninUIOnDesktopEnabled()) {
        states_[ButtonState::kUpgradeClientError] =
            std::make_unique<SyncErrorStateProvider>(
                /*state_observer=*/*this, *profile,
                AvatarSyncErrorType::kUpgradeClientError);
        states_[ButtonState::kPassphraseError] =
            std::make_unique<SyncErrorStateProvider>(
                /*state_observer=*/*this, *profile,
                AvatarSyncErrorType::kPassphraseError);
      }

      if (AccountConsistencyModeManager::IsDiceEnabledForProfile(profile)) {
        states_[ButtonState::kSyncPaused] =
            std::make_unique<SyncErrorStateProvider>(
                /*state_observer=*/*this, *profile,
                AvatarSyncErrorType::kSyncPaused);
      }

      // Generic catch-all providers for sync errors not handled by higher
      // priority providers.
      states_[ButtonState::kSyncError] =
          std::make_unique<SyncErrorStateProvider>(
              /*state_observer=*/*this, *profile,
              /*sync_error_type=*/std::nullopt);

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
      if (base::FeatureList::IsEnabled(
              features::kEnterpriseProfileBadgingForAvatar) ||
          base::FeatureList::IsEnabled(
              features::kEnterpriseProfileBadgingPolicies)) {
        // Contains both Work and School.
        states_[ButtonState::kManagement] =
            std::make_unique<ManagementStateProvider>(
                /*state_observer=*/*this, *profile,
                avatar_toolbar_button_.get());
      }

      if (switches::IsExplicitBrowserSigninUIOnDesktopEnabled()) {
        states_[ButtonState::kSigninPending] =
            std::make_unique<SigninPendingStateProvider>(
                /*state_observer=*/*this, *profile, *avatar_toolbar_button_);
      }
#endif

      signin::IdentityManager* identity_manager =
          IdentityManagerFactory::GetForProfile(profile);
      scoped_identity_manager_observation_.Observe(identity_manager);
      if (identity_manager->AreRefreshTokensLoaded()) {
        OnRefreshTokensLoaded();
      }
      profile_observation_.Observe(&GetProfileAttributesStorage());

    } else if (profile->IsGuestSession()) {
      // This state is always active.
      states_[ButtonState::kGuestSession] =
          std::make_unique<PrivateStateProvider>(
              /*state_observer=*/*this);
    } else if (profile->IsIncognitoProfile()) {
      // This state is always active.
      states_[ButtonState::kIncognitoProfile] =
          std::make_unique<PrivateStateProvider>(
              /*state_observer=*/*this);
    }

    // This state is always active.
    states_[ButtonState::kNormal] =
        std::make_unique<NormalStateProvider>(/*state_observer=*/*this);
  }

  // StateObserver:
  void OnStateProviderUpdateRequest(StateProvider* requesting_state) override {
    if (!requesting_state->IsActive()) {
      // Updates goes through if the requesting state was the current button
      // active state, since we are now clearing it, otherwise we just ignore
      // the request.
      if (current_active_state_pair_->second.get() == requesting_state) {
        // Recompute the new button active state as we are clearing the
        // requesting state effects.
        ComputeButtonActiveState();
        // Always update the button since we do not know exactly which state
        // should now be active.
        UpdateAvatarButton();
      }
      return;
    }

    // Updates `current_active_state_`, and does not alter the states active
    // status. In that case, `requesting_state` remains active at this point but
    // is not necessarily the one with the highest priority.
    ComputeButtonActiveState();
    // Ignore the request if the requested state is not the button active one
    // because the requesting state despite being active, does not have the
    // highest current active priority, meaning that it's update request should
    // not have any effect.
    if (current_active_state_pair_->second.get() != requesting_state) {
      return;
    }
    UpdateAvatarButton();
  }

  // Computes the current active state with the highest priority.
  // Multiple states could be active at the same time.
  void ComputeButtonActiveState() {
    // Traverse the map of states sorted by their priority set in `ButtonState`.
    for (auto& state_pair : states_) {
      // Sets first state that is active.
      if (state_pair.second->IsActive()) {
        current_active_state_pair_ = &state_pair;
        return;
      }
    }

    NOTREACHED_IN_MIGRATION()
        << "There should at least be one active state in the map.";
  }

  // `AvatarToolbarButton::UpdateIcon()` will notify observers, the
  // `ShowIdentityNameStateProvider` being one of the observers.
  void UpdateButtonIcon() { avatar_toolbar_button_->UpdateIcon(); }

  void UpdateButtonText() { avatar_toolbar_button_->UpdateText(); }

  // This is mainly used `OnStateProviderUpdateRequest()` where currently only
  // one state transition needs the icon update. Consider adding a filter if
  // this impacting performance.
  void UpdateAvatarButton() {
    UpdateButtonText();
    UpdateButtonIcon();
  }

  // signin::IdentityManager::Observer:
  void OnIdentityManagerShutdown(signin::IdentityManager*) override {
    scoped_identity_manager_observation_.Reset();
  }

  void OnRefreshTokensLoaded() override { UpdateButtonIcon(); }

  void OnAccountsInCookieUpdated(const signin::AccountsInCookieJarInfo&,
                                 const GoogleServiceAuthError&) override {
    UpdateButtonIcon();
  }

  void OnExtendedAccountInfoUpdated(const AccountInfo&) override {
    UpdateButtonIcon();
  }

  void OnExtendedAccountInfoRemoved(const AccountInfo&) override {
    UpdateButtonIcon();
  }

  //  ProfileAttributesStorage::Observer:
  void OnProfileAvatarChanged(const base::FilePath&) override {
    UpdateButtonIcon();
  }

  void OnProfileHighResAvatarLoaded(const base::FilePath&) override {
    UpdateButtonIcon();
  }

  void OnProfileNameChanged(const base::FilePath&,
                            const std::u16string&) override {
    UpdateButtonText();
  }

  base::flat_map<ButtonState, std::unique_ptr<StateProvider>> states_;
  raw_ref<AvatarToolbarButton> avatar_toolbar_button_;

  // Active state per the last request to `ComputeButtonActiveState()`.
  // Pointer to the active element of `states_` with the highest priority.
  raw_ptr<std::pair<ButtonState, std::unique_ptr<StateProvider>>>
      current_active_state_pair_ = nullptr;

  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      scoped_identity_manager_observation_{this};
  base::ScopedObservation<ProfileAttributesStorage,
                          ProfileAttributesStorage::Observer>
      profile_observation_{this};
};

}  // namespace internal

using ButtonState = internal::ButtonState;
using ExplicitStateProvider = internal::ExplicitStateProvider;

AvatarToolbarButtonDelegate::AvatarToolbarButtonDelegate(
    AvatarToolbarButton* button,
    Browser* browser)
    : avatar_toolbar_button_(button),
      browser_(browser),
      profile_(browser->profile()),
      identity_manager_(
          IdentityManagerFactory::GetForProfile(browser->profile())) {
  if (identity_manager_) {
    identity_manager_observation_.Observe(identity_manager_);
  }
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // On CrOS this button should only show as badging for Incognito, Guest and
  // captivie portal signin. It's only enabled for non captive portal Incognito
  // where a menu is available for closing all Incognito windows.
  avatar_toolbar_button_->SetEnabled(
      profile_->IsOffTheRecord() && !profile_->IsGuestSession() &&
      !profile_->GetOTRProfileID().IsCaptivePortal());
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  // On Lacros we need to disable the button for captivie portal signin.
  avatar_toolbar_button_->SetEnabled(
      !profile_->IsOffTheRecord() || profile_->IsGuestSession() ||
      !profile_->GetOTRProfileID().IsCaptivePortal());
#endif  // !BUILDFLAG(IS_CHROMEOS)
}

AvatarToolbarButtonDelegate::~AvatarToolbarButtonDelegate() = default;

void AvatarToolbarButtonDelegate::InitializeStateManager() {
  CHECK(!state_manager_);
  state_manager_ = std::make_unique<internal::StateManager>(
      *avatar_toolbar_button_, browser_);
  state_manager_->InitializeStates();
}

bool AvatarToolbarButtonDelegate::IsStateManagerInitialized() const {
  return state_manager_.get() != nullptr;
}

std::u16string AvatarToolbarButtonDelegate::GetProfileName() const {
  DCHECK_NE(state_manager_->GetButtonActiveState(),
            ButtonState::kIncognitoProfile);
  return profiles::GetAvatarNameForProfile(profile_->GetPath());
}

std::u16string AvatarToolbarButtonDelegate::GetShortProfileName() const {
  ProfileAttributesEntry* entry = GetProfileAttributesEntry(profile_);
  // If the profile is being deleted, it doesn't matter what name is shown.
  if (!entry) {
    return std::u16string();
  }

  return signin_ui_util::GetShortProfileIdentityToDisplay(*entry, profile_);
}

gfx::Image AvatarToolbarButtonDelegate::GetGaiaAccountImage() const {
  return ::GetGaiaAccountImage(profile_);
}

gfx::Image AvatarToolbarButtonDelegate::GetProfileAvatarImage(
    int preferred_size) const {
  ProfileAttributesEntry* entry = GetProfileAttributesEntry(profile_);
  if (!entry) {  // This can happen if the user deletes the current profile.
    return ui::ResourceBundle::GetSharedInstance().GetImageNamed(
        profiles::GetPlaceholderAvatarIconResourceID());
  }

  // TODO(crbug.com/40102223): it should suffice to call entry->GetAvatarIcon().
  // For this to work well, this class needs to observe ProfileAttributesStorage
  // instead of (or on top of) IdentityManager. Only then we can rely on |entry|
  // being up to date (as the storage also observes IdentityManager so there's
  // no guarantee on the order of notifications).
  if (entry->IsUsingGAIAPicture() && entry->GetGAIAPicture()) {
    return *entry->GetGAIAPicture();
  }

  // Show |user_identity_image| when the following conditions are satisfied:
  //  - the user is migrated to Dice
  //  - the user isn't syncing
  //  - the profile icon wasn't explicitly changed
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);
  gfx::Image gaia_account_image = GetGaiaAccountImage();
  if (!gaia_account_image.IsEmpty() &&
      AccountConsistencyModeManager::IsDiceEnabledForProfile(profile_) &&
      !identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync) &&
      entry->IsUsingDefaultAvatar()) {
    return gaia_account_image;
  }

  return entry->GetAvatarIcon(preferred_size, /*use_high_res_file=*/true,
                              GetPlaceholderAvatarIconParamsDependingOnTheme(
                                  ThemeServiceFactory::GetForProfile(profile_),
                                  /*background_color_id=*/kColorToolbar,
                                  *avatar_toolbar_button_->GetColorProvider()));
}

int AvatarToolbarButtonDelegate::GetWindowCount() const {
  if (profile_->IsGuestSession()) {
    return BrowserList::GetGuestBrowserCount();
  }
  DCHECK(profile_->IsOffTheRecord());
  return BrowserList::GetOffTheRecordBrowsersActiveForProfile(profile_);
}

void AvatarToolbarButtonDelegate::OnThemeChanged(
    const ui::ColorProvider* color_provider) {
  // Update avatar color information in profile attributes.
  if (profile_->IsOffTheRecord() || profile_->IsGuestSession()) {
    return;
  }

  // Do not update the profile theme colors if the current browser window is a
  // web app.
  if (web_app::AppBrowserController::IsWebApp(browser_)) {
    return;
  }

  ProfileAttributesEntry* entry = GetProfileAttributesEntry(profile_);
  if (!entry) {
    return;
  }

  ThemeService* service = ThemeServiceFactory::GetForProfile(profile_);
  if (!service || !color_provider) {
    return;
  }

  // Use default profile colors only for extension and system themes.
  entry->SetProfileThemeColors(
      ShouldUseDefaultProfileColors(*service)
          ? GetDefaultProfileThemeColors(color_provider)
          : GetCurrentProfileThemeColors(*color_provider, *service));
}

base::ScopedClosureRunner AvatarToolbarButtonDelegate::ShowExplicitText(
    const std::u16string& new_text,
    std::optional<std::u16string> accessibility_label) {
  CHECK(!new_text.empty());

  // Create the new explicit state with the clear text callback.
  std::unique_ptr<ExplicitStateProvider> explicit_state_provider =
      std::make_unique<ExplicitStateProvider>(
          /*state_observer=*/*state_manager_, new_text, accessibility_label);

  ExplicitStateProvider* explicit_state_provider_ptr =
      explicit_state_provider.get();
  // Activate the state.
  state_manager_->SetExplicitStateProvider(std::move(explicit_state_provider));

  return base::ScopedClosureRunner(
      base::BindOnce(&ExplicitStateProvider::Clear,
                     // WeakPtr is needed here since this state could be
                     // replaced before the call to the closure.
                     explicit_state_provider_ptr->GetWeakPtr()));
}

std::pair<std::u16string, std::optional<SkColor>>
AvatarToolbarButtonDelegate::GetTextAndColor(
    const ui::ColorProvider* const color_provider) const {
  std::optional<SkColor> color =
      color_provider->GetColor(kColorAvatarButtonHighlightDefault);
  std::u16string text;
  switch (state_manager_->GetButtonActiveState()) {
    case ButtonState::kIncognitoProfile: {
      const int incognito_window_count = GetWindowCount();
      avatar_toolbar_button_->GetViewAccessibility().SetName(
          l10n_util::GetPluralStringFUTF16(
              IDS_INCOGNITO_BUBBLE_ACCESSIBLE_TITLE, incognito_window_count));
      text = l10n_util::GetPluralStringFUTF16(IDS_AVATAR_BUTTON_INCOGNITO,
                                              incognito_window_count);
      color = color_provider->GetColor(kColorAvatarButtonHighlightIncognito);
      break;
    }
    case ButtonState::kShowIdentityName:
      text = switches::IsExplicitBrowserSigninUIOnDesktopEnabled()
                 ? l10n_util::GetStringFUTF16(IDS_AVATAR_BUTTON_GREETING,
                                              GetShortProfileName())
                 : GetShortProfileName();
      break;
    case ButtonState::kExplicitTextShowing: {
      const internal::ExplicitStateProvider* explicit_state =
          internal::StateProviderGetter(
              *state_manager_->GetActiveStateProvider())
              .AsExplicit();
      CHECK(explicit_state);
      text = explicit_state->GetText();
      color = color_provider->GetColor(kColorAvatarButtonHighlightExplicitText);
      break;
    }
    case ButtonState::kSyncPaused:
      color = color_provider->GetColor(kColorAvatarButtonHighlightSyncPaused);
      text = l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SYNC_PAUSED);
      break;
    case ButtonState::kUpgradeClientError:
      color = color_provider->GetColor(kColorAvatarButtonHighlightSyncPaused);
      text = l10n_util::GetStringUTF16(IDS_SYNC_ERROR_USER_MENU_UPGRADE_BUTTON);
      break;
    case ButtonState::kPassphraseError:
      color = color_provider->GetColor(kColorAvatarButtonHighlightSyncPaused);
      text =
          l10n_util::GetStringUTF16(IDS_SYNC_ERROR_USER_MENU_PASSPHRASE_BUTTON);
      break;
    case ButtonState::kSyncError:
      if (!IdentityManagerFactory::GetForProfile(profile_)->HasPrimaryAccount(
              signin::ConsentLevel::kSync) &&
          switches::IsImprovedSigninUIOnDesktopEnabled()) {
        color =
            color_provider->GetColor(kColorAvatarButtonHighlightSigninPaused);
        text = l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SIGNIN_PAUSED);
      } else {
        color = color_provider->GetColor(kColorAvatarButtonHighlightSyncError);
        text = l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SYNC_ERROR);
      }
      break;
    case ButtonState::kSigninPending: {
      const internal::SigninPendingStateProvider* signin_pending_state =
          internal::StateProviderGetter(
              *state_manager_->GetActiveStateProvider())
              .AsSigninPending();
      CHECK(signin_pending_state);
      if (signin_pending_state->ShouldShowText()) {
        color =
            color_provider->GetColor(kColorAvatarButtonHighlightSigninPaused);
        text = l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SIGNIN_PAUSED);
      }
    } break;
    case ButtonState::kGuestSession: {
#if BUILDFLAG(IS_CHROMEOS_ASH)
      // On ChromeOS all windows are either Guest or not Guest and the Guest
      // avatar button is not actionable. Showing the number of open windows is
      // not as helpful as on other desktop platforms. Please see
      // crbug.com/1178520.
      const int guest_window_count = 1;
#else
      const int guest_window_count = GetWindowCount();
#endif
      avatar_toolbar_button_->GetViewAccessibility().SetName(
          l10n_util::GetPluralStringFUTF16(IDS_GUEST_BUBBLE_ACCESSIBLE_TITLE,
                                           guest_window_count));
      text = l10n_util::GetPluralStringFUTF16(IDS_AVATAR_BUTTON_GUEST,
                                              guest_window_count);
      break;
    }
    case ButtonState::kManagement: {
      const std::string enterprise_custom_label =
          profile_->GetPrefs()->GetString(
              prefs::kEnterpriseCustomLabelForProfile);
      if (!enterprise_custom_label.empty()) {
        text = base::UTF8ToUTF16(enterprise_custom_label);
      } else if (IsManagementWork(profile_)) {
        text = l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_WORK);
      } else {
        // School.
        text = l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SCHOOL);
      }
      color = color_provider->GetColor(kColorAvatarButtonHighlightNormal);
      break;
    }
    case ButtonState::kNormal:
      break;
  }

  return {text, color};
}

std::optional<std::u16string>
AvatarToolbarButtonDelegate::GetAccessibilityLabel() const {
  std::optional<std::u16string> accessibility_label;

  switch (state_manager_->GetButtonActiveState()) {
    case ButtonState::kGuestSession:
    case ButtonState::kShowIdentityName:
    case ButtonState::kIncognitoProfile:
    case ButtonState::kManagement:
    case ButtonState::kUpgradeClientError:
    case ButtonState::kPassphraseError:
    case ButtonState::kSyncError:
    case ButtonState::kSyncPaused:
    case ButtonState::kNormal:
      break;
    case ButtonState::kSigninPending: {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
      const internal::SigninPendingStateProvider* signin_pending_state =
          internal::StateProviderGetter(
              *state_manager_->GetActiveStateProvider())
              .AsSigninPending();
      CHECK(signin_pending_state);
      accessibility_label = l10n_util::GetStringUTF16(
          IDS_AVATAR_BUTTON_SIGNIN_PENDING_ACCESSIBILITY_LABEL);
#endif
      break;
    }
    case ButtonState::kExplicitTextShowing:
      const internal::ExplicitStateProvider* explicit_state =
          internal::StateProviderGetter(
              *state_manager_->GetActiveStateProvider())
              .AsExplicit();
      CHECK(explicit_state);
      accessibility_label = explicit_state->GetAccessibiltyLabel();
      break;
  }

  return accessibility_label;
}

SkColor AvatarToolbarButtonDelegate::GetHighlightTextColor(
    const ui::ColorProvider* const color_provider) const {
  switch (state_manager_->GetButtonActiveState()) {
    case ButtonState::kIncognitoProfile:
      return color_provider->GetColor(
          kColorAvatarButtonHighlightIncognitoForeground);
    case ButtonState::kSyncError:
      if (IdentityManagerFactory::GetForProfile(profile_)->HasPrimaryAccount(
              signin::ConsentLevel::kSync) ||
          !switches::IsImprovedSigninUIOnDesktopEnabled()) {
        return color_provider->GetColor(
            kColorAvatarButtonHighlightSyncErrorForeground);
      }
      [[fallthrough]];
    case ButtonState::kManagement:
    case ButtonState::kSigninPending:
    case ButtonState::kUpgradeClientError:
    case ButtonState::kPassphraseError:
    case ButtonState::kSyncPaused:
      return color_provider->GetColor(
          kColorAvatarButtonHighlightNormalForeground);
    case ButtonState::kExplicitTextShowing:
    case ButtonState::kGuestSession:
    case ButtonState::kShowIdentityName:
    case ButtonState::kNormal:
      return color_provider->GetColor(
          kColorAvatarButtonHighlightDefaultForeground);
  }
}

std::u16string AvatarToolbarButtonDelegate::GetAvatarTooltipText() const {
  switch (state_manager_->GetButtonActiveState()) {
    case ButtonState::kIncognitoProfile:
      return l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_INCOGNITO_TOOLTIP);
    case ButtonState::kGuestSession:
      return l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_GUEST_TOOLTIP);
    case ButtonState::kShowIdentityName:
      return GetShortProfileName();
    case ButtonState::kUpgradeClientError:
    case ButtonState::kPassphraseError:
    case ButtonState::kSyncPaused:
    case ButtonState::kSyncError: {
      const internal::SyncErrorStateProvider* sync_error_state =
          internal::StateProviderGetter(
              *state_manager_->GetActiveStateProvider())
              .AsSyncError();
      CHECK(sync_error_state);
      std::optional<AvatarSyncErrorType> sync_error =
          sync_error_state->GetLastAvatarSyncErrorType();
      CHECK(sync_error.has_value());
      return l10n_util::GetStringFUTF16(
          IDS_AVATAR_BUTTON_SYNC_ERROR_TOOLTIP, GetShortProfileName(),
          GetAvatarSyncErrorDescription(
              *sync_error,
              IdentityManagerFactory::GetForProfile(profile_)
                  ->HasPrimaryAccount(signin::ConsentLevel::kSync)));
    }
    case ButtonState::kSigninPending:
    case ButtonState::kExplicitTextShowing:
    case ButtonState::kManagement:
    case ButtonState::kNormal:
      return GetProfileName();
  }
}

std::pair<ChromeColorIds, ChromeColorIds>
AvatarToolbarButtonDelegate::GetInkdropColors() const {
  ChromeColorIds hover_color_id = kColorToolbarInkDropHover;
  ChromeColorIds ripple_color_id = kColorToolbarInkDropRipple;

  if (avatar_toolbar_button_->IsLabelPresentAndVisible()) {
    switch (state_manager_->GetButtonActiveState()) {
      case ButtonState::kIncognitoProfile:
        hover_color_id = kColorAvatarButtonIncognitoHover;
        break;
      case ButtonState::kGuestSession:
      case ButtonState::kNormal:
      case ButtonState::kExplicitTextShowing:
      case ButtonState::kShowIdentityName:
        break;
      case ButtonState::kSyncError:
        if (IdentityManagerFactory::GetForProfile(profile_)->HasPrimaryAccount(
                signin::ConsentLevel::kSync) ||
            !switches::IsImprovedSigninUIOnDesktopEnabled()) {
          break;
        }
        [[fallthrough]];
      case ButtonState::kManagement:
      case ButtonState::kSigninPending:
      case ButtonState::kSyncPaused:
      case ButtonState::kUpgradeClientError:
      case ButtonState::kPassphraseError:
        ripple_color_id = kColorAvatarButtonNormalRipple;
        break;
    }
  }

  return {hover_color_id, ripple_color_id};
}

ui::ImageModel AvatarToolbarButtonDelegate::GetAvatarIcon(
    int icon_size,
    SkColor icon_color) const {
  switch (state_manager_->GetButtonActiveState()) {
    case ButtonState::kIncognitoProfile:
      return ui::ImageModel::FromVectorIcon(kIncognitoRefreshMenuIcon,
                                            icon_color, icon_size);
    case ButtonState::kGuestSession:
      return profiles::GetGuestAvatar(icon_size);
    case ButtonState::kExplicitTextShowing:
    case ButtonState::kShowIdentityName:
    // TODO(crbug.com/40756583): If sync-the-feature is disabled, the icon
    // should be different.
    case ButtonState::kSyncPaused:
    case ButtonState::kManagement:
    case ButtonState::kNormal:
      return ui::ImageModel::FromImage(profiles::GetSizedAvatarIcon(
          GetProfileAvatarImage(icon_size), icon_size, icon_size,
          profiles::SHAPE_CIRCLE));
    case ButtonState::kSyncError:
      if (IdentityManagerFactory::GetForProfile(profile_)->HasPrimaryAccount(
              signin::ConsentLevel::kSync) ||
          !switches::IsImprovedSigninUIOnDesktopEnabled()) {
        return ui::ImageModel::FromImage(profiles::GetSizedAvatarIcon(
            GetProfileAvatarImage(icon_size), icon_size, icon_size,
            profiles::SHAPE_CIRCLE));
      }
      [[fallthrough]];
    case ButtonState::kPassphraseError:
    case ButtonState::kUpgradeClientError:
    case ButtonState::kSigninPending:
      // First shrink the icon from it's regular size in order to accommodate
      // for the dotted circle that is drawn around it in `PaintIcon()`.
      int shrunk_icon_size = icon_size * kAvatarIconSigninPendingShrinkRatio;
      gfx::Image shrunk_image = profiles::GetSizedAvatarIcon(
          GetProfileAvatarImage(icon_size), shrunk_icon_size, shrunk_icon_size,
          profiles::SHAPE_CIRCLE);
      // Then add a transparent background to the image, with the original size.
      // This way the whole image is the same as the regular one (so that it
      // does not affect it's position or other elements such as the text next
      // to it). The transparent background will have the dotted paint on top of
      // it in `PaintIcon()`.
      return ui::ImageModel::FromImageSkia(
          gfx::ImageSkiaOperations::CreateImageWithCircleBackground(
              icon_size / 2, SK_ColorTRANSPARENT, shrunk_image.AsImageSkia()));
  }
}

bool AvatarToolbarButtonDelegate::ShouldPaintBorder() const {
  switch (state_manager_->GetButtonActiveState()) {
    case ButtonState::kGuestSession:
    case ButtonState::kShowIdentityName:
    case ButtonState::kNormal:
      return true;
    case ButtonState::kIncognitoProfile:
    case ButtonState::kExplicitTextShowing:
    case ButtonState::kManagement:
    case ButtonState::kSigninPending:
    case ButtonState::kUpgradeClientError:
    case ButtonState::kPassphraseError:
    case ButtonState::kSyncPaused:
    case ButtonState::kSyncError:
      return false;
  }
}

void AvatarToolbarButtonDelegate::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  // Try showing the IPH for signin preference remembered.
  if (event_details.GetEventTypeFor(signin::ConsentLevel::kSignin) !=
          signin::PrimaryAccountChangeEvent::Type::kSet ||
      event_details.GetSetPrimaryAccountAccessPoint() !=
          signin_metrics::AccessPoint::ACCESS_POINT_SIGNIN_CHOICE_REMEMBERED) {
    return;
  }

  std::string gaia_id = event_details.GetCurrentState().primary_account.gaia;
  const SigninPrefs signin_prefs(*profile_->GetPrefs());
  std::optional<base::Time> last_signout_time =
      signin_prefs.GetChromeLastSignoutTime(gaia_id);
  if (last_signout_time &&
      base::Time::Now() - last_signout_time.value() < base::Days(14)) {
    // Less than two weeks since the last sign out event.
    return;
  }

  AccountInfo account_info = identity_manager_->FindExtendedAccountInfo(
      event_details.GetCurrentState().primary_account);
  if (!account_info.given_name.empty()) {
    avatar_toolbar_button_
        ->MaybeShowExplicitBrowserSigninPreferenceRememberedIPH(account_info);
  } else {
    gaia_id_for_signin_choice_remembered_ = account_info.gaia;
  }
}

void AvatarToolbarButtonDelegate::OnExtendedAccountInfoUpdated(
    const AccountInfo& info) {
  if (info.gaia == gaia_id_for_signin_choice_remembered_ &&
      !info.given_name.empty()) {
    gaia_id_for_signin_choice_remembered_.clear();
    avatar_toolbar_button_
        ->MaybeShowExplicitBrowserSigninPreferenceRememberedIPH(info);
  }
}

void AvatarToolbarButtonDelegate::OnErrorStateOfRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info,
    const GoogleServiceAuthError& error,
    signin_metrics::SourceForRefreshTokenOperation token_operation_source) {
  if (base::FeatureList::IsEnabled(
          syncer::kReplaceSyncPromosWithSignInPromos) &&
      profile_->GetPrefs()->GetBoolean(prefs::kExplicitBrowserSignin) &&
      account_info == identity_manager_->GetPrimaryAccountInfo(
                          signin::ConsentLevel::kSignin) &&
      !identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSync) &&
      error.state() ==
          GoogleServiceAuthError::State::INVALID_GAIA_CREDENTIALS &&
      token_operation_source == signin_metrics::SourceForRefreshTokenOperation::
                                    kDiceResponseHandler_Signout) {
    avatar_toolbar_button_->MaybeShowWebSignoutIPH(account_info.gaia);
  }
}

void AvatarToolbarButtonDelegate::PaintIcon(
    gfx::Canvas* canvas,
    const gfx::Rect& icon_bounds) const {
  switch (state_manager_->GetButtonActiveState()) {
    case ButtonState::kGuestSession:
    case ButtonState::kShowIdentityName:
    case ButtonState::kNormal:
    case ButtonState::kIncognitoProfile:
    case ButtonState::kExplicitTextShowing:
    case ButtonState::kManagement:
    case ButtonState::kSyncPaused:
      return;
    case ButtonState::kSyncError:
      if (IdentityManagerFactory::GetForProfile(profile_)->HasPrimaryAccount(
              signin::ConsentLevel::kSync) ||
          !switches::IsImprovedSigninUIOnDesktopEnabled()) {
        return;
      }
      [[fallthrough]];
    case ButtonState::kSigninPending:
    case ButtonState::kUpgradeClientError:
    case ButtonState::kPassphraseError:
      // Paints the dotted circle around the shrunk icon (from
      // `GetAvatarIcon()`).
      PaintRingDottedPath(canvas, icon_bounds,
                          avatar_toolbar_button_->GetColorProvider()->GetColor(
                              kColorTabDiscardRingFrameActive));
      return;
  }
}

// static
base::AutoReset<std::optional<base::TimeDelta>>
AvatarToolbarButtonDelegate::CreateScopedInfiniteDelayOverrideForTesting(
    AvatarDelayType delay_type) {
  switch (delay_type) {
    case AvatarDelayType::kNameGreeting:
      return base::AutoReset<std::optional<base::TimeDelta>>(
          &g_show_name_duration_for_testing, kInfiniteTimeForTesting);
    case AvatarDelayType::kSigninPendingText:
      return base::AutoReset<std::optional<base::TimeDelta>>(
          &g_show_signin_pending_text_delay_for_testing,
          kInfiniteTimeForTesting);
  }
}

void AvatarToolbarButtonDelegate::TriggerTimeoutForTesting(
    AvatarDelayType delay_type) {
  switch (delay_type) {
    case AvatarDelayType::kNameGreeting:
      if (state_manager_->GetButtonActiveState() ==
          ButtonState::kShowIdentityName) {
        internal::ShowIdentityNameStateProvider* show_identity_state =
            const_cast<internal::ShowIdentityNameStateProvider*>(
                internal::StateProviderGetter(
                    *state_manager_->GetActiveStateProvider())
                    .AsShowIdentity());
        show_identity_state->ForceDelayTimeoutForTesting();  // IN-TEST
      }
      break;
    case AvatarDelayType::kSigninPendingText:
      if (state_manager_->GetButtonActiveState() ==
          ButtonState::kSigninPending) {
        internal::SigninPendingStateProvider* signin_pending_state =
            const_cast<internal::SigninPendingStateProvider*>(
                internal::StateProviderGetter(
                    *state_manager_->GetActiveStateProvider())
                    .AsSigninPending());
        signin_pending_state->ForceTimerTimeoutForTesting();  // IN-TEST
      }
      break;
  }
}

// static
base::AutoReset<std::optional<base::TimeDelta>> AvatarToolbarButtonDelegate::
    CreateScopedZeroDelayOverrideSigninPendingTextForTesting() {
  return base::AutoReset<std::optional<base::TimeDelta>>(
      &g_show_signin_pending_text_delay_for_testing, base::Seconds(0));
}
