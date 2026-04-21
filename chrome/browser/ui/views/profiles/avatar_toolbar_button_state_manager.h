// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_AVATAR_TOOLBAR_BUTTON_STATE_MANAGER_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_AVATAR_TOOLBAR_BUTTON_STATE_MANAGER_H_

#include <optional>
#include <string>

#include "base/auto_reset.h"
#include "base/callback_list.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button_types.h"
#include "chrome/browser/ui/views/toolbar/avatar_toolbar_button_interface.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/gaia_id.h"
#include "ui/base/models/image_model.h"

class Browser;
class Profile;

namespace ui {
class ColorProvider;
}

class StateObserver;

// States of the button ordered in priority of getting displayed.
// The order of those values is used with the `StateManager` to make sure the
// active state with the highest priority is shown.
// The lower the value of the enum, the higher the priority.
enum class AvatarToolbarButtonState {
  kGuestSession,
  kIncognitoProfile,
  kExplicitTextShowing,
  kOnSignin,
  kShowIdentityName,
  kSigninPending,
  kSyncPaused,
  kUpgradeClientError,
  kPassphraseError,
  kBookmarksLimitExceeded,
  // Catch-all for remaining errors in sync-the-feature or sync-the-transport
  // (this includes Trusted Vault locked Sync error).
  kSyncError,
  kPasskeysLockedError,
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  // Any promo presented through expanding the button. This includes any promo
  // listed in `signin::ProfileMenuAvatarButtonPromoInfo::Type`.
  kPromo,
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
  // Includes Work and School.
  kManagement,
  kNormal
};

// Provides the information needed to display a specific button state.
// This class provides a default implementation for button appearance/behavior,
// the derived classes can override any of the `StateProvider` methods to
// provide a specific button appearance/behavior. The text shown on the button
// is state specific, therefore derived classes must override the `GetText()`
// method.
class StateProvider {
 public:
  // The constructor should not call any function that would end up calling
  // `RequestUpdate()` as it could end up trying to compute the active state,
  // which is not guaranteed to return a valid state at this point since all the
  // main states might not be created yet.
  // Consider overriding `Init()` if you need to add a potential code to
  // `RequestUpdate()`. The init method will be
  // called right after all the main states are created.
  explicit StateProvider(Profile* profile, StateObserver* state_observer);

  virtual ~StateProvider();

  // TODO(crbug.com/324018028): Consider changing `IsActive()` to be non-virtual
  // and return a member variable `is_active_` that can be controlled by the
  // derived classes that sets the active/inactive state when needed, also
  // requesting updates on state change. This way we would make sure not to miss
  // updates when a state activation changes.
  virtual bool IsActive() const = 0;

  // This method should be used to initialize anything that could potentially
  // call a `RequestUpdate()` which would end up computing the active state.
  // This method will be called after all main states are created, making sure
  // that an active state will be correctly computed.
  virtual void Init();

  // Returns the text to be shown on the button.
  virtual std::u16string GetText() const = 0;

  // Returns the highlight color of the button.
  virtual std::optional<SkColor> GetHighlightColor(
      const ui::ColorProvider& color_provider) const;

  // Returns the text color of the button.
  virtual std::optional<SkColor> GetHighlightTextColor(
      const ui::ColorProvider& color_provider) const;

  // Returns the avatar icon and its type.
  virtual std::pair<ui::ImageModel, AvatarIconType> GetAvatarIcon(
      int icon_size,
      SkColor /*icon_color*/,
      const ui::ColorProvider& color_provider) const;

  // Returns the tooltip text of the avatar button.
  virtual std::u16string GetAvatarTooltipText() const;

  // Returns ink drop colors as a pair of hover and ripple colors of the
  // button.
  virtual std::pair<ChromeColorIds, ChromeColorIds> GetInkdropColors() const;

  // Returns whether the border should be painted for the button.
  virtual bool ShouldPaintBorder() const;

  // Returns the accessibility label of the button.
  virtual std::optional<std::u16string> GetAccessibilityLabel() const;

  // Returns the action to be used when the button is pressed. This is used to
  // override the default action of the button (defined by
  // `AvatarToolbarButtonDelegate`) when it is pressed.
  virtual std::optional<base::RepeatingCallback<void(bool)>>
  GetButtonActionOverride();

