// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/avatar_toolbar_button_delegate.h"

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/scoped_observation.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/timer/timer.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
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
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/service/sync_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

static std::optional<base::TimeDelta> kTestingDuration;

constexpr base::TimeDelta kIdentityAnimationDuration = base::Seconds(3);

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
constexpr base::TimeDelta kEnterpriseTextTransientDuration = base::Seconds(30);
#endif

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

// Expected to be called when there is a sync error.
// Returning true for non sync paused error.
bool IsErrorSyncPaused(Profile* profile) {
  std::optional<AvatarSyncErrorType> error = ::GetAvatarSyncErrorType(profile);
  CHECK(error);
  return error == AvatarSyncErrorType::kSyncPaused &&
         AccountConsistencyModeManager::IsDiceEnabledForProfile(profile);
}

// Expected to be called when Management is set.
// Returns:
// - true for Work.
// - false for School.
bool IsManagementWork(Profile* profile) {
  CHECK(base::FeatureList::IsEnabled(features::kEnterpriseProfileBadging));
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  auto management_environment =
      chrome::enterprise_util::GetManagementEnvironment(
          profile, identity_manager->FindExtendedAccountInfoByAccountId(
                       identity_manager->GetPrimaryAccountId(
                           signin::ConsentLevel::kSignin)));
  CHECK_NE(management_environment,
           chrome::enterprise_util::ManagementEnvironment::kNone);
  return management_environment ==
         chrome::enterprise_util::ManagementEnvironment::kWork;
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
  // An error in sync-the-feature or sync-the-transport or SyncPaused (use
  // `IsErrorSyncPaused()` to differentiate).
  kSyncError,
  kSigninPaused,
  // Includes Work and School.
  kManagement,
  kNormal
};

namespace {

enum class ElementToUpdate {
  kText,
  kIcon,
  kAll,
};

class StateProvider;

class StateObserver {
 public:
  virtual void OnStateProviderUpdateRequest(
      StateProvider* state_provider,
      ElementToUpdate element_to_update) = 0;

  virtual ~StateObserver() = default;
};

// Each implementation of StateProvider should be able to manage itself with the
// appropriate initial values such as a profile and observe/listen to changes in
// order to affect their active status.
class StateProvider {
 public:
  explicit StateProvider(StateObserver& state_observer)
      : state_observer_(state_observer) {}

  // TODO(b/324018028): Consider changing `IsActive()` to be non-virtual and
  // return a member variable `is_active_` that can be controlled by the derived
  // classes that sets the active/inactive state when needed, also requesting
  // updates on state change. This way we would make sure not to miss updates
  // when a state activation changes.
  virtual bool IsActive() const = 0;

  void RequestUpdate(ElementToUpdate element_to_update) {
    state_observer_->OnStateProviderUpdateRequest(this, element_to_update);
  }

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
  void OnBrowserAdded(Browser* browser) override {
    RequestUpdate(ElementToUpdate::kAll);
  }
  void OnBrowserRemoved(Browser* browser) override {
    RequestUpdate(ElementToUpdate::kAll);
  }

 private:
  base::ScopedObservation<BrowserList, BrowserListObserver>
      scoped_browser_list_observation_{this};
};

class ExplicitStateProvider : public StateProvider {
 public:
  explicit ExplicitStateProvider(StateObserver& state_observer)
      : StateProvider(state_observer) {}
  ~ExplicitStateProvider() override = default;

  bool IsActive() const override { return active_; }

  // Used as the callback closure to the setter of the explicit state,
  // or when overriding the explicit state by another one.
  void Clear() {
    active_ = false;
    RequestUpdate(ElementToUpdate::kAll);
  }

  base::WeakPtr<ExplicitStateProvider> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  bool active_ = true;

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
    if (identity_manager->AreRefreshTokensLoaded()) {
      OnRefreshTokensLoaded();
    }

    avatar_button_observation_.Observe(&avatar_toolbar_button);
  }

