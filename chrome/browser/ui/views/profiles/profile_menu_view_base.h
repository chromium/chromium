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
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/close_bubble_on_tab_activation_helper.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/controls/styled_label_listener.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/style/typography.h"

class Browser;

namespace views {
class Button;
class Label;
}  // namespace views

struct AccountInfo;
class DiceSigninButtonView;

// This class provides the UI for different menus that are created by user
// clicking the avatar button.
class ProfileMenuViewBase : public content::WebContentsDelegate,
                            public views::BubbleDialogDelegateView,
                            public views::ButtonListener,
                            public views::StyledLabelListener,
                            public views::LinkListener {
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
    kLockButton = 6,
    kExitProfileButton = 7,
    kSyncErrorButton = 8,
    kCurrentProfileCard = 9,
    kSigninButton = 10,
    kSigninAccountButton = 11,
    kSignoutButton = 12,
    kOtherProfileButton = 13,
    kCookiesClearedOnExitLink = 14,
    kAddNewProfileButton = 15,
    kSyncSettingsButton = 16,
    kEditProfileButton = 17,
    kMaxValue = kEditProfileButton,
  };

  // MenuItems struct keeps the menu items and meta data for a group of items in
  // a menu. It takes the ownership of views and passes it to the menu when menu
  // is constructed.
  struct MenuItems {
    MenuItems();
    MenuItems(MenuItems&&);
    ~MenuItems();

    enum ItemType {
      kNone,
      kTitleCard,
      kLabel,
      kButton,
      kStyledButton,
      kGeneral
    };

    std::vector<std::unique_ptr<views::View>> items;

    ItemType first_item_type;
    ItemType last_item_type;
    bool different_item_types;

    DISALLOW_COPY_AND_ASSIGN(MenuItems);
  };

  enum GroupMarginSize { kNone, kTiny, kSmall, kLarge };

  // Shows the bubble if one is not already showing.  This allows us to easily
  // make a button toggle the bubble on and off when clicked: we unconditionally
  // call this function when the button is clicked and if the bubble isn't
  // showing it will appear while if it is showing, nothing will happen here and
  // the existing bubble will auto-close due to focus loss.
  static void ShowBubble(
      profiles::BubbleViewMode view_mode,
      signin_metrics::AccessPoint access_point,
      views::Button* anchor_button,
      Browser* browser,
      bool is_source_keyboard);

  static bool IsShowing();
  static void Hide();

  static ProfileMenuViewBase* GetBubbleForTesting();

  ProfileMenuViewBase(views::Button* anchor_button,
                      Browser* browser);
  ~ProfileMenuViewBase() override;

  // This method is called once to add all menu items.
  virtual void BuildMenu() = 0;

  // API to build the profile menu.
  void SetHeading(const base::string16& heading,
                  const base::string16& tooltip_text,
                  base::RepeatingClosure action);
  void SetIdentityInfo(const gfx::ImageSkia& image,
                       const gfx::ImageSkia& badge,
                       const base::string16& title,
                       const base::string16& subtitle = base::string16());
  void SetSyncInfo(const gfx::ImageSkia& icon,
                   const base::string16& description,
                   const base::string16& clickable_text,
                   base::RepeatingClosure action);
  void SetSyncInfoBackgroundColor(SkColor bg_color);
  void AddShortcutFeatureButton(const gfx::ImageSkia& icon,
                                const base::string16& text,
                                base::RepeatingClosure action);
  void AddFeatureButton(const gfx::ImageSkia& icon,
                        const base::string16& text,
                        base::RepeatingClosure action);
  void SetProfileManagementHeading(const base::string16& heading);
  void AddSelectableProfile(const gfx::ImageSkia& image,
                            const base::string16& name,
                            base::RepeatingClosure action);
  void AddProfileManagementShortcutFeatureButton(const gfx::ImageSkia& icon,
                                                 const base::string16& text,
                                                 base::RepeatingClosure action);
  void AddProfileManagementFeatureButton(const gfx::ImageSkia& icon,
                                         const base::string16& text,
                                         base::RepeatingClosure action);
  // 0 < |icon_to_image_ratio| <= 1 is the size ratio of |icon| in the returned
  // image. E.g. a value of 0.8 means that |icon| only takes up 80% of the
  // returned image, with the rest being padding around it.
  gfx::ImageSkia ImageForMenu(const gfx::VectorIcon& icon,
                              float icon_to_image_ratio = 1.0f);
  gfx::ImageSkia ColoredImageForMenu(const gfx::VectorIcon& icon,
                                     SkColor color);
  // Should be called inside each button/link action.
  void RecordClick(ActionableItem item);

  // Initializes a new group of menu items. A separator is added before them if
  // |add_separator| is true.
  void AddMenuGroup(bool add_separator = true);

  // The following functions add different menu items to the latest menu group.
  // They pass the ownership of the generated item to |menu_item_groups_| and
  // return a raw pointer to the object. The ownership is transferred to the
  // menu when view is repopulated from menu items.
  // Please use |AddViewItem| only if none of the previous ones match.
  views::Button* CreateAndAddButton(const gfx::ImageSkia& icon,
                                    const base::string16& title,
                                    base::RepeatingClosure action);
  views::Button* CreateAndAddBlueButton(const base::string16& text,
                                        bool md_style,
                                        base::RepeatingClosure action);
  // If |action| is null the card will be disabled.
  views::Button* CreateAndAddTitleCard(std::unique_ptr<views::View> icon_view,
                                       const base::string16& title,
                                       const base::string16& subtitle,
                                       base::RepeatingClosure action);
