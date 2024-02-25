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
#include "base/task/single_thread_task_runner.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
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
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/sync/service/sync_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace internal {

// States of the button ordered in priority of getting displayed.
// The order of those values is used with the `StateManager` to make sure the
// active state with the highest priority is shown.
// The lower the value of the enum, the higher the priority.
enum class ButtonState {
  kGuestSession,
  kIncognitoProfile,
  kExplicitTextShowing,
  kAnimatedUserIdentity,
  kSyncPaused,
  // An error in sync-the-feature or sync-the-transport.
  kSyncError,
  kWork,
  kSchool,
  kNormal
};

namespace {

enum class ElementToUpdate {
  kText,
  kIcon,
  kAll,
};

class StateObserver {
 public:
  virtual void Update(ElementToUpdate element_to_update) = 0;

  virtual ~StateObserver() = default;
};

// Each implementation of StateProvider should be able to manage itself with the
// appropriate initial values such as a profile and observe/listen to changes in
// order to affect their active status.
class StateProvider {
 public:
  explicit StateProvider(StateObserver& state_observer)
      : state_observer_(state_observer) {}

  virtual bool IsActive() const = 0;

  void NotifyUpdate(ElementToUpdate element_to_update) {
    if (IsActive()) {
      state_observer_->Update(element_to_update);
    }
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
    NotifyUpdate(ElementToUpdate::kAll);
  }
  void OnBrowserRemoved(Browser* browser) override {
    NotifyUpdate(ElementToUpdate::kAll);
  }

 private:
  base::ScopedObservation<BrowserList, BrowserListObserver>
      scoped_browser_list_observation_{this};
};

class ExplicitStateProvider : public StateProvider {
 public:
  explicit ExplicitStateProvider(StateObserver& state_observer,
                                 base::OnceClosure clear_avatar_text_closure)
      : StateProvider(state_observer),
        clear_avatar_text_closure_(std::move(clear_avatar_text_closure)) {}
  ~ExplicitStateProvider() override = default;

  bool IsActive() const override { return active_; }

  // Used as the callback closure to the setter of the explicit state,
  // or when overriding the explicit state by another one.
  void Clear() {
    if (!active_) {
      return;
    }

    active_ = false;
    // TODO(b/324018028): Once the default states are implemented through the
    // state manager, remove this call back and replace it with a call to
    // `NotifyUpdate(ElementToUpdate::kText)`. The concept of default state
    // would not exist anymore, which is the main difference with the current
    // call.
    std::move(clear_avatar_text_closure_).Run();
  }

  base::WeakPtr<ExplicitStateProvider> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  bool active_ = true;

  base::OnceClosure clear_avatar_text_closure_;

  base::WeakPtrFactory<ExplicitStateProvider> weak_ptr_factory_{this};
};

// TO BE USED at the end of the imlpementation.
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
// `kExplicitTextShowing` with `ExplicitStateProvider` is the only state that
// can change dynamically.
class StateManager : public StateObserver {
 public:
  explicit StateManager(AvatarToolbarButton& avatar_toolbar_button,
                        Profile* profile)
      : avatar_toolbar_button_(avatar_toolbar_button) {
    // Add each possible state for each Profile type, since this structure is
    // tied to Browser, in which a Profile cannot change, it is correct to
    // compute the Profile type once.
    if (profile->IsGuestSession()) {
      states_[ButtonState::kGuestSession] =
          std::make_unique<PrivateStateProvider>(
              /*state_observer=*/*this);
    } else if (profile->IsIncognitoProfile()) {
      states_[ButtonState::kIncognitoProfile] =
          std::make_unique<PrivateStateProvider>(
              /*state_observer=*/*this);
    }

    // TODO(b/324018028): The normal state should be added in the end since it
    // is always active. While transitioning, since we use nullptr state as not
    // implemented state yet we cannot activate this one yet.
    // states_[ButtonState::kNormal] =
    // std::make_unique<NormalStateProvider>(/*state_observer=*/*this);
  }
  ~StateManager() override = default;

  // Returns the current active state with the highest priority.
  // Multiple states could be active at the same time.
  std::optional<ButtonState> GetActiveState() {
    // Traverse the map of states sorted by their priority set in `ButtonState`.
    for (auto& state_pair : states_) {
      // Return the first state that is active.
      if (state_pair.second->IsActive()) {
        // TODO(b/324018028): this could return the state provider itself, if
        // the information can be get from it later.
        return state_pair.first;
      }
    }

    // TODO(b/324018028): At the end of the implementation this should not be
    // expected anymore and be replaced by a `NOTREACHED_NORETURN()`.
    return std::nullopt;
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
  void Update(ElementToUpdate element_to_update) override {
    if (element_to_update == ElementToUpdate::kAll ||
        element_to_update == ElementToUpdate::kText) {
      avatar_toolbar_button_->UpdateText();
    }
    if (element_to_update == ElementToUpdate::kAll ||
        element_to_update == ElementToUpdate::kIcon) {
      avatar_toolbar_button_->UpdateIcon();
    }
  }

 private:
  base::flat_map<ButtonState, std::unique_ptr<StateProvider>> states_;
  raw_ref<AvatarToolbarButton> avatar_toolbar_button_;
};

}  // namespace internal

using ButtonState = internal::ButtonState;
using ExplicitStateProvider = internal::ExplicitStateProvider;

namespace {
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

}  // namespace

AvatarToolbarButtonDelegate::AvatarToolbarButtonDelegate(
    AvatarToolbarButton* button,
    Browser* browser)
    : avatar_toolbar_button_(button),
      browser_(browser),
      profile_(browser->profile()),
      last_avatar_error_(::GetAvatarSyncErrorType(profile_)),
      state_manager_(
          std::make_unique<internal::StateManager>(*avatar_toolbar_button_,
                                                   profile_)) {
  profile_observation_.Observe(&GetProfileAttributesStorage());

  if (auto* sync_service = SyncServiceFactory::GetForProfile(profile_)) {
    sync_service_observation_.Observe(sync_service);
  }

  bool is_incognito = profile_->IsOffTheRecord();
  if (!is_incognito && !profile_->IsGuestSession()) {
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(profile_);
    identity_manager_observation_.Observe(identity_manager);
    if (identity_manager->AreRefreshTokensLoaded()) {
      OnRefreshTokensLoaded();
    }
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // On CrOS this button should only show as badging for Incognito, Guest and
  // captivie portal signin. It's only enabled for non captive portal Incognito
  // where a menu is available for closing all Incognito windows.
  avatar_toolbar_button_->SetEnabled(
      is_incognito && !profile_->GetOTRProfileID().IsCaptivePortal());
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  // On Lacros we need to disable the button for captivie portal signin.
  avatar_toolbar_button_->SetEnabled(
      !is_incognito || !profile_->GetOTRProfileID().IsCaptivePortal());
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
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);
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
  // TODO(b/324018028): adapt each state to be part of a `StateProvider`. When
  // all states are migrated, remove the optional part of
  // `StateManager::GetActiveState()` as we should always have at least one
  // active state at all time.
  std::optional<ButtonState> active_state = state_manager_->GetActiveState();
  if (active_state.has_value()) {
    return active_state.value();
  }

  if (button_text_state_ == TextState::kShowingName) {
    return ButtonState::kAnimatedUserIdentity;
  }

  if (!last_avatar_error_ &&
      button_text_state_ == TextState::kShowingEnterpriseText) {
    CHECK(base::FeatureList::IsEnabled(features::kEnterpriseProfileBadging));
    auto* identity_manager = IdentityManagerFactory::GetForProfile(profile_);
    auto management_environment =
        chrome::enterprise_util::GetManagementEnvironment(
            profile_, identity_manager->FindExtendedAccountInfoByAccountId(
                          identity_manager->GetPrimaryAccountId(
                              signin::ConsentLevel::kSignin)));
    CHECK_NE(management_environment,
             chrome::enterprise_util::ManagementEnvironment::kNone);
    if (management_environment ==
        chrome::enterprise_util::ManagementEnvironment::kWork) {
      return ButtonState::kWork;
    }
    if (management_environment ==
        chrome::enterprise_util::ManagementEnvironment::kSchool) {
      return ButtonState::kSchool;
    }
  }

  // Web app has limited toolbar space, thus always show kNormal state.
  if (web_app::AppBrowserController::IsWebApp(browser_) ||
      !SyncServiceFactory::IsSyncAllowed(profile_)) {
    return ButtonState::kNormal;
  }

  // Show any existing sync errors (sync-the-feature or sync-the-transport).
  // |last_avatar_error_| should be checked here rather than
  // ::GetAvatarSyncErrorType(), so the result agrees with
  // AvatarToolbarButtonDelegate::GetAvatarSyncErrorType().
  if (!last_avatar_error_) {
    return ButtonState::kNormal;
  }

  if (last_avatar_error_ == AvatarSyncErrorType::kSyncPaused &&
      AccountConsistencyModeManager::IsDiceEnabledForProfile(profile_)) {
    return ButtonState::kSyncPaused;
  }

  return ButtonState::kSyncError;
}

std::optional<AvatarSyncErrorType>
AvatarToolbarButtonDelegate::GetAvatarSyncErrorType() const {
  return last_avatar_error_;
}

void AvatarToolbarButtonDelegate::MaybeShowIdentityAnimation() {
  const gfx::Image gaia_account_image = GetGaiaAccountImage();
  if (button_text_state_ != TextState::kWaitingForImage ||
      gaia_account_image.IsEmpty()) {
    return;
  }

  // Check that the user is still signed in. See https://crbug.com/1025674
  if (!IdentityManagerFactory::GetForProfile(profile_)->HasPrimaryAccount(
          signin::ConsentLevel::kSignin)) {
    ShowDefaultText();
    return;
  }

  ShowIdentityAnimation();
}

void AvatarToolbarButtonDelegate::SetHasInProductHelpPromo(bool has_promo) {
  if (has_in_product_help_promo_ == has_promo) {
    return;
  }

  has_in_product_help_promo_ = has_promo;
  // Trigger a new animation, even if the IPH is being removed. This keeps the
  // pill open a little more and avoids jankiness caused by the two animations
  // (IPH and identity pill) happening concurrently.
  // See https://crbug.com/1198907
  ShowIdentityAnimation();
}

void AvatarToolbarButtonDelegate::OnMouseExited() {
  MaybeHideIdentityAnimation();
}

void AvatarToolbarButtonDelegate::OnBlur() {
  MaybeHideIdentityAnimation();
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
  // This is required so that the enterprise text is shown when a profile is
  // opened.
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  MaybeShowEnterpriseText();
#endif
}

void AvatarToolbarButtonDelegate::OnProfileAvatarChanged(
    const base::FilePath& profile_path) {
  avatar_toolbar_button_->UpdateIcon();
}

void AvatarToolbarButtonDelegate::OnProfileHighResAvatarLoaded(
    const base::FilePath& profile_path) {
  avatar_toolbar_button_->UpdateIcon();
}

void AvatarToolbarButtonDelegate::OnProfileNameChanged(
    const base::FilePath& profile_path,
    const std::u16string& old_profile_name) {
  avatar_toolbar_button_->UpdateText();
}

void AvatarToolbarButtonDelegate::OnProfileUserManagementAcceptanceChanged(
    const base::FilePath& profile_path) {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  MaybeShowEnterpriseText();
#endif
}

void AvatarToolbarButtonDelegate::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event) {
  if (event.GetEventTypeFor(signin::ConsentLevel::kSignin) !=
      signin::PrimaryAccountChangeEvent::Type::kSet) {
    return;
  }
  OnUserIdentityChanged();
}

void AvatarToolbarButtonDelegate::OnRefreshTokensLoaded() {
  if (refresh_tokens_loaded_) {
    // This is possible, if |AvatarToolbarButtonDelegate::Init| is called within
    // the loop in |IdentityManager::OnRefreshTokensLoaded()| to notify
    // observers. In that case, |OnRefreshTokensLoaded| will be called twice,
    // once from |AvatarToolbarButtonDelegate::Init| and another time from the
    // |IdentityManager|. This happens for new signed in profiles.
    // See https://crbug.com/1035480
    return;
  }

  refresh_tokens_loaded_ = true;
  if (!signin_ui_util::ShouldShowAnimatedIdentityOnOpeningWindow(
          GetProfileAttributesStorage(), profile_)) {
    return;
  }
  CoreAccountInfo account =
      IdentityManagerFactory::GetForProfile(profile_)->GetPrimaryAccountInfo(
          signin::ConsentLevel::kSignin);
  if (account.IsEmpty()) {
    return;
  }
  OnUserIdentityChanged();
}

void AvatarToolbarButtonDelegate::OnAccountsInCookieUpdated(
    const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
    const GoogleServiceAuthError& error) {
  avatar_toolbar_button_->UpdateIcon();
}

void AvatarToolbarButtonDelegate::OnExtendedAccountInfoUpdated(
    const AccountInfo& info) {
  avatar_toolbar_button_->UpdateIcon();
}

void AvatarToolbarButtonDelegate::OnExtendedAccountInfoRemoved(
    const AccountInfo& info) {
  avatar_toolbar_button_->UpdateIcon();
}

void AvatarToolbarButtonDelegate::OnIdentityManagerShutdown(
    signin::IdentityManager*) {
  identity_manager_observation_.Reset();
}

void AvatarToolbarButtonDelegate::OnStateChanged(syncer::SyncService*) {
  const std::optional<AvatarSyncErrorType> error =
      ::GetAvatarSyncErrorType(profile_);
  if (last_avatar_error_ == error) {
    return;
  }

  last_avatar_error_ = error;
  avatar_toolbar_button_->UpdateIcon();
  avatar_toolbar_button_->UpdateText();
}

void AvatarToolbarButtonDelegate::OnSyncShutdown(syncer::SyncService*) {
  sync_service_observation_.Reset();
}

void AvatarToolbarButtonDelegate::OnUserIdentityChanged() {
  signin_ui_util::RecordAnimatedIdentityTriggered(profile_);
  button_text_state_ = TextState::kWaitingForImage;
  // If we already have a gaia image, the pill will be immediately displayed by
  // `UpdateIcon()`. If not, it can still be displayed later, since the button
  // text state is now set to `TextState::kWaitingForImage`. This state
  // will trigger the animation in `MaybeShowIdentityAnimation(...)`.
  avatar_toolbar_button_->UpdateIcon();
}

void AvatarToolbarButtonDelegate::OnIdentityAnimationTimeout() {
  --identity_animation_timeout_count_;
  // If the count is > 0, there's at least one more pending
  // OnIdentityAnimationTimeout() that will hide it after the proper delay.
  // Also return if the button is showing the signin text rather than the name.
  if (identity_animation_timeout_count_ > 0 ||
      button_text_state_ == TextState::kShowingExplicitText ||
      button_text_state_ == TextState::kShowingEnterpriseText) {
    return;
  }

  DCHECK_EQ(button_text_state_, TextState::kShowingName);
  MaybeHideIdentityAnimation();
}

void AvatarToolbarButtonDelegate::MaybeHideIdentityAnimation() {
  // No-op if not showing or if the timeout hasn't passed, yet.
  if (button_text_state_ != TextState::kShowingName ||
      identity_animation_timeout_count_ > 0) {
    return;
  }

  // Keep identity visible if this button is in use (hovered or has focus) or
  // has an associated In-Product-Help promo. We should not move things around
  // when the user wants to click on |this| or another button in the parent.
  if (avatar_toolbar_button_->IsMouseHovered() ||
      avatar_toolbar_button_->HasFocus() || has_in_product_help_promo_) {
    return;
  }

  // Update the text to the pre-shown state. This also makes sure that we now
  // reflect changes that happened while the identity pill was shown.
  ShowDefaultText();
}

void AvatarToolbarButtonDelegate::ShowIdentityAnimation() {
  button_text_state_ = TextState::kShowingName;
  avatar_toolbar_button_->UpdateText();

  // Hide the pill after a while.
  ++identity_animation_timeout_count_;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&AvatarToolbarButtonDelegate::OnIdentityAnimationTimeout,
                     weak_ptr_factory_.GetWeakPtr()),
      kIdentityAnimationDuration);
}

