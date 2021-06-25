// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_MENU_VIEW_BASE_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_MENU_VIEW_BASE_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/close_bubble_on_tab_activation_helper.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/style/typography.h"

class Browser;

namespace views {
class Button;
}  // namespace views

namespace ui {
class ImageModel;
}  // namespace ui

// This class provides the UI for different menus that are created by user
// clicking the avatar button.
class ProfileMenuViewBase : public content::WebContentsDelegate,
                            public views::BubbleDialogDelegateView {
 public:
  // Enumeration of all actionable items in the profile menu.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class ActionableItem {
    kManageGoogleAccountButton = 0,
    kPasswordsButton = 1,
    kCreditCardsButton = 2,
    kAddressesButton = 3,
    kGuestProfileButton = 4,
    kManageProfilesButton = 5,
    // DEPRECATED: kLockButton = 6,
    kExitProfileButton = 7,
    kSyncErrorButton = 8,
    // DEPRECATED: kCurrentProfileCard = 9,
    // Note: kSigninButton and kSigninAccountButton should probably be renamed
    // to kSigninAndEnableSyncButton and kEnableSyncForSignedInAccountButton.
    kSigninButton = 10,
    kSigninAccountButton = 11,
    kSignoutButton = 12,
    kOtherProfileButton = 13,
    kCookiesClearedOnExitLink = 14,
    kAddNewProfileButton = 15,
    kSyncSettingsButton = 16,
    kEditProfileButton = 17,
    kCreateIncognitoShortcutButton = 18,
    kMaxValue = kCreateIncognitoShortcutButton,
  };

  enum class SyncInfoContainerBackgroundState {
    kNoError,
    kPaused,
    kError,
    kNoPrimaryAccount,
  };

  struct SyncInfo {
    int description_string_id;
    int button_string_id;
    SyncInfoContainerBackgroundState background_state;
  };

  struct EditButtonParams {
    EditButtonParams(const gfx::VectorIcon* edit_icon,
                     const std::u16string& edit_tooltip_text,
                     base::RepeatingClosure edit_action);
    EditButtonParams(const EditButtonParams&);
    ~EditButtonParams();

    const gfx::VectorIcon* edit_icon;
    std::u16string edit_tooltip_text;
    base::RepeatingClosure edit_action;
  };

  // Size of the large identity image in the menu.
  static constexpr int kIdentityImageSize = 64;

  // Shows the bubble if one is not already showing.  This allows us to easily
  // make a button toggle the bubble on and off when clicked: we unconditionally
  // call this function when the button is clicked and if the bubble isn't
  // showing it will appear while if it is showing, nothing will happen here and
  // the existing bubble will auto-close due to focus loss.
  static void ShowBubble(
      profiles::BubbleViewMode view_mode,
      views::Button* anchor_button,
      Browser* browser,
      bool is_source_keyboard);

  static bool IsShowing();
  static void Hide();

  static ProfileMenuViewBase* GetBubbleForTesting();

  ProfileMenuViewBase(views::Button* anchor_button,
                      Browser* browser);
  ~ProfileMenuViewBase() override;

  ProfileMenuViewBase(const ProfileMenuViewBase&) = delete;
  ProfileMenuViewBase& operator=(const ProfileMenuViewBase&) = delete;

  // This method is called once to add all menu items.
  virtual void BuildMenu() = 0;

  // Override to supply a sync icon for the profile menu.
  virtual gfx::ImageSkia GetSyncIcon() const;

  // If |profile_name| is empty, no heading will be displayed.
  void SetProfileIdentityInfo(
      const std::u16string& profile_name,
      SkColor profile_background_color,
      base::Optional<EditButtonParams> edit_button_params,
      const ui::ImageModel& image_model,
      const std::u16string& title,
      const std::u16string& subtitle = std::u16string(),
      const ui::ThemedVectorIcon& avatar_header_art = ui::ThemedVectorIcon());
  void SetSyncInfo(const SyncInfo& sync_info,
                   const base::RepeatingClosure& action,
                   bool show_badge);
  void AddShortcutFeatureButton(const gfx::VectorIcon& icon,
                                const std::u16string& text,
                                base::RepeatingClosure action);
  void AddFeatureButton(const std::u16string& text,
                        base::RepeatingClosure action,
                        const gfx::VectorIcon& icon = gfx::kNoneIcon,
                        float icon_to_image_ratio = 1.0f);
  void SetProfileManagementHeading(const std::u16string& heading);
  void AddSelectableProfile(const ui::ImageModel& image_model,
                            const std::u16string& name,
                            bool is_guest,
                            base::RepeatingClosure action);
  void AddProfileManagementShortcutFeatureButton(const gfx::VectorIcon& icon,
                                                 const std::u16string& text,
                                                 base::RepeatingClosure action);
  void AddProfileManagementFeatureButton(const gfx::VectorIcon& icon,
                                         const std::u16string& text,
                                         base::RepeatingClosure action);

  gfx::ImageSkia ColoredImageForMenu(const gfx::VectorIcon& icon,
                                     SkColor color) const;
  // Should be called inside each button/link action.
  void RecordClick(ActionableItem item);

  Browser* browser() const { return browser_; }

  // Return maximal height for the view after which it becomes scrollable.
  // TODO(crbug.com/870303): remove when a general solution is available.
  int GetMaxHeight() const;

  views::Button* anchor_button() const { return anchor_button_; }

  bool perform_menu_actions() const { return perform_menu_actions_; }
  void set_perform_menu_actions_for_testing(bool perform_menu_actions) {
    perform_menu_actions_ = perform_menu_actions;
  }

 private:
  class AXMenuWidgetObserver;

  friend class ProfileMenuViewExtensionsTest;

  void Reset();
  void OnWindowClosing();

  // Requests focus for a button when opened by keyboard.
  void FocusButtonOnKeyboardOpen();

  // views::BubbleDialogDelegateView:
  void Init() final;
  void OnThemeChanged() override;
  ax::mojom::Role GetAccessibleWindowRole() override;

  // content::WebContentsDelegate:
  bool HandleContextMenu(content::RenderFrameHost* render_frame_host,
                         const content::ContextMenuParams& params) override;

  void ButtonPressed(base::RepeatingClosure action);

  void UpdateSyncInfoContainerBackground();

  Browser* const browser_;

  views::Button* const anchor_button_;

  // Component containers.
  views::View* heading_container_ = nullptr;
  views::View* identity_info_container_ = nullptr;
  views::View* sync_info_container_ = nullptr;
  views::View* shortcut_features_container_ = nullptr;
  views::View* features_container_ = nullptr;
  views::View* profile_mgmt_separator_container_ = nullptr;
  views::View* profile_mgmt_heading_container_ = nullptr;
  views::View* selectable_profiles_container_ = nullptr;
  views::View* profile_mgmt_shortcut_features_container_ = nullptr;
  views::View* profile_mgmt_features_container_ = nullptr;

  // The first profile button that should be focused when the menu is opened
  // using a key accelerator.
  views::Button* first_profile_button_ = nullptr;

  // May be disabled by tests that only watch to histogram records and don't
  // care about actual actions.
  bool perform_menu_actions_ = true;

  CloseBubbleOnTabActivationHelper close_bubble_helper_;

  SyncInfoContainerBackgroundState sync_background_state_ =
      SyncInfoContainerBackgroundState::kNoError;

  // Actual heading string would be set by children classes.
  std::u16string profile_mgmt_heading_;

  std::unique_ptr<AXMenuWidgetObserver> ax_widget_observer_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_MENU_VIEW_BASE_H_