  ~ShowIdentityNameStateProvider() override {
    avatar_button_observation_.Reset();
  }

  bool IsActive() const override { return show_identity_request_count_ > 0; }

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

    CoreAccountInfo account =
        IdentityManagerFactory::GetForProfile(&profile_.get())
            ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
    if (account.IsEmpty()) {
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

  void OnIconUpdated() override { MaybeShowIdentityName(); }

 private:
  void UpdateButtonIcon() {
    if (!avatar_toolbar_button_->GetWidget()) {
      return;
    }

    RequestUpdate(ElementToUpdate::kIcon);

    // Try to show the name if we were waiting for an image.
    MaybeShowIdentityName();
  }

  // Initiates showing the identity.
  void OnUserIdentityChanged() {
    signin_ui_util::RecordAnimatedIdentityTriggered(&profile_.get());
    // On any following icon update the name will be attempted to be shown when
    // the image is ready.
    waiting_for_image_ = true;
    UpdateButtonIcon();
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

    RequestUpdate(ElementToUpdate::kText);

    // Hide the pill after a while.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            &ShowIdentityNameStateProvider::OnIdentityAnimationTimeout,
            weak_ptr_factory_.GetWeakPtr()),
        kTestingDuration.value_or(kIdentityAnimationDuration));
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
    avatar_toolbar_button_->NotifyShowNameClearedForTesting();  // IN-TEST
  }

  // Clears the effects of the state being active.
  void Clear() {
    show_identity_request_count_ = 0;
    waiting_for_image_ = false;
    show_identity_request_count_ = false;
    has_in_product_help_promo_ = false;

    RequestUpdate(ElementToUpdate::kAll);
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

class SyncErrorStateProvider : public StateProvider,
                               public syncer::SyncServiceObserver {
 public:
  explicit SyncErrorStateProvider(StateObserver& state_observer,
                                  Profile& profile)
      : StateProvider(state_observer),
        profile_(profile),
        last_avatar_error_(::GetAvatarSyncErrorType(&profile)) {
    if (auto* sync_service = SyncServiceFactory::GetForProfile(&profile)) {
      sync_service_observation_.Observe(sync_service);
    }
  }

  bool IsActive() const override {
    return ::GetAvatarSyncErrorType(&profile_.get()).has_value();
  }

 private:
  void OnStateChanged(syncer::SyncService*) override {
    const std::optional<AvatarSyncErrorType> error =
        ::GetAvatarSyncErrorType(&profile_.get());
    if (last_avatar_error_ == error) {
      return;
    }

    last_avatar_error_ = error;
    RequestUpdate(ElementToUpdate::kAll);
  }

  void OnSyncShutdown(syncer::SyncService*) override {
    sync_service_observation_.Reset();
  }

  raw_ref<Profile> profile_;
  // Caches the value of the last error so the class can detect when it
  // changes and notify changes.
  std::optional<AvatarSyncErrorType> last_avatar_error_;

  base::ScopedObservation<syncer::SyncService, syncer::SyncServiceObserver>
      sync_service_observation_{this};
};

class SigninPausedStateProvider : public StateProvider,
                                  public signin::IdentityManager::Observer {
 public:
  explicit SigninPausedStateProvider(StateObserver& state_observer,
                                     Profile* profile)
      : StateProvider(state_observer),
        identity_manager_(*IdentityManagerFactory::GetForProfile(profile)) {
    identity_manager_observation_.Observe(&identity_manager_.get());
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

 private:
  // signin::IdentityManager::Observer:
  void OnErrorStateOfRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info,
      const GoogleServiceAuthError& error) override {
    if (account_info != identity_manager_->GetPrimaryAccountInfo(
                            signin::ConsentLevel::kSignin)) {
      return;
    }

    RequestUpdate(ElementToUpdate::kAll);
  }

  void OnIdentityManagerShutdown(signin::IdentityManager*) override {
    identity_manager_observation_.Reset();
  }

  raw_ref<signin::IdentityManager> identity_manager_;

  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};
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
        avatar_toolbar_button_(avatar_toolbar_button),
        user_accepted_account_management_(
            chrome::enterprise_util::UserAcceptedAccountManagement(
                &profile_.get())) {
    BrowserList::AddObserver(this);
    profile_observation_.Observe(&GetProfileAttributesStorage());

    pref_change_registrar_.Init(profile_->GetPrefs());
    pref_change_registrar_.Add(
        prefs::kCustomProfileLabel,
        base::BindRepeating(&ManagementStateProvider::RequestUpdate,
                            weak_ptr_factory_.GetWeakPtr(),
                            ElementToUpdate::kText));
    pref_change_registrar_.Add(
        prefs::kProfileLabelPreset,
        base::BindRepeating(&ManagementStateProvider::RequestUpdate,
                            weak_ptr_factory_.GetWeakPtr(),
                            ElementToUpdate::kText));
  }

  ~ManagementStateProvider() override { BrowserList::RemoveObserver(this); }

  // StateProvider:
  bool IsActive() const override {
    if (policy::ManagementServiceFactory::GetForPlatform()->IsManaged() &&
        !IsEnterpriseToolbarLabelVisibilityManaged()) {
      return false;
    }
    return user_accepted_account_management_ &&
           (!IsTransient() || temporarily_showing_);
  }

 private:
  void OnBrowserAdded(Browser*) override {
    // This is required so that the enterprise text is shown when a profile is
    // opened.
    TryShowManagementText();
  }

  // ProfileAttributesStorage::Observer:
  void OnProfileUserManagementAcceptanceChanged(
      const base::FilePath& profile_path) override {
    user_accepted_account_management_ =
        chrome::enterprise_util::UserAcceptedAccountManagement(&profile_.get());
    if (!user_accepted_account_management_) {
      RequestUpdate(ElementToUpdate::kAll);
      return;
    }

    TryShowManagementText();
  }

  void TryShowManagementText() {
    if (IsTransient() && !enterprise_text_hide_scheduled_) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&ManagementStateProvider::ClearTransientText,
                         weak_ptr_factory_.GetWeakPtr()),
          kTestingDuration.value_or(kEnterpriseTextTransientDuration));
      enterprise_text_hide_scheduled_ = true;
      temporarily_showing_ = true;
    }
    RequestUpdate(ElementToUpdate::kText);
  }

  void ClearTransientText() {
    CHECK(IsTransient());

    temporarily_showing_ = false;
    RequestUpdate(ElementToUpdate::kAll);
    avatar_toolbar_button_
        ->NotifyManagementTransientTextClearedForTesting();  // IN-TEST
  }

  // Used to determine if the text should be shown permanently or not.
  bool IsTransient() const {
    return g_browser_process->local_state()->GetInteger(
               prefs::kToolbarAvatarLabelSettings) == 1;
  }

  bool IsEnterpriseToolbarLabelVisibilityManaged() const {
    return g_browser_process->local_state()
        ->FindPreference(prefs::kToolbarAvatarLabelSettings)
        ->IsManaged();
  }

  raw_ref<Profile> profile_;
  const raw_ref<const AvatarToolbarButton> avatar_toolbar_button_;

  bool user_accepted_account_management_ = false;
  bool enterprise_text_hide_scheduled_ = false;
  bool temporarily_showing_ = false;
  PrefChangeRegistrar pref_change_registrar_;

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
              /*state_observer=*/*this, *profile, avatar_toolbar_button);

      // Will also be active for SyncPaused state.
      states_[ButtonState::kSyncError] =
          std::make_unique<SyncErrorStateProvider>(
              /*state_observer=*/*this, *profile);

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
      if (base::FeatureList::IsEnabled(features::kEnterpriseProfileBadging)) {
        // Contains both Work and School.
        states_[ButtonState::kManagement] =
            std::make_unique<ManagementStateProvider>(
                /*state_observer=*/*this, *profile, avatar_toolbar_button);
      }