#if !defined(OS_CHROMEOS)
  DiceSigninButtonView* CreateAndAddDiceSigninButton(
      AccountInfo* account_info,
      gfx::Image* account_icon,
      base::RepeatingClosure action);
#endif
  views::Label* CreateAndAddLabel(
      const base::string16& text,
      int text_context = views::style::CONTEXT_LABEL);
  views::StyledLabel* CreateAndAddLabelWithLink(const base::string16& text,
                                                gfx::Range link_range,
                                                base::RepeatingClosure action);
  void AddViewItem(std::unique_ptr<views::View> view);

  Browser* browser() const { return browser_; }

  // Return maximal height for the view after which it becomes scrollable.
  // TODO(crbug.com/870303): remove when a general solution is available.
  int GetMaxHeight() const;

  views::Button* anchor_button() const { return anchor_button_; }

  gfx::ImageSkia CreateVectorIcon(const gfx::VectorIcon& icon);

  int GetDefaultIconSize();

 private:
  friend class ProfileMenuViewExtensionsTest;

  void Reset();
  void RepopulateViewFromMenuItems();

  // Requests focus for a button when opened by keyboard.
  virtual void FocusButtonOnKeyboardOpen() {}

  // views::BubbleDialogDelegateView:
  void Init() final;
  void WindowClosing() override;
  void OnThemeChanged() override;
  ax::mojom::Role GetAccessibleWindowRole() override;

  // content::WebContentsDelegate:
  bool HandleContextMenu(content::RenderFrameHost* render_frame_host,
                         const content::ContextMenuParams& params) override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* button, const ui::Event& event) final;

  // views::LinkListener:
  void LinkClicked(views::Link* link, int event_flags) final;

  // views::StyledLabelListener:
  void StyledLabelLinkClicked(views::StyledLabel* link,
                              const gfx::Range& range,
                              int event_flags) final;

  // Handles all click events.
  void OnClick(views::View* clickable_view);

  void RegisterClickAction(views::View* clickable_view,
                           base::RepeatingClosure action);

  // Returns the size of different margin types.
  int GetMarginSize(GroupMarginSize margin_size) const;

  void AddMenuItemInternal(std::unique_ptr<views::View> view,
                           MenuItems::ItemType item_type);

  Browser* const browser_;

  // ProfileMenuViewBase takes ownership of all menu_items and passes it to the
  // underlying view when it is created.
  std::vector<MenuItems> menu_item_groups_;

  views::Button* const anchor_button_;

  std::map<views::View*, base::RepeatingClosure> click_actions_;

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

  CloseBubbleOnTabActivationHelper close_bubble_helper_;

  DISALLOW_COPY_AND_ASSIGN(ProfileMenuViewBase);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_MENU_VIEW_BASE_H_
