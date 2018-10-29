// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_CHOOSER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_CHOOSER_VIEW_H_

#include <stddef.h>

#include <map>
#include <vector>

#include "base/macros.h"
#include "chrome/browser/profiles/avatar_menu.h"
#include "chrome/browser/profiles/avatar_menu_observer.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/profile_chooser_constants.h"
#include "chrome/browser/ui/views/close_bubble_on_tab_activation_helper.h"
#include "chrome/browser/ui/views/profiles/dice_accounts_menu.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "content/public/browser/web_contents_delegate.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/controls/styled_label_listener.h"

namespace views {
class GridLayout;
class ImageButton;
class Link;
class LabelButton;
}

class Browser;
class DiceSigninButtonView;
class HoverButton;

// This bubble view is displayed when the user clicks on the avatar button.
// It displays a list of profiles and allows users to switch between profiles.
class ProfileChooserView : public content::WebContentsDelegate,
                           public views::BubbleDialogDelegateView,
                           public views::ButtonListener,
                           public views::LinkListener,
                           public views::StyledLabelListener,
                           public AvatarMenuObserver,
                           public OAuth2TokenService::Observer {
 public:
  // Shows the bubble if one is not already showing.  This allows us to easily
  // make a button toggle the bubble on and off when clicked: we unconditionally
  // call this function when the button is clicked and if the bubble isn't
  // showing it will appear while if it is showing, nothing will happen here and
  // the existing bubble will auto-close due to focus loss.
  // There are 2 ways to position the Bubble, if |anchor_button| is set, then
  // |parent_window| and |anchor_rect| are ignored. Otherwise, |parent_window|
  // and |anchor_rect| have to be set.
  static void ShowBubble(
      profiles::BubbleViewMode view_mode,
      const signin::ManageAccountsParams& manage_accounts_params,
      signin_metrics::AccessPoint access_point,
      views::Button* anchor_button,
      gfx::NativeView parent_window,
      const gfx::Rect& anchor_rect,
      Browser* browser,
      bool is_source_keyboard);
  static bool IsShowing();
  static void Hide();

  const Browser* browser() const { return browser_; }

 private:
  friend class ProfileChooserViewExtensionsTest;

  typedef std::vector<size_t> Indexes;
  typedef std::map<views::Button*, int> ButtonIndexes;
  typedef std::map<views::Button*, std::string> AccountButtonIndexes;

  ProfileChooserView(views::Button* anchor_button,
                     Browser* browser,
                     profiles::BubbleViewMode view_mode,
                     signin::GAIAServiceType service_type,
                     signin_metrics::AccessPoint access_point);
  ~ProfileChooserView() override;

  // views::BubbleDialogDelegateView:
  void Init() override;
  void OnNativeThemeChanged(const ui::NativeTheme* native_theme) override;
  void WindowClosing() override;
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  views::View* GetInitiallyFocusedView() override;
  int GetDialogButtons() const override;

  // content::WebContentsDelegate:
  bool HandleContextMenu(const content::ContextMenuParams& params) override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::LinkListener:
  void LinkClicked(views::Link* sender, int event_flags) override;

  // views::StyledLabelListener:
  void StyledLabelLinkClicked(views::StyledLabel* label,
                              const gfx::Range& range,
                              int event_flags) override;

  // AvatarMenuObserver:
  void OnAvatarMenuChanged(AvatarMenu* avatar_menu) override;

  // OAuth2TokenService::Observer overrides.
  void OnRefreshTokenAvailable(const std::string& account_id) override;
  void OnRefreshTokenRevoked(const std::string& account_id) override;

  static ProfileChooserView* profile_bubble_;

  // We normally close the bubble any time it becomes inactive but this can lead
  // to flaky tests where unexpected UI events are triggering this behavior.
  // Tests set this to "false" for more consistent operation.
  static bool close_on_deactivate_for_testing_;

  void ResetView();

  // Shows the bubble with the |view_to_display|.
  void ShowView(profiles::BubbleViewMode view_to_display,
                AvatarMenu* avatar_menu);
  void ShowViewFromMode(profiles::BubbleViewMode mode);

  // Focuses the first profile button in the menu list.
  void FocusFirstProfileButton();

  // Creates the profile chooser view.
  views::View* CreateProfileChooserView(AvatarMenu* avatar_menu);

  // Creates the main profile card for the profile |avatar_item|. |is_guest|
  // is used to determine whether to show any Sign in/Sign out/Manage accounts
  // links.
  views::View* CreateCurrentProfileView(
      const AvatarMenu::Item& avatar_item,
      bool is_guest);
  views::View* CreateGuestProfileView();
  views::View* CreateOptionsView(bool display_lock, AvatarMenu* avatar_menu);
  views::View* CreateSupervisedUserDisclaimerView();
  views::View* CreateAutofillHomeView();

  // Account Management view for the profile |avatar_item|.
  views::View* CreateCurrentProfileAccountsView(
      const AvatarMenu::Item& avatar_item);
  void CreateAccountButton(views::GridLayout* layout,
                           const std::string& account_id,
                           bool is_primary_account,
                           bool reauth_required,
                           int width);

  // Creates the DICE UI view to sign in and turn on sync. It includes an
  // illustration, a promo and a button.
  views::View* CreateDiceSigninView();

  // Creates a view to confirm account removal for |account_id_to_remove_|.
  views::View* CreateAccountRemovalView();

  // Removes the currently selected account and attempts to restart Chrome.
  void RemoveAccount();

  // Creates a header for signin and sync error surfacing for the user menu.
  views::View* CreateSyncErrorViewIfNeeded(const AvatarMenu::Item& avatar_item);

  // Creates a view showing the profile associated with |avatar_item| and an
  // error button below.
  views::View* CreateDiceSyncErrorView(const AvatarMenu::Item& avatar_item,
                                       sync_ui_util::AvatarSyncErrorType error,
                                       int button_string_id);

  bool ShouldShowGoIncognito() const;

  // Return maximal height for the view after which it becomes scrollable.
  // TODO(crbug.com/870303): remove when a general solution is available.
  int GetMaxHeight() const;

  // Clean-up done after an action was performed in the ProfileChooser.
  void PostActionPerformed(ProfileMetrics::ProfileDesktopMenu action_performed);

  // Callbacks for DiceAccountsMenu.
  void EnableSync(const base::Optional<AccountInfo>& account);
  void SignOutAllWebAccounts();

  // Methods to keep track of the number of times the Dice sign-in promo has
  // been shown.
  int GetDiceSigninPromoShowCount() const;
  void IncrementDiceSigninPromoShowCount();

  std::unique_ptr<AvatarMenu> avatar_menu_;
  Browser* const browser_;
  views::Button* const anchor_button_;

  // Other profiles used in the "fast profile switcher" view.
  ButtonIndexes open_other_profile_indexes_map_;

  // Buttons associated with the current profile.
  AccountButtonIndexes delete_account_button_map_;
  AccountButtonIndexes reauth_account_button_map_;

  // Button in the signin/sync error header on top of the desktop user menu.
  views::LabelButton* sync_error_button_;

  // Links and buttons displayed in the active profile card.
  views::Link* manage_accounts_link_;
  views::LabelButton* manage_accounts_button_;
  views::LabelButton* signin_current_profile_button_;
  HoverButton* sync_to_another_account_button_;
  views::LabelButton* signin_with_gaia_account_button_;

  // For material design user menu, the active profile card owns the profile
  // name and photo.
  views::LabelButton* current_profile_card_;

  // Action buttons.
  views::LabelButton* first_profile_button_;
  views::LabelButton* guest_profile_button_;
  views::LabelButton* users_button_;
  views::LabelButton* go_incognito_button_;
  views::LabelButton* lock_button_;
  views::LabelButton* close_all_windows_button_;
  views::Link* add_account_link_;
  views::LabelButton* passwords_button_;
  views::LabelButton* credit_cards_button_;
  views::LabelButton* addresses_button_;
  views::LabelButton* signout_button_;

  // Buttons displayed in the gaia signin view.
  views::ImageButton* gaia_signin_cancel_button_;

  // Links and buttons displayed in the account removal view.
  views::LabelButton* remove_account_button_;
  views::ImageButton* account_removal_cancel_button_;

  // View for the signin/turn-on-sync button in the dice promo.
  DiceSigninButtonView* dice_signin_button_view_;

  // Records the account id to remove.
  std::string account_id_to_remove_;

  // Active view mode.
  profiles::BubbleViewMode view_mode_;

  // The GAIA service type provided in the response header.
  signin::GAIAServiceType gaia_service_type_;

  // The current access point of sign in.
  const signin_metrics::AccessPoint access_point_;

  CloseBubbleOnTabActivationHelper close_bubble_helper_;

  // Accounts that are presented in the enable sync promo.
  std::vector<AccountInfo> dice_sync_promo_accounts_;

  // Accounts submenu that is shown when |sync_to_another_account_button_| is
  // pressed.
  std::unique_ptr<DiceAccountsMenu> dice_accounts_menu_;

  const bool dice_enabled_;

  const int menu_width_;

  DISALLOW_COPY_AND_ASSIGN(ProfileChooserView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_CHOOSER_VIEW_H_
