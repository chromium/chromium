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
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/gaia_id.h"
#include "ui/base/models/image_model.h"

class Browser;
class Profile;
class AvatarToolbarButton;
enum class AvatarDelayType;

namespace ui {
class ColorProvider;
}

class StateObserver;

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

  // Returns the avatar icon.
  virtual ui::ImageModel GetAvatarIcon(
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
  // States of the button ordered in priority of getting displayed.
  // The order of those values is used with the `StateManager` to make sure the
  // active state with the highest priority is shown.
  // The lower the value of the enum, the higher the priority.
  enum class ButtonState {
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
    kHistorySyncOptin,
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
    // Includes Work and School.
    kManagement,
    kNormal
  };

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
    virtual void OnButtonStateChanged(std::optional<ButtonState> old_state,
                                      ButtonState new_state) = 0;

    virtual ~Observer() = default;
  };

  explicit AvatarToolbarButtonStateManager(
      AvatarToolbarButton& avatar_toolbar_button,
      Browser* browser);
  ~AvatarToolbarButtonStateManager() override;

  // This needs to be separated from the constructor since it might call
  // updates, which will try to access the `StateManager`.
  void InitializeStates();

  StateProvider* GetActiveStateProvider() const;

  // Special setter for the explicit state as it is controlled externally.
  base::ScopedClosureRunner SetExplicitState(
      const std::u16string& text,
      std::optional<std::u16string> accessibility_label,
      std::optional<base::RepeatingCallback<void(bool is_source_accelerator)>>
          action);

  // Returns whether the explicit state is set.
  bool HasExplicitButtonState() const;

  // Testing functions: check `AvatarToolbarButton` equivalent functions.
  [[nodiscard]] static base::AutoReset<std::optional<base::TimeDelta>>
  CreateScopedInfiniteDelayOverrideForTesting(AvatarDelayType delay_type);
  void ClearActiveStateForTesting();
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  [[nodiscard]] static base::AutoReset<std::optional<base::TimeDelta>>
  CreateScopedZeroDelayOverrideSigninPendingTextForTesting();

  // WARNING: Check `AvatarToolbarButton::ForceShowingPromoForTesting()` before
  // using.
  void ForceShowingPromoForTesting();
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

 private:
  // Creates all main states and attach listeners.
  void CreateStatesAndListeners(Browser* browser);

  // StateObserver:
  void OnStateProviderUpdateRequest(StateProvider* requesting_state) override;

  // Computes the current active state with the highest priority.
  // Multiple states could be active at the same time.
  void ComputeButtonActiveState();

  // `AvatarToolbarButton::UpdateIcon()` will notify observers, the
  // `ShowIdentityNameStateProvider` being one of the observers.
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

  //  ProfileAttributesStorage::Observer:
  void OnProfileAvatarChanged(const base::FilePath&) override;

  void OnProfileHighResAvatarLoaded(const base::FilePath&) override;

  void OnProfileNameChanged(const base::FilePath&,
                            const std::u16string&) override;

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

  const raw_ref<Profile> profile_;

  std::vector<raw_ref<Observer>> state_manager_observers_;
};

void SigninDetectionServiceFactoryEnsureFactoryBuilt();

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_AVATAR_TOOLBAR_BUTTON_STATE_MANAGER_H_