#endif

      if (switches::IsExplicitBrowserSigninUIOnDesktopEnabled(
              switches::ExplicitBrowserSigninPhase::kFull)) {
        states_[ButtonState::kSigninPaused] =
            std::make_unique<SigninPausedStateProvider>(
                /*state_observer=*/*this, profile);
      }

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
  ~StateManager() override = default;

  // Computes and returns the current active state with the highest priority.
  // Multiple states could be active at the same time.
  ButtonState ComputeButtonActiveState() {
    // Traverse the map of states sorted by their priority set in `ButtonState`.
    for (auto& state_pair : states_) {
      // Return the first state that is active.
      if (state_pair.second->IsActive()) {
        current_active_state_ = state_pair.second.get();
        // TODO(b/324018028): this could return the state provider itself, if
        // the information can be get from it later.
        return state_pair.first;
      }
    }

    NOTREACHED_NORETURN()
        << "There should at least be one active state in the map.";
  }

  // Special setter for the explicit state as it is controlled externally.
  void SetExplicitStateProvider(
      std::unique_ptr<ExplicitStateProvider> explicit_state_provider) {
    if (auto it = states_.find(ButtonState::kExplicitTextShowing);
        it != states_.end()) {
      // Attempt to clear existing states if not already done.
      static_cast<ExplicitStateProvider*>(it->second.get())->Clear();
    }

    states_[ButtonState::kExplicitTextShowing] =
        std::move(explicit_state_provider);
  }

  // StateObserver:
  void OnStateProviderUpdateRequest(
      StateProvider* requesting_state,
      ElementToUpdate element_to_update) override {
    if (!requesting_state->IsActive()) {
      // Updates everything if the requesting state was the current button
      // active state, clearing it, otherwise we just ignore the request.
      if (current_active_state_ == requesting_state) {
        // Will recompute the new button active state as we are clearing the
        // requesting state effects.
        Update(ElementToUpdate::kAll);
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
    if (current_active_state_ != requesting_state) {
      return;
    }

    Update(element_to_update);
  }

 private:
  // This method will compute the button active state again with
  // `ComputeButtonActiveState()` through the delegate.
  void Update(ElementToUpdate element_to_update) {
    if (element_to_update == ElementToUpdate::kAll ||
        element_to_update == ElementToUpdate::kText) {
      avatar_toolbar_button_->UpdateText();
    }
    if (element_to_update == ElementToUpdate::kAll ||
        element_to_update == ElementToUpdate::kIcon) {
      avatar_toolbar_button_->UpdateIconWithoutObservers();
    }
  }

  // Make sure to notify obsers, the `ShowIdentityNameStateProvider` being one
  // of the observers.
  void UpdateIconWithObservers() { avatar_toolbar_button_->UpdateIcon(); }

  // signin::IdentityManager::Observer:
  void OnIdentityManagerShutdown(signin::IdentityManager*) override {
    scoped_identity_manager_observation_.Reset();
  }

  void OnRefreshTokensLoaded() override { UpdateIconWithObservers(); }

  void OnAccountsInCookieUpdated(const signin::AccountsInCookieJarInfo&,
                                 const GoogleServiceAuthError&) override {
    UpdateIconWithObservers();
  }

  void OnExtendedAccountInfoUpdated(const AccountInfo&) override {
    UpdateIconWithObservers();
  }

  void OnExtendedAccountInfoRemoved(const AccountInfo&) override {
    UpdateIconWithObservers();
  }

  //  ProfileAttributesStorage::Observer:
  void OnProfileAvatarChanged(const base::FilePath&) override {
    UpdateIconWithObservers();
  }

  void OnProfileHighResAvatarLoaded(const base::FilePath&) override {
    UpdateIconWithObservers();
  }

  void OnProfileNameChanged(const base::FilePath&,
                            const std::u16string&) override {
    Update(ElementToUpdate::kText);
  }

  base::flat_map<ButtonState, std::unique_ptr<StateProvider>> states_;
  raw_ref<AvatarToolbarButton> avatar_toolbar_button_;

  // Active state per the last request to `ComputeButtonActiveState()`.
  raw_ptr<StateProvider> current_active_state_ = nullptr;

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
      state_manager_(
          std::make_unique<internal::StateManager>(*avatar_toolbar_button_,
                                                   browser)) {
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

std::u16string AvatarToolbarButtonDelegate::GetProfileName() const {
  DCHECK_NE(ComputeState(), ButtonState::kIncognitoProfile);
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

  // TODO(crbug.com/1012179): it should suffice to call entry->GetAvatarIcon().
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

  return entry->GetAvatarIcon(preferred_size);
}

int AvatarToolbarButtonDelegate::GetWindowCount() const {
  if (profile_->IsGuestSession()) {
    return BrowserList::GetGuestBrowserCount();
  }
  DCHECK(profile_->IsOffTheRecord());
  return BrowserList::GetOffTheRecordBrowsersActiveForProfile(profile_);
}

ButtonState AvatarToolbarButtonDelegate::ComputeState() const {
  return state_manager_->ComputeButtonActiveState();
}

void AvatarToolbarButtonDelegate::OnThemeChanged(
    const ui::ColorProvider* color_provider) {
  // Update avatar color information in profile attributes.
  if (profile_->IsOffTheRecord() || profile_->IsGuestSession()) {
    return;
  }

  ProfileAttributesEntry* entry = GetProfileAttributesEntry(profile_);
  if (!entry) {
    return;
  }

  ThemeService* service = ThemeServiceFactory::GetForProfile(profile_);
  if (!service) {
    return;
  }

  // Only save colors for autogenerated themes.
  if (service->UsingAutogeneratedTheme() ||
      service->GetUserColor().has_value()) {
    if (!color_provider) {
      return;
    }
    entry->SetProfileThemeColors(GetCurrentProfileThemeColors(*color_provider));
  } else {
    entry->SetProfileThemeColors(std::nullopt);
  }
}

base::ScopedClosureRunner AvatarToolbarButtonDelegate::ShowExplicitText(
    const std::u16string& new_text) {
  CHECK(!new_text.empty());

  // Create the new explicit state with the clear text callback.
  std::unique_ptr<ExplicitStateProvider> explicit_state_provider =
      std::make_unique<ExplicitStateProvider>(
          /*state_observer=*/*state_manager_);

  ExplicitStateProvider* explicit_state_provider_ptr =
      explicit_state_provider.get();
  // Activate the state.
  state_manager_->SetExplicitStateProvider(std::move(explicit_state_provider));

  // Prepare and update the button text.
  explicit_text_ = new_text;
  avatar_toolbar_button_->UpdateText();

  return base::ScopedClosureRunner(
      base::BindOnce(&ExplicitStateProvider::Clear,
                     // WeakPtr is needed here since this state could be
                     // replaced before the call to the closure.
                     explicit_state_provider_ptr->GetWeakPtr()));
}

std::pair<std::u16string, std::optional<SkColor>>
AvatarToolbarButtonDelegate::GetTextAndColor(
    const ui::ColorProvider* const color_provider) const {
  std::optional<SkColor> color;
  std::u16string text;

  if (features::IsChromeRefresh2023()) {
    color = color_provider->GetColor(kColorAvatarButtonHighlightDefault);
  }
  switch (ComputeState()) {
    case ButtonState::kIncognitoProfile: {
      const int incognito_window_count = GetWindowCount();
      avatar_toolbar_button_->SetAccessibleName(
          l10n_util::GetPluralStringFUTF16(
              IDS_INCOGNITO_BUBBLE_ACCESSIBLE_TITLE, incognito_window_count));
      text = l10n_util::GetPluralStringFUTF16(IDS_AVATAR_BUTTON_INCOGNITO,
                                              incognito_window_count);
      // TODO(shibalik): Remove this condition to make it generic by refactoring
      // `ToolbarButton::HighlightColorAnimation`.
      if (features::IsChromeRefresh2023()) {
        color = color_provider->GetColor(kColorAvatarButtonHighlightIncognito);
      }
      break;
    }
    case ButtonState::kShowIdentityName:
      text = GetShortProfileName();
      break;
    case ButtonState::kExplicitTextShowing: {
      CHECK(!explicit_text_.empty());
      text = explicit_text_;
      break;
    }
    case ButtonState::kSyncError:
      if (IsErrorSyncPaused(profile_)) {
        color = color_provider->GetColor(kColorAvatarButtonHighlightSyncPaused);
        text = l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SYNC_PAUSED);
      } else {
        color = color_provider->GetColor(kColorAvatarButtonHighlightSyncError);
        text = l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SYNC_ERROR);
      }
      break;
    case ButtonState::kSigninPaused:
      color = color_provider->GetColor(kColorAvatarButtonHighlightSigninPaused);
      text = l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SIGNIN_PAUSED);
      break;
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
      avatar_toolbar_button_->SetAccessibleName(
          l10n_util::GetPluralStringFUTF16(IDS_GUEST_BUBBLE_ACCESSIBLE_TITLE,
                                           guest_window_count));
      text = l10n_util::GetPluralStringFUTF16(IDS_AVATAR_BUTTON_GUEST,
                                              guest_window_count);
      break;
    }
    case ButtonState::kManagement: {
      const std::string custom_managed_label =
          profile_->GetPrefs()->GetString(prefs::kCustomProfileLabel);
      if (!custom_managed_label.empty()) {
        text = base::UTF8ToUTF16(custom_managed_label);
      } else if (profile_->GetPrefs()
                     ->FindPreference(prefs::kProfileLabelPreset)
                     ->IsManaged()) {
        const int profile_label_preset =
            profile_->GetPrefs()->GetInteger(prefs::kProfileLabelPreset);
        if (profile_label_preset ==
            AvatarToolbarButton::ProfileLabelType::kWork) {
          text = l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_WORK);
        } else if (profile_label_preset ==
                   AvatarToolbarButton::ProfileLabelType::kSchool) {
          text = l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SCHOOL);
        }
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

SkColor AvatarToolbarButtonDelegate::GetHighlightTextColor(
    const ui::ColorProvider* const color_provider) const {
  switch (ComputeState()) {
    case ButtonState::kIncognitoProfile:
      return color_provider->GetColor(
          kColorAvatarButtonHighlightIncognitoForeground);
    case ButtonState::kSyncError:
      if (IsErrorSyncPaused(profile_)) {
        return color_provider->GetColor(
            kColorAvatarButtonHighlightNormalForeground);
      } else {
        return color_provider->GetColor(
            kColorAvatarButtonHighlightSyncErrorForeground);
      }
    case ButtonState::kGuestSession:
    case ButtonState::kExplicitTextShowing:
    case ButtonState::kShowIdentityName:
      return color_provider->GetColor(
          kColorAvatarButtonHighlightDefaultForeground);
    case ButtonState::kManagement:
    case ButtonState::kSigninPaused:
      return color_provider->GetColor(
          kColorAvatarButtonHighlightNormalForeground);
    case ButtonState::kNormal:
      return color_provider->GetColor(
          kColorAvatarButtonHighlightDefaultForeground);
  }
}

std::u16string AvatarToolbarButtonDelegate::GetAvatarTooltipText() const {
  switch (ComputeState()) {
    case ButtonState::kIncognitoProfile:
      return l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_INCOGNITO_TOOLTIP);
    case ButtonState::kGuestSession:
      return l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_GUEST_TOOLTIP);
    case ButtonState::kShowIdentityName:
      return GetShortProfileName();
    case ButtonState::kSyncError: {
      std::optional<AvatarSyncErrorType> sync_error =
          ::GetAvatarSyncErrorType(profile_);
      DCHECK(sync_error);
      return l10n_util::GetStringFUTF16(
          IDS_AVATAR_BUTTON_SYNC_ERROR_TOOLTIP, GetShortProfileName(),
          GetAvatarSyncErrorDescription(
              *sync_error,
              IdentityManagerFactory::GetForProfile(profile_)
                  ->HasPrimaryAccount(signin::ConsentLevel::kSync)));
    }
    case ButtonState::kSigninPaused:
    case ButtonState::kExplicitTextShowing:
    case ButtonState::kManagement:
    case ButtonState::kNormal:
      return GetProfileName();
  }
}

