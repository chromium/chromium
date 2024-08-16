// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LOCATION_BAR_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LOCATION_BAR_VIEW_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_context_menu_model.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/views/location_bar/content_setting_image_view.h"
#include "chrome/browser/ui/views/location_bar/location_icon_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/permissions/chip/chip_controller.h"
#include "chrome/browser/ui/views/permissions/chip/permission_dashboard_controller.h"
#include "components/permissions/permission_prompt.h"
#include "components/security_state/core/security_state.h"
#include "services/device/public/cpp/geolocation/buildflags.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/font.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image.h"
#include "ui/views/animation/animation_delegate_views.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/drag_controller.h"

#if BUILDFLAG(IS_MAC)
#include "components/webapps/common/web_app_id.h"
#endif

#if BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)
#include "services/device/public/cpp/geolocation/geolocation_system_permission_manager.h"
#endif  // BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)

class CommandUpdater;
class ContentSettingBubbleModelDelegate;
class IntentChipButton;
class LocationIconView;
enum class OmniboxPart;
class OmniboxPopupView;
class OmniboxViewViews;
class PageActionIconController;
class PageActionIconContainerView;
class PermissionDashboardView;
class Profile;
class SelectedKeywordView;

namespace views {
class ImageButton;
class Label;
}  // namespace views

/////////////////////////////////////////////////////////////////////////////
//
// LocationBarView class
//
//   The LocationBarView class is a View subclass that paints the background
//   of the URL bar strip and contains its content.
//
//   This class can be used outside the context of a normal browser window. See
//   PresentationReceiverWindowView and https://crbug.com/41351409 for details.
//
/////////////////////////////////////////////////////////////////////////////
class LocationBarView
    : public LocationBar,
      public LocationBarTesting,
      public views::View,
      public views::FocusChangeListener,
      public views::DragController,
      public views::AnimationDelegateViews,
      public IconLabelBubbleView::Delegate,
      public LocationIconView::Delegate,
      public ContentSettingImageView::Delegate,
#if BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)
      public device::GeolocationSystemPermissionManager::PermissionObserver,