base::ScopedClosureRunner AvatarToolbarButtonDelegate::ShowExplicitText(
    const std::u16string& new_text) {
  CHECK(!new_text.empty());

  // Create the new explicit state with the clear text callback.
  std::unique_ptr<ExplicitStateProvider> explicit_state_provider =
      std::make_unique<ExplicitStateProvider>(
          /*state_observer=*/*state_manager_,
          /*clear_avatar_text_closure=*/base::BindOnce(
              &AvatarToolbarButtonDelegate::ClearExplicitText,
              // This state will exist in the `StateManager` which is part of
              // the button delegate, so `base::Unretained()` is fine.
              base::Unretained(this)));

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

void AvatarToolbarButtonDelegate::ClearExplicitText() {
  explicit_text_.clear();
  ShowDefaultText();
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
void AvatarToolbarButtonDelegate::MaybeShowEnterpriseText() {
  if (!base::FeatureList::IsEnabled(features::kEnterpriseProfileBadging) ||
      !chrome::enterprise_util::UserAcceptedAccountManagement(profile_)) {
    return;
  }
  bool transient = g_browser_process->local_state()->GetInteger(
                       prefs::kToolbarAvatarLabelSettings) == 1;
  button_text_state_ = TextState::kShowingEnterpriseText;
  avatar_toolbar_button_->UpdateText();
  if (transient && !enterprise_text_hide_scheduled_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&AvatarToolbarButtonDelegate::ShowDefaultText,
                       weak_ptr_factory_.GetWeakPtr()),
        kEnterpriseTextTransientDuration);
    enterprise_text_hide_scheduled_ = true;
  }
}
#endif