std::pair<ChromeColorIds, ChromeColorIds>
AvatarToolbarButtonDelegate::GetInkdropColors() const {
  CHECK(features::IsChromeRefresh2023());

  ChromeColorIds hover_color_id = kColorToolbarInkDropHover;
  ChromeColorIds ripple_color_id = kColorToolbarInkDropRipple;

  if (avatar_toolbar_button_->IsLabelPresentAndVisible()) {
    switch (ComputeState()) {
      case ButtonState::kIncognitoProfile:
        hover_color_id = kColorAvatarButtonIncognitoHover;
        break;
      case ButtonState::kSyncError:
        if (IsErrorSyncPaused(profile_)) {
          ripple_color_id = kColorAvatarButtonNormalRipple;
        }
        break;
      case ButtonState::kGuestSession:
      case ButtonState::kExplicitTextShowing:
      case ButtonState::kShowIdentityName:
        break;
      case ButtonState::kManagement:
      case ButtonState::kSigninPaused:
        ripple_color_id = kColorAvatarButtonNormalRipple;
        break;
      case ButtonState::kNormal:
        ripple_color_id = kColorToolbarInkDropRipple;
        break;
    }
  }

  return {hover_color_id, ripple_color_id};
}

ui::ImageModel AvatarToolbarButtonDelegate::GetAvatarIcon(
    int icon_size,
    SkColor icon_color) const {
  switch (ComputeState()) {
    case ButtonState::kIncognitoProfile:
      return ui::ImageModel::FromVectorIcon(features::IsChromeRefresh2023()
                                                ? kIncognitoRefreshMenuIcon
                                                : kIncognitoIcon,
                                            icon_color, icon_size);
    case ButtonState::kGuestSession:
      return profiles::GetGuestAvatar(icon_size);
    case ButtonState::kExplicitTextShowing:
    case ButtonState::kShowIdentityName:
    // TODO(crbug.com/1191411): If sync-the-feature is disabled, the icon
    // should be different.
    case ButtonState::kSyncError:
    case ButtonState::kManagement:
    case ButtonState::kSigninPaused:
    case ButtonState::kNormal:
      return ui::ImageModel::FromImage(profiles::GetSizedAvatarIcon(
          GetProfileAvatarImage(icon_size), icon_size, icon_size,
          profiles::SHAPE_CIRCLE));
  }
}

bool AvatarToolbarButtonDelegate::ShouldPaintBorder() const {
  switch (ComputeState()) {
    case ButtonState::kGuestSession:
    case ButtonState::kShowIdentityName:
    case ButtonState::kNormal:
      return true;
    case ButtonState::kIncognitoProfile:
    case ButtonState::kExplicitTextShowing:
    case ButtonState::kManagement:
    case ButtonState::kSigninPaused:
    case ButtonState::kSyncError:
      return false;
  }
}

// static
void AvatarToolbarButtonDelegate::SetTextDurationForTesting(
    base::TimeDelta duration) {
  kTestingDuration = duration;
}
