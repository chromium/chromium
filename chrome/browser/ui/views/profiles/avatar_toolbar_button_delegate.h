// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_AVATAR_TOOLBAR_BUTTON_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_AVATAR_TOOLBAR_BUTTON_DELEGATE_H_

#include <optional>
#include <string>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_service_observer.h"
#include "ui/gfx/image/image.h"

class Browser;
class Profile;

// Handles the business logic for AvatarToolbarButton.
// Listens to Chrome and Profile changes in order to compute the proper state of
// the button. This state is used to compute the information requested by
// the button to be shown, such as Text and color, Icon, tooltip text etc...
// The different states that can be reached:
// - Regular state: regular browsing session.
// - Private mode: Incognito or Guest browser sessions.
// - Identity name shown: the identity name is shown for a short period of time.
//   This can be triggered by identity changes in Chrome or when an IPH is
//   showing.
// - Explicit modifications override: such as displaying specific text when
//   intercept bubbles are displayed.
// - Sync paused/error state.
class AvatarToolbarButtonDelegate : public BrowserListObserver,
                                    public ProfileAttributesStorage::Observer,
                                    public signin::IdentityManager::Observer,
                                    public syncer::SyncServiceObserver {
 public:
  AvatarToolbarButtonDelegate(AvatarToolbarButton* button, Browser* browser);

  AvatarToolbarButtonDelegate(const AvatarToolbarButtonDelegate&) = delete;
  AvatarToolbarButtonDelegate& operator=(const AvatarToolbarButtonDelegate&) =
      delete;

  ~AvatarToolbarButtonDelegate() override;

  // These info are based on the `ButtonState`.
  std::pair<std::u16string, std::optional<SkColor>> GetTextAndColor(
      const ui::ColorProvider* const color_provider) const;
  std::optional<SkColor> GetHighlightTextColor(
      const ui::ColorProvider* const color_provider) const;
  std::u16string GetAvatarTooltipText() const;
  std::pair<ChromeColorIds, ChromeColorIds> GetInkdropColors() const;
  ui::ImageModel GetAvatarIcon(int icon_size, SkColor icon_color) const;
  bool ShouldPaintBorder() const;

  [[nodiscard]] base::ScopedClosureRunner ShowExplicitText(
      const std::u16string& text);

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  void MaybeShowEnterpriseText();
#endif
  void ShowDefaultText();

  // Should be called when the icon is updated. This may trigger the identity
  // pill animation if the delegate is waiting for the image.
  void MaybeShowIdentityAnimation();

  // Enables or disables the IPH highlight.
  void SetHasInProductHelpPromo(bool has_promo);

  // Called by the AvatarToolbarButton to notify the delegate about events.
  void OnMouseExited();
  void OnBlur();
  void OnThemeChanged(const ui::ColorProvider* color_provider);

 private:
  // Internal text state
  enum class TextState {
    kNotShowing,
    kWaitingForImage,
    kShowingName,
    kShowingExplicitText,
    kShowingEnterpriseText,
  };

  // States of the button ordered in priority of getting displayed.
  enum class ButtonState {
    kIncognitoProfile,
    kGuestSession,
    kExplicitTextShowing,
    kAnimatedUserIdentity,
    kSyncPaused,
    // An error in sync-the-feature or sync-the-transport.
    kSyncError,
    kWork,
    kSchool,
    kNormal
  };

  ButtonState ComputeState() const;

  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override;
  void OnBrowserRemoved(Browser* browser) override;

  // ProfileAttributesStorage::Observer:
  void OnProfileAvatarChanged(const base::FilePath& profile_path) override;
  void OnProfileHighResAvatarLoaded(
      const base::FilePath& profile_path) override;
  void OnProfileNameChanged(const base::FilePath& profile_path,
                            const std::u16string& old_profile_name) override;
  void OnProfileUserManagementAcceptanceChanged(
      const base::FilePath& profile_path) override;

  // IdentityManager::Observer:
  // Needed if the first sync promo account should be displayed.
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event) override;
  void OnRefreshTokensLoaded() override;
  void OnAccountsInCookieUpdated(
      const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
      const GoogleServiceAuthError& error) override;
  void OnExtendedAccountInfoUpdated(const AccountInfo& info) override;
  void OnExtendedAccountInfoRemoved(const AccountInfo& info) override;
  void OnIdentityManagerShutdown(signin::IdentityManager*) override;

  // SyncServiceObserver:
  void OnStateChanged(syncer::SyncService*) override;
  void OnSyncShutdown(syncer::SyncService*) override;

  // Initiates showing the identity.
  void OnUserIdentityChanged();

  void OnIdentityAnimationTimeout();
  // Called after the user interacted with the button or after some timeout.
  void MaybeHideIdentityAnimation();

  // Shows the identity pill animation. If the animation is already showing,
  // this extends the duration of the current animation.
  void ShowIdentityAnimation();

  TextState GetDefaultTextState() const;

  std::u16string GetProfileName() const;
  std::u16string GetShortProfileName() const;
  // Must only be called in states which have an avatar image (i.e. not
  // kGuestSession and not kIncognitoProfile).
  gfx::Image GetProfileAvatarImage(int preferred_size) const;
  // Returns the count of incognito or guest windows attached to the profile.
  int GetWindowCount() const;
  std::optional<AvatarSyncErrorType> GetAvatarSyncErrorType() const;
  gfx::Image GetGaiaAccountImage() const;

  // Callback used to remove the explicit text shown and reset to the default.
  void ClearExplicitText();

  base::ScopedObservation<ProfileAttributesStorage,
                          ProfileAttributesStorage::Observer>
      profile_observation_{this};
  base::ScopedObservation<syncer::SyncService, syncer::SyncServiceObserver>
      sync_service_observation_{this};
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};

  const raw_ptr<AvatarToolbarButton> avatar_toolbar_button_;
  const raw_ptr<Browser> browser_;
  const raw_ptr<Profile> profile_;
  TextState button_text_state_ = TextState::kNotShowing;

  // Count of identity pill animation timeouts that are currently scheduled.
  // Multiple timeouts are scheduled when multiple animation triggers happen
  // in a quick sequence (before the first timeout passes). The identity pill
  // tries to close when this reaches 0.
  int identity_animation_timeout_count_ = 0;

  bool enterprise_text_hide_scheduled_ = false;

  bool refresh_tokens_loaded_ = false;
  bool has_in_product_help_promo_ = false;

  // Caches the value of the last error so the class can detect when it
  // changes and notify |avatar_toolbar_button_|.
  std::optional<AvatarSyncErrorType> last_avatar_error_;

  // Text to be displayed while the state is
  // `ButtonState::kExplicitTextShowing`.
  std::u16string explicit_text_;
  // Internal pointer to the current explicit closure. This is used if multiple
  // explicit content is trying to be shown at the same time. Priority to the
  // last call.
  raw_ptr<base::ScopedClosureRunner> hide_explicit_closure_ptr_ = nullptr;

  base::WeakPtrFactory<AvatarToolbarButtonDelegate> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_AVATAR_TOOLBAR_BUTTON_DELEGATE_H_