#endif  // BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)
      public PageActionIconView::Delegate {
  METADATA_HEADER(LocationBarView, views::View)

 public:
  class Delegate {
   public:
    // Should return the current web contents.
    virtual content::WebContents* GetWebContents() = 0;

    virtual LocationBarModel* GetLocationBarModel() = 0;
    virtual const LocationBarModel* GetLocationBarModel() const = 0;

    // Returns ContentSettingBubbleModelDelegate.
    virtual ContentSettingBubbleModelDelegate*
    GetContentSettingBubbleModelDelegate() = 0;

   protected:
    virtual ~Delegate() {}
  };

  LocationBarView(Browser* browser,
                  Profile* profile,
                  CommandUpdater* command_updater,
                  Delegate* delegate,
                  bool is_popup_mode);
  LocationBarView(const LocationBarView&) = delete;
  LocationBarView& operator=(const LocationBarView&) = delete;
  ~LocationBarView() override;

  // Returns the location bar border radius in DIPs.
  int GetBorderRadius() const;

  // Initializes the LocationBarView.
  void Init();

  // True if this instance has been initialized by calling Init, which can only
  // be called when the receiving instance is attached to a view container.
  bool IsInitialized() const;

  // Returns a background that paints an (optionally stroked) rounded rect with
  // the given color.
  std::unique_ptr<views::Background> CreateRoundRectBackground(
      SkColor background_color,
      SkColor stroke_color,
      SkBlendMode blend_mode = SkBlendMode::kSrcOver,
      bool antialias = true,
      bool should_border_scale = false) const;

  // Returns the delegate.
  Delegate* delegate() const { return delegate_; }

  PageActionIconController* page_action_icon_controller() {
    return page_action_icon_controller_;
  }

  // Returns the screen coordinates of the omnibox (where the URL text appears,
  // not where the icons are shown).
  gfx::Point GetOmniboxViewOrigin() const;

  // Shows |text| as an inline autocompletion.  This is useful for IMEs, where
  // we can't show the autocompletion inside the actual OmniboxView.  See
  // comments on |ime_inline_autocomplete_view_|.
  void SetImePrefixAutocompletion(const std::u16string& text);
  std::u16string GetImePrefixAutocompletion() const;
  void SetImeInlineAutocompletion(const std::u16string& text);
  std::u16string GetImeInlineAutocompletion() const;

  // Sets the additional omnibox text. E.g. the title corresponding to the URL
  // displayed in the OmniboxView.
  void SetOmniboxAdditionalText(const std::u16string& text);
  std::u16string GetOmniboxAdditionalText() const;

  // Select all of the text. Needed when the user tabs through controls
  // in the toolbar in full keyboard accessibility mode.
  virtual void SelectAll();

  LocationIconView* location_icon_view() { return location_icon_view_; }

  SelectedKeywordView* selected_keyword_view() {
    return selected_keyword_view_;
  }

  OmniboxViewViews* omnibox_view() { return omnibox_view_; }

  // Returns true if the location bar's current security state does not match
  // the currently visible state.
  bool HasSecurityStateChanged();

  // Updates the controller, and, if |contents| is non-null, restores saved
  // state that the tab holds.
  void Update(content::WebContents* contents);

  // Clears the location bar's state for |contents|.
  void ResetTabState(content::WebContents* contents);

  // Activates the first visible but inactive PageActionIconView for
  // accessibility.
  bool ActivateFirstInactiveBubbleForAccessibility();

  // Controls the chip in the LocationBarView.
  ChipController* GetChipController();

  // Controls the permission dashboard in the LocationBarView.
  PermissionDashboardController* permission_dashboard_controller() {
    return permission_dashboard_controller_.get();
  }

  IntentChipButton* intent_chip() { return intent_chip_; }

  // LocationBar:
  void FocusLocation(bool is_user_initiated) override;
  void Revert() override;
  OmniboxView* GetOmniboxView() override;
  void UpdateWithoutTabRestore() override;
  LocationBarModel* GetLocationBarModel() override;
  content::WebContents* GetWebContents() override;

  // views::View:
  void AddedToWidget() override;
  void RemovedFromWidget() override;
  bool HasFocus() const override;
  gfx::Size GetMinimumSize() const override;
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void Layout(PassKey) override;
  void OnThemeChanged() override;
  void ChildPreferredSizeChanged(views::View* child) override;

  // views::FocusChangeListener:
  void OnWillChangeFocus(views::View* before, views::View* now) override;
  void OnDidChangeFocus(views::View* before, views::View* now) override;

  // IconLabelBubbleView::Delegate:
  SkColor GetIconLabelBubbleSurroundingForegroundColor() const override;
  SkAlpha GetIconLabelBubbleSeparatorAlpha() const override;
  SkColor GetIconLabelBubbleBackgroundColor() const override;

  // ContentSettingImageView::Delegate:
  bool ShouldHideContentSettingImage() override;
  content::WebContents* GetContentSettingWebContents() override;
  ContentSettingBubbleModelDelegate* GetContentSettingBubbleModelDelegate()
      override;

#if BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)
  // GeolocationSystemPermissionManager::PermissionObserver:
  void OnSystemPermissionUpdated(
      device::LocationSystemPermissionStatus new_status) override;
#endif  // BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)

  static bool IsVirtualKeyboardVisible(views::Widget* widget);

  // Returns the height available for user-entered text in the location bar.
  static int GetAvailableTextHeight();

  // Returns the height available for text within location bar decorations.
  static int GetAvailableDecorationTextHeight();

  void OnOmniboxFocused();
  void OnOmniboxBlurred();

  // Called when omnibox view receives mouse notifications relevant to hover.
  // |is_hovering| should be true when mouse is in omnibox; false when exited.
  void OnOmniboxHovered(bool is_hovering);

  Browser* browser() { return browser_; }
  Profile* profile() { return profile_; }

  // LocationIconView::Delegate:
  const LocationBarModel* GetLocationBarModel() const override;
  bool IsEditingOrEmpty() const override;
  void OnLocationIconPressed(const ui::MouseEvent& event) override;
  void OnLocationIconDragged(const ui::MouseEvent& event) override;
  bool ShowPageInfoDialog() override;
  SkColor GetSecurityChipColor(
      security_state::SecurityLevel security_level) const override;
  ui::ImageModel GetLocationIcon(LocationIconView::Delegate::IconFetchedCallback
                                     on_icon_fetched) const override;
  std::vector<raw_ptr<ContentSettingImageView, VectorExperimental>>&
  GetContentSettingViewsForTest() {
    return content_setting_views_;
  }

  void RecordPageInfoMetrics();

  void ResetConfirmationChipShownTime() {
    confirmation_chip_collapsed_time_ = base::TimeTicks::Now();
  }

  void SetConfirmationChipShownTimeForTesting(base::TimeTicks time) {
    confirmation_chip_collapsed_time_ = time;
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(SecurityIndicatorTest, CheckIndicatorText);
  FRIEND_TEST_ALL_PREFIXES(TouchLocationBarViewBrowserTest,
                           OmniboxViewViewsSize);
  FRIEND_TEST_ALL_PREFIXES(TouchLocationBarViewBrowserTest,
                           IMEInlineAutocompletePosition);
  using ContentSettingViews =
      std::vector<raw_ptr<ContentSettingImageView, VectorExperimental>>;

  // Returns the amount of space required to the left of the omnibox text.
  int GetMinimumLeadingWidth() const;

  // Returns the amount of space required to the right of the omnibox text.
  int GetMinimumTrailingWidth() const;

  // The LocationBarView bounds, without the ends which have a border radius.
  // E.g., if the LocationBarView was 50dip long, and the border radius was 2,
  // this method would return a gfx::Rect with 46dip width.
  gfx::Rect GetLocalBoundsWithoutEndcaps() const;

  // Updates the background on a theme change, or dropdown state change.
  void RefreshBackground();

  // Updates the visibility state of the Content Blocked icons to reflect what
  // is actually blocked on the current page. Returns true if the visibility
  // of at least one of the views in |content_setting_views_| changed.
  bool RefreshContentSettingViews();

  // Updates the visibility state of the PageActionIconViews to reflect what
  // actions are available on the current page.
  void RefreshPageActionIconViews();

  // Updates the color of the icon for the "clear all" button.
  void RefreshClearAllButtonIcon();

  // Returns true if a keyword is selected in the model.
  bool ShouldShowKeywordBubble() const;

  // Gets the OmniboxPopupView associated with the model in |omnibox_view_|.
  OmniboxPopupView* GetOmniboxPopupView();
  const OmniboxPopupView* GetOmniboxPopupView() const;

  // Called when the page info bubble is closed.
  void OnPageInfoBubbleClosed(views::Widget::ClosedReason closed_reason,
                              bool reload_prompt);

  // Helper to set the texts of labels adjacent to the omnibox:
  // `ime_prefix_autocomplete_view_`, `ime_inline_autocomplete_view_`, and
  // `omnibox_additional_text_view_`.
  void SetOmniboxAdjacentText(views::Label* label, const std::u16string& text);

  // LocationBar:
  void FocusSearch() override;
  void UpdateContentSettingsIcons() override;
  void SaveStateToContents(content::WebContents* contents) override;
  LocationBarTesting* GetLocationBarForTesting() override;
  void OnChanged() override;
  void OnPopupVisibilityChanged() override;

  // LocationBarTesting:
  bool TestContentSettingImagePressed(size_t index) override;
  bool IsContentSettingBubbleShowing(size_t index) override;

  // views::View:
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  bool GetNeedsNotificationWhenVisibleBoundsChange() const override;
  void OnVisibleBoundsChanged() override;
  void OnFocus() override;
  void OnPaintBorder(gfx::Canvas* canvas) override;
  // LocationBarView directs mouse button events from
  // |omnibox_additional_text_view_| to |omnibox_view_| so that e.g., clicking
  // the former will focus the latter.
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnMouseMoved(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void ShowContextMenu(const gfx::Point& p,
                       ui::MenuSourceType source_type) override;

  // views::DragController:
  void WriteDragDataForView(View* sender,
                            const gfx::Point& press_pt,
                            OSExchangeData* data) override;
  int GetDragOperationsForView(View* sender, const gfx::Point& p) override;
  bool CanStartDragForView(View* sender,
                           const gfx::Point& press_pt,
                           const gfx::Point& p) override;

  // PageActionIconView::Delegate:
  content::WebContents* GetWebContentsForPageActionIconView() override;
  bool ShouldHidePageActionIcons() const override;
  bool ShouldHidePageActionIcon(PageActionIconView* icon_view) const override;

  // views::AnimationDelegateViews:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationCanceled(const gfx::Animation* animation) override;
  void OnChildViewRemoved(View* observed_view, View* child) override;

  void OnTouchUiChanged();

  // Determines whether the location icon should be overridden while a chip is
  // being displayed
  bool ShouldChipOverrideLocationIcon();

  // Called with an async fetched for the keyword view.
  void OnKeywordFaviconFetched(const gfx::Image& icon);

  // Updates the visibility of the QR Code Generator icon.
  void UpdateQRCodeGeneratorIcon();

  // Updates the visibility of the send tab to self icon.
  // Returns true if visibility changed.
  bool UpdateSendTabToSelfIcon();

  // Updates the visibility of the permission chip when omnibox is in the edit
  // mode.
  void UpdateChipVisibility();

  // Adjusts |event|'s location to be relative to |omnibox_view_|'s origin; used
  // for directing LocationBarView events to the |omnibox_view_|.
  ui::MouseEvent AdjustMouseEventLocationForOmniboxView(
      const ui::MouseEvent& event) const;

  bool GetPopupMode() const;

#if BUILDFLAG(IS_MAC)
  // Called when app shims change.
  void OnAppShimChanged(const webapps::AppId& app_id);

  // Manage a subscription to AppShimRegistry; used to monitor for changes
  // to system level notification permissions.
  base::CallbackListSubscription app_shim_observation_;
#endif

#if BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)
  // Manage a subscription to GeolocationSystemPermissionManager, which may
  // outlive this object.
  base::ScopedObservation<
      device::GeolocationSystemPermissionManager,
      device::GeolocationSystemPermissionManager::PermissionObserver>
      geolocation_permission_observation_{this};
#endif  // BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)

  // The Browser this LocationBarView is in.  Note that at least
  // ash::SimpleWebViewDialog uses a LocationBarView outside any browser
  // window, so this may be NULL.
  const raw_ptr<Browser> browser_;

  // May be nullptr in tests.
  const raw_ptr<Profile> profile_;

  // The omnibox view where the user types and the current page URL is displayed
  // when user input is not in progress.
  raw_ptr<OmniboxViewViews> omnibox_view_ = nullptr;

  // Our delegate.
  raw_ptr<Delegate> delegate_;

  // A controller for a view that contains a chip button which is used for
  // permission information and requests.
  std::unique_ptr<ChipController> chip_controller_ = nullptr;

  std::unique_ptr<PermissionDashboardController>
      permission_dashboard_controller_;
  raw_ptr<PermissionDashboardView> permission_dashboard_view_;

  // An icon to the left of the edit field: the HTTPS lock, blank page icon,
  // search icon, EV HTTPS bubble, etc.
  raw_ptr<LocationIconView> location_icon_view_ = nullptr;

  // Views to show inline autocompletion when an IME is active.  In this case,
  // we shouldn't change the text or selection inside the OmniboxView itself,
  // since this will conflict with the IME's control over the text.  So instead
  // we show any autocompletion in a separate field after the OmniboxView.
  raw_ptr<views::Label> ime_prefix_autocomplete_view_ = nullptr;
  raw_ptr<views::Label> ime_inline_autocomplete_view_ = nullptr;

  // The complementary omnibox label displaying the selected suggestion's title
  // or URL when the omnibox view is displaying the other and when user input is
  // in progress. Will remain a nullptr if the rich autocompletion
  // feature flag is disabled.
  raw_ptr<views::Label> omnibox_additional_text_view_ = nullptr;

  // The following views are used to provide hints and remind the user as to
  // what is going in the edit. They are all added a children of the
  // LocationBarView. At most one is visible at a time. These autocollapse when
  // the edit needs the room.

  // Shown if the user has selected a keyword.
  raw_ptr<SelectedKeywordView> selected_keyword_view_ = nullptr;

  raw_ptr<IntentChipButton> intent_chip_ = nullptr;

  // The content setting views.
  ContentSettingViews content_setting_views_;

  // The controller for page action icons.
  raw_ptr<PageActionIconController> page_action_icon_controller_ = nullptr;

  // The container for page action icons.
  raw_ptr<PageActionIconContainerView> page_action_icon_container_ = nullptr;

  // An [x] that appears in touch mode (when the OSK is visible) and allows the
  // user to clear all text.
  raw_ptr<views::ImageButton> clear_all_button_ = nullptr;

  // Animation to change whole location bar background color on hover.
  gfx::SlideAnimation hover_animation_{this};

  // Whether we're in popup mode (i.e. location bar is situated within a popup
  // window). This value also controls whether the location bar is read-only.
  const bool is_popup_mode_;

  bool is_initialized_ = false;

  // Used for metrics collection.
  base::TimeTicks confirmation_chip_collapsed_time_ = base::TimeTicks();

  // The focus manager associated with this view. The focus manager is expected
  // to outlive this view.
  raw_ptr<views::FocusManager> focus_manager_ = nullptr;

  base::CallbackListSubscription subscription_ =
      ui::TouchUiController::Get()->RegisterCallback(
          base::BindRepeating(&LocationBarView::OnTouchUiChanged,
                              base::Unretained(this)));

  base::WeakPtrFactory<LocationBarView> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LOCATION_BAR_VIEW_H_