void AvatarToolbarButtonDelegate::ShowDefaultText() {
  button_text_state_ = GetDefaultTextState();
  avatar_toolbar_button_->UpdateText();
}

AvatarToolbarButtonDelegate::TextState
AvatarToolbarButtonDelegate::GetDefaultTextState() const {
  bool transient_enterprise_text = g_browser_process->local_state()->GetInteger(
                                       prefs::kToolbarAvatarLabelSettings) == 1;
  if (base::FeatureList::IsEnabled(features::kEnterpriseProfileBadging) &&
      chrome::enterprise_util::UserAcceptedAccountManagement(profile_) &&
      !transient_enterprise_text) {
    return TextState::kShowingEnterpriseText;
  }

  return TextState::kNotShowing;
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
    case ButtonState::kAnimatedUserIdentity:
      text = GetShortProfileName();
      break;
    case ButtonState::kExplicitTextShowing: {
      CHECK(!explicit_text_.empty());
      text = explicit_text_;
      break;
    }
    case ButtonState::kSyncError:
      color = color_provider->GetColor(kColorAvatarButtonHighlightSyncError);
      text = l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SYNC_ERROR);
      break;
    case ButtonState::kSyncPaused:
      color = color_provider->GetColor(kColorAvatarButtonHighlightSyncPaused);
      text = l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SYNC_PAUSED);
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
    case ButtonState::kWork: {
      text = l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_WORK);
      color = color_provider->GetColor(kColorAvatarButtonHighlightNormal);
      break;
    }
    case ButtonState::kSchool: {
      text = l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SCHOOL);
      color = color_provider->GetColor(kColorAvatarButtonHighlightNormal);
      break;
    }
    case ButtonState::kNormal:
      break;
  }

  return {text, color};
}