  virtual void OnButtonPressed();
  virtual void OnIconUpdated();
  virtual void OnMouseExited();
  virtual void OnBlur();
  virtual void OnIPHPromoChanged(bool has_promo);

  virtual void OnButtonStateChanged(
      std::optional<AvatarToolbarButtonState> old_state,
      AvatarToolbarButtonState new_state);

  // Clears the state (makes it inactive). Should be used only for testing
  // purposes.
  virtual void ClearForTesting();

 protected:
  // This update request will attempt to update the text shown on the button.
  // The update will only go through if the requesting state was the main button
  // active one and is now inactive or if it is currently the main active one.
  // Therefore every time a `StateProvider` expects a change of internal state
  // it should call this method to attempt to propagate the changes.
  void RequestUpdate();

  Profile& profile() const;

 private:
  const raw_ref<Profile> profile_;
  const raw_ref<StateObserver> state_observer_;
};

class StateObserver {
 public:
  virtual void OnStateProviderUpdateRequest(StateProvider* state_provider) = 0;

  virtual ~StateObserver() = default;
};

// Container of all the states and returns the active state with the highest
// priority.
// All states are initialized at construction based on the Profile type.
// Exception for `ButtonState::kExplicitTextShowing` with
// `ExplicitStateProvider`  which is the only state that can be added
// dynamically and controlled externally. It has to be part of the
// `StateManager` however to properly compute the current active state.
// This class also listens to Profile changes that should affect the global
// state of the button, for changes that should occur regardless of the current
// active state for Regular Profiles.
class AvatarToolbarButtonStateManager
    : public StateObserver,
      public signin::IdentityManager::Observer,
      public ProfileAttributesStorage::Observer {
 public:
  using ButtonState = AvatarToolbarButtonState;

  // Observer is used to observe changes in the active button state.
  //
  // NOTE: This should only be used by `StateProvider`(s) if they really need to
  // know when the active state changes. `StateProvider`(s) should be as
  // independent as possible and in most cases this is not needed.
  class Observer {
   public:
    // Called by `StateManager` when the active button state changes.
    // `old_state` will be `std::nullopt` if there was no active state before
    // (i.e. initialization).
    virtual void OnButtonStateChanged(
        std::optional<AvatarToolbarButtonState> old_state,
        ButtonState new_state) = 0;

    virtual ~Observer() = default;
  };

  explicit AvatarToolbarButtonStateManager(
      AvatarToolbarButtonInterface& avatar_control,
      Browser* browser);
  ~AvatarToolbarButtonStateManager() override;

  // This needs to be separated from the constructor since it might call
  // updates, which will try to access the `StateManager`.
  void InitializeStates();

  Browser* browser() const { return browser_; }

  StateProvider* GetActiveStateProvider() const;

  // Methods to register or remove observers of the button.
  void AddObserver(AvatarToolbarButtonInterface::Observer* observer);
  void RemoveObserver(AvatarToolbarButtonInterface::Observer* observer);

  // Methods to register observers of the button state.
  void RegisterObserver(Observer* observer);

  void NotifyMouseExited();
  void NotifyBlur();
  void NotifyButtonPressed();
  void NotifyIconUpdated();
  void NotifyIPHPromoChanged(bool has_promo);

  // Special setter for the explicit state as it is controlled externally.
  base::ScopedClosureRunner SetExplicitState(
      const std::u16string& text,
      std::optional<std::u16string> accessibility_label,
      std::optional<base::RepeatingCallback<void(bool is_source_accelerator)>>
          action);

  // Returns whether the explicit state is set.
  bool HasExplicitButtonState() const;

  // Shared button press logic.
  void HandleButtonPressed(bool is_source_accelerator);

  // Shared IPH methods.
  void MaybeShowProfileSwitchIPH();
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  void MaybeShowSupervisedUserSignInIPH();
  void MaybeShowSignInBenefitsIPH();
#endif
  void MaybeShowExplicitBrowserSigninPreferenceRememberedIPH(
      const AccountInfo& account_info);

  // Shared accessibility logic. Returns the name and description.
  std::pair<std::u16string, std::u16string> GetAccessibilityLabels(
      std::u16string_view button_text) const;

  // Testing functions: check `AvatarToolbarButton` equivalent functions.
  [[nodiscard]] static base::AutoReset<std::optional<base::TimeDelta>>
  CreateScopedInfiniteDelayOverrideForTesting(AvatarDelayType delay_type);

  // static
  [[nodiscard]] static base::AutoReset<base::TimeDelta>
  SetScopedIPHMinDelayAfterCreationForTesting(base::TimeDelta delay);

  // Do not show the IPH right when creating the window, so that the IPH has a
  // separate animation.
  static base::TimeDelta g_iph_min_delay_after_creation;

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  [[nodiscard]] static base::AutoReset<std::optional<base::TimeDelta>>
  CreateScopedZeroDelayOverrideSigninPendingTextForTesting();

  // WARNING: Check `AvatarToolbarButton::ForceShowingPromoForTesting()` before
  // using.
  void ForceShowingPromoForTesting();

  // Returns whether the delay timer was running or not.
  // Stops the timer if it is running.
  bool GetStateAndFireSignedOutTriggerDelayTimerForTesting();
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

 private:
  // Creates all main states and attach listeners.
  void CreateStatesAndListeners(Browser* browser);

  // StateObserver:
  void OnStateProviderUpdateRequest(StateProvider* requesting_state) override;

  // Computes the current active state with the highest priority.
  // Multiple states could be active at the same time.
  void ComputeButtonActiveState();

  // `AvatarToolbarButtonInterface::UpdateIcon()` will notify observers,
  // the `ShowIdentityNameStateProvider` being one of the observers.
  void UpdateButtonIcon();

  void UpdateButtonText();

  // This is mainly used `OnStateProviderUpdateRequest()` where not all of the
  // state transitions update all of the button properties. Consider adding a
  // filter if this is impacting performance.
  void UpdateAvatarButton();

  // signin::IdentityManager::Observer:
  void OnIdentityManagerShutdown(signin::IdentityManager*) override;

  void OnRefreshTokensLoaded() override;

  void OnAccountsInCookieUpdated(const signin::AccountsInCookieJarInfo&,
                                 const GoogleServiceAuthError&) override;

  void OnExtendedAccountInfoUpdated(const AccountInfo&) override;

  void OnExtendedAccountInfoRemoved(const AccountInfo&) override;

  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override;

  //  ProfileAttributesStorage::Observer:
  void OnProfileAvatarChanged(const base::FilePath&) override;

  void OnProfileHighResAvatarLoaded(const base::FilePath&) override;

  void OnProfileNameChanged(const base::FilePath&,
                            const std::u16string&) override;

  base::ObserverList<
      AvatarToolbarButtonInterface::Observer,
      /*check_empty=*/true,
      base::ObserverListReentrancyPolicy::kAllowReentrancyUntriaged>
      observer_list_;

  base::flat_map<ButtonState, std::unique_ptr<StateProvider>> states_;
  raw_ref<AvatarToolbarButtonInterface> avatar_control_;
  const raw_ptr<Browser> browser_;

  // Active state per the last request to `ComputeButtonActiveState()`.
  // Pointer to the active element of `states_` with the highest priority.
  raw_ptr<std::pair<ButtonState, std::unique_ptr<StateProvider>>>
      current_active_state_pair_ = nullptr;

  bool is_updating_ = false;
  bool is_initializing_ = false;
  bool was_update_requested_ = false;

  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      scoped_identity_manager_observation_{this};
  base::ScopedObservation<ProfileAttributesStorage,
                          ProfileAttributesStorage::Observer>
      profile_observation_{this};

  const raw_ref<Profile> profile_;

  // Gaia Id of the account that was signed in from having it's choice
  // remembered following a web sign-in event but waiting for the available
  // account information to be fetched in order to show the sign in IPH.
  GaiaId gaia_id_for_signin_choice_remembered_;

  // Time when this object was created.
  const base::TimeTicks creation_time_;

  std::vector<raw_ref<Observer>> state_manager_observers_;

  base::WeakPtrFactory<AvatarToolbarButtonStateManager> weak_ptr_factory_{this};
};

void SigninDetectionServiceFactoryEnsureFactoryBuilt();

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_AVATAR_TOOLBAR_BUTTON_STATE_MANAGER_H_
