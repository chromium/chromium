// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_MENU_VIEW_BASE_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_MENU_VIEW_BASE_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/close_bubble_on_tab_activation_helper.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/style/typography.h"

class Browser;

namespace views {
class Button;
}  // namespace views

namespace ui {
class ColorProvider;
}  // namespace ui

// This class provides the UI for different menus that are created by user
// clicking the avatar button.
class ProfileMenuViewBase : public content::WebContentsDelegate,
                            public views::BubbleDialogDelegateView {
  METADATA_HEADER(ProfileMenuViewBase, views::BubbleDialogDelegateView)

 public:
  // Enumeration of all actionable items in the profile menu.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // LINT.IfChange(ActionableItem)
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
    // DEPRECATED: kCreateIncognitoShortcutButton = 18,
    kEnableSyncForWebOnlyAccountButton = 19,
    kProfileManagementLabel = 20,
    kSigninReauthButton = 21,
    kAutofillSettingsButton = 22,
    kHistorySyncOptInButton = 23,
    kMaxValue = kHistorySyncOptInButton,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/profile/enums.xml:ProfileMenuActionableItem)

  // Parameters for `SetProfileIdentityWithCallToAction()`
  struct IdentitySectionParams {
    IdentitySectionParams();
    ~IdentitySectionParams();

    IdentitySectionParams(const IdentitySectionParams&) = delete;
    IdentitySectionParams& operator=(const IdentitySectionParams&) = delete;

    IdentitySectionParams(IdentitySectionParams&&);
    IdentitySectionParams& operator=(IdentitySectionParams&&);

    // Optional header displayed at the top (e.g. for management notice).
    // `header_string` and `header_image` must both be non-empty for the header
    // to be shown.
    std::u16string header_string;
    ui::ImageModel header_image;
    base::RepeatingClosure header_action;

    // `profile_image` must not be empty. It does not need to be circular.
    ui::ImageModel profile_image;
    bool has_dotted_ring = false;
    // This padding does not make the avatar larger in the menu.
    // `profile_image` is drawn smaller to leave space around for the padding.
    int profile_image_padding = 0;

    // Must not be empty.
    std::u16string title;

    // If `subtitle` is empty, no subtitle is shown (see disclaimer below).
    std::u16string subtitle;

    // If `button_text` is empty, no button is shown.
    // Disclaimer: This function does not support showing a button with no
    // subtitle. If the `subtitle` is empty then `button_text` must be empty.
    std::u16string button_text;

    // If `button_image` is empty, the button has no image.
    ui::ImageModel button_image;

    // Must be valid if there is a button.
    base::RepeatingClosure button_action;
  };

  // Size of the large identity image in the Sync info section (deprecated).
  static constexpr int kIdentityImageSize = 64;
  // Size of the large identity image in the identity info section.
  static constexpr int kIdentityInfoImageSize = 56;
  // Size of the badge shown with the identity image when the profile is
  // managed. This can be the business icon or a logo set by the
  // `EnterpriseLogoUrl` policy.
  static constexpr int kManagementBadgeSize = 24;
  // Size of the small identity image shown inside the signin button.
  static constexpr int kIdentityImageSizeForButton = 22;
  // Size of the profile image in the "Other profiles" section, matches the
  // icon size of other rows.
  static constexpr int kOtherProfileImageSize = 16;

  ProfileMenuViewBase(views::Button* anchor_button, Browser* browser);
  ~ProfileMenuViewBase() override;

  ProfileMenuViewBase(const ProfileMenuViewBase&) = delete;
  ProfileMenuViewBase& operator=(const ProfileMenuViewBase&) = delete;

  // This method is called once to add all menu items.
  virtual void BuildMenu() = 0;

  // If |profile_name| is empty, no heading will be displayed.
  // `management_badge` and `image_model` do not need to be circular.
  void SetProfileIdentityInfo(const ui::ImageModel& image_model,
                              const std::u16string& title,
                              const std::u16string& subtitle = std::u16string(),
                              const gfx::VectorIcon* header_art_icon = nullptr);

  // See `IdentitySectionParams` for documentation of the parameters.
  void SetProfileIdentityWithCallToAction(IdentitySectionParams params);

  void AddFeatureButton(
      const std::u16string& text,
      base::RepeatingClosure action,
      const gfx::VectorIcon& icon = gfx::VectorIcon::EmptyIcon(),
      float icon_to_image_ratio = 1.0f,
      std::optional<ui::ColorId> background_color = std::nullopt,
      bool add_vertical_margin = false);
  void SetProfileManagementHeading(const std::u16string& heading);
  void AddAvailableProfile(const ui::ImageModel& image_model,
                           const std::u16string& name,
                           bool is_guest,
                           base::RepeatingClosure action);
  void AddProfileManagementFeaturesSeparator();
  void AddProfileManagementFeatureButton(const gfx::VectorIcon& icon,
                                         const std::u16string& text,
                                         base::RepeatingClosure action);

  void AddBottomMargin();

  // Should be called inside each button/link action.
  void RecordClick(ActionableItem item);

  Browser* browser() const { return browser_; }

  // Return maximal height for the view after which it becomes scrollable.
  // TODO(crbug.com/40587757): remove when a general solution is available.
  int GetMaxHeight() const;

  views::Button* anchor_button() const { return anchor_button_; }

  bool perform_menu_actions() const { return perform_menu_actions_; }
  void set_perform_menu_actions_for_testing(bool perform_menu_actions) {
    perform_menu_actions_ = perform_menu_actions;
  }

 private:
  class AXMenuWidgetObserver;

  friend class ProfileMenuCoordinator;
  friend class ProfileMenuViewExtensionsTest;

  void Reset();
  void OnWindowClosing();

  // Requests focus for the first profile in the 'Other profiles' section (if it
  // exists).
  void FocusFirstProfileButton();

  void BuildIdentityInfoColorCallback(const ui::ColorProvider* color_provider);

  void BuildProfileBackgroundContainer(
      std::unique_ptr<views::View> avatar_image_view,
      const ui::ThemedVectorIcon& avatar_header_art);

  // views::BubbleDialogDelegateView:
  void Init() final;
  void OnThemeChanged() override;

  // content::WebContentsDelegate:
  bool HandleContextMenu(content::RenderFrameHost& render_frame_host,
                         const content::ContextMenuParams& params) override;

  void ButtonPressed(base::RepeatingClosure action);

  void CreateAXWidgetObserver(views::Widget* widget);

  std::unique_ptr<HoverButton> CreateMenuRowButton(
      base::RepeatingClosure action,
      std::unique_ptr<views::View> icon_view,
      const std::u16string& text);

  const raw_ptr<Browser> browser_;

  const raw_ptr<views::Button> anchor_button_;

  // Component containers.
  raw_ptr<views::View> identity_info_container_ = nullptr;
  raw_ptr<views::View> features_container_ = nullptr;
  raw_ptr<views::View> profile_mgmt_separator_container_ = nullptr;
  raw_ptr<views::View> profile_mgmt_heading_container_ = nullptr;
  raw_ptr<views::View> selectable_profiles_container_ = nullptr;
  raw_ptr<views::View> profile_mgmt_features_separator_container_ = nullptr;
  raw_ptr<views::View> profile_mgmt_features_container_ = nullptr;

  // Child components of `identity_info_container_`.
  raw_ptr<views::FlexLayoutView> profile_background_container_ = nullptr;
  raw_ptr<views::Label> title_label_ = nullptr;
  raw_ptr<views::Label> subtitle_label_ = nullptr;

  // The first profile button that should be focused when the menu is opened
  // using a key accelerator.
  raw_ptr<views::Button> first_profile_button_ = nullptr;

  // May be disabled by tests that only watch to histogram records and don't
  // care about actual actions.
  bool perform_menu_actions_ = true;

  CloseBubbleOnTabActivationHelper close_bubble_helper_;

  // Builds the colors for `profile_background_container_` and `heading_label_`
  // in `identity_info_container_`. This requires ui::ColorProvider, which is
  // only available once OnThemeChanged() is called, so the class caches this
  // callback and calls it afterwards.
  base::RepeatingCallback<void(const ui::ColorProvider*)>
      identity_info_color_callback_ = base::DoNothing();

  // Builds the background for |sync_info_container_|. This requires
  // ui::ColorProvider, which is only available once OnThemeChanged() is called,
  // so the class caches this callback and calls it afterwards.
  base::RepeatingCallback<void(const ui::ColorProvider*)>
      sync_info_background_callback_ = base::DoNothing();

  // Actual heading string would be set by children classes.
  std::u16string profile_mgmt_heading_;

  std::unique_ptr<AXMenuWidgetObserver> ax_widget_observer_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_MENU_VIEW_BASE_H_