std::optional<SkColor> AvatarToolbarButtonDelegate::GetHighlightTextColor(
    const ui::ColorProvider* const color_provider) const {
  std::optional<SkColor> color;
  switch (ComputeState()) {
    case ButtonState::kIncognitoProfile:
      color = color_provider->GetColor(
          kColorAvatarButtonHighlightIncognitoForeground);
      break;
    case ButtonState::kSyncError:
      color = color_provider->GetColor(
          kColorAvatarButtonHighlightSyncErrorForeground);
      break;
    case ButtonState::kSyncPaused:
      color =
          color_provider->GetColor(kColorAvatarButtonHighlightNormalForeground);
      break;
    case ButtonState::kGuestSession:
    case ButtonState::kExplicitTextShowing:
    case ButtonState::kAnimatedUserIdentity:
      color = color_provider->GetColor(
          kColorAvatarButtonHighlightDefaultForeground);
      break;
    case ButtonState::kWork:
    case ButtonState::kSchool:
      color =
          color_provider->GetColor(kColorAvatarButtonHighlightNormalForeground);
      break;
    case ButtonState::kNormal:
      color = color_provider->GetColor(
          kColorAvatarButtonHighlightDefaultForeground);
      break;
  }
  return color;
}

std::u16string AvatarToolbarButtonDelegate::GetAvatarTooltipText() const {
  switch (ComputeState()) {
    case ButtonState::kIncognitoProfile:
      return l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_INCOGNITO_TOOLTIP);
    case ButtonState::kGuestSession:
      return l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_GUEST_TOOLTIP);
    case ButtonState::kAnimatedUserIdentity:
      return GetShortProfileName();
    // kSyncPaused is just a type of sync error with different color, but should
    // still use GetAvatarSyncErrorDescription() as tooltip.
    case ButtonState::kSyncError:
    case ButtonState::kSyncPaused: {
      std::optional<AvatarSyncErrorType> error = GetAvatarSyncErrorType();
      DCHECK(error);
      return l10n_util::GetStringFUTF16(
          IDS_AVATAR_BUTTON_SYNC_ERROR_TOOLTIP, GetShortProfileName(),
          GetAvatarSyncErrorDescription(
              *error, IdentityManagerFactory::GetForProfile(profile_)
                          ->HasPrimaryAccount(signin::ConsentLevel::kSync)));
    }
    case ButtonState::kExplicitTextShowing:
    case ButtonState::kWork:
    case ButtonState::kSchool:
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
      case ButtonState::kGuestSession:
      case ButtonState::kExplicitTextShowing:
      case ButtonState::kAnimatedUserIdentity:
        break;
      case ButtonState::kSyncPaused:
        ripple_color_id = kColorAvatarButtonNormalRipple;
        break;
      case ButtonState::kSchool:
      case ButtonState::kWork:
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
    case ButtonState::kAnimatedUserIdentity:
    case ButtonState::kSyncError:
    // TODO(crbug.com/1191411): If sync-the-feature is disabled, the icon
    // should be different.
    case ButtonState::kSyncPaused:
    case ButtonState::kSchool:
    case ButtonState::kWork:
    case ButtonState::kNormal:
      return ui::ImageModel::FromImage(profiles::GetSizedAvatarIcon(
          GetProfileAvatarImage(icon_size), icon_size, icon_size,
          profiles::SHAPE_CIRCLE));
  }
}

bool AvatarToolbarButtonDelegate::ShouldPaintBorder() const {
  switch (ComputeState()) {
    case ButtonState::kGuestSession:
    case ButtonState::kAnimatedUserIdentity:
    case ButtonState::kNormal:
      return true;
    case ButtonState::kIncognitoProfile:
    case ButtonState::kExplicitTextShowing:
    case ButtonState::kWork:
    case ButtonState::kSchool:
    case ButtonState::kSyncPaused:
    case ButtonState::kSyncError:
      return false;
  }
}
