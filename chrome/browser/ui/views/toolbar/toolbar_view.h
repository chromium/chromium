// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_VIEW_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/command_observer.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/toolbar/app_menu_icon_controller.h"
#include "chrome/browser/ui/toolbar/back_forward_menu_model.h"
#include "chrome/browser/ui/toolbar/chrome_labs/chrome_labs_model.h"
#include "chrome/browser/ui/views/frame/browser_root_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/intent_picker_bubble_view.h"
#include "chrome/browser/ui/views/location_bar/custom_tab_bar_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/overflow_button.h"
#include "components/prefs/pref_member.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/animation/animation_delegate_views.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/view.h"
#include "url/origin.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/components/arc/mojom/intent_helper.mojom-forward.h"  // nogncheck https://crbug.com/784179
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

class AppMenuButton;
class AvatarToolbarButton;
class BatterySaverButton;
class BrowserAppMenuButton;
class Browser;
class DownloadToolbarButtonView;
class ExtensionsToolbarButton;
class ExtensionsToolbarContainer;
class ChromeLabsButton;
class HomeButton;
class IntentChipButton;
class ExtensionsToolbarCoordinator;
class ManagementToolbarButton;
class MediaToolbarButtonView;
class ReloadButton;
class PinnedToolbarActionsContainer;
class ToolbarButton;
class AvatarToolbarButtonBrowserTest;
class ToolbarController;
class OverflowButton;
class PerformanceInterventionButton;

namespace media_router {
class CastToolbarButton;
}

namespace send_tab_to_self {
class SendTabToSelfToolbarIconView;
}

namespace views {
class FlexLayout;
}

// The Browser Window's toolbar.
class ToolbarView : public views::AccessiblePaneView,
                    public ui::AcceleratorProvider,
                    public views::AnimationDelegateViews,
                    public LocationBarView::Delegate,
                    public CommandObserver,
                    public AppMenuIconController::Delegate,
                    public ToolbarButtonProvider,
                    public BrowserRootView::DropTarget {
  METADATA_HEADER(ToolbarView, views::AccessiblePaneView)

 public:
  // Types of display mode this toolbar can have.
  enum class DisplayMode {
    NORMAL,     // Normal toolbar with buttons, etc.
    LOCATION,   // Slimline toolbar showing only compact location
                // bar, used for popups.
    CUSTOM_TAB  // Custom tab bar, used in PWAs when a location
                // needs to be displayed.
  };

  ToolbarView(Browser* browser, BrowserView* browser_view);
  ToolbarView(const ToolbarView&) = delete;
  ToolbarView& operator=(const ToolbarView&) = delete;
  ~ToolbarView() override;

  // Create the contents of the Browser Toolbar.
  void Init();

  // Forces the toolbar (and transitively the location bar) to update its
  // current state.  If |tab| is non-NULL, we're switching (back?) to this tab
  // and should restore any previous location bar state (such as user editing)
  // as well.
  void Update(content::WebContents* tab);

  // Updates the toolbar's visible security state if the state has changed
  // since the last update. Returns true if the toolbar was updated.
  bool UpdateSecurityState();

  // Updates the visibility of the custom tab bar, potentially animating the
  // transition.
  void UpdateCustomTabBarVisibility(bool visible, bool animate);

  // We may or may not be using a WebUI tab strip. Make sure toolbar items are
  // added or removed accordingly.
  void UpdateForWebUITabStrip();

  // Clears the current state for |tab|.
  void ResetTabState(content::WebContents* tab);

  // Set focus to the toolbar with complete keyboard access, with the
  // focus initially set to the app menu. Focus will be restored
  // to the last focused view if the user escapes.
  void SetPaneFocusAndFocusAppMenu();

  // Returns true if the app menu is focused.
  bool GetAppMenuFocused() const;

  void ShowIntentPickerBubble(
      std::vector<IntentPickerBubbleView::AppInfo> app_info,
      bool show_stay_in_chrome,
      bool show_remember_selection,
      IntentPickerBubbleView::BubbleType bubble_type,
      const std::optional<url::Origin>& initiating_origin,
      IntentPickerResponse callback);

  // Shows a bookmark bubble and anchors it appropriately.
  void ShowBookmarkBubble(const GURL& url, bool already_bookmarked);

  // Accessors.
  Browser* browser() const { return browser_; }
  views::Button* GetChromeLabsButton() const;

  // NOTE: Use of the above method `GetChromeLabsButton` is preferred while the
  // Chrome Labs button is migrated to PinnedActionToolbarButton.
  // TODO(b/353385180): Remove once Chrome Labs button migration is complete.
  ChromeLabsButton* chrome_labs_button() const { return chrome_labs_button_; }
  ChromeLabsModel* chrome_labs_model() const {
    return chrome_labs_model_.get();
  }
  DownloadToolbarButtonView* download_button() const {
    return download_button_;
  }
  ExtensionsToolbarContainer* extensions_container() const {
    return extensions_container_;
  }
  ToolbarButton* forward_button() const { return forward_; }
  ExtensionsToolbarButton* GetExtensionsButton() const;
  ReloadButton* reload_button() const { return reload_; }
  LocationBarView* location_bar() const { return location_bar_; }
  CustomTabBarView* custom_tab_bar() { return custom_tab_bar_; }
  BatterySaverButton* battery_saver_button() const {
    return battery_saver_button_;
  }
  PerformanceInterventionButton* performance_intervention_button() const {
    return performance_intervention_button_;
  }
  ToolbarButton* GetCastButton() const;
  PinnedToolbarActionsContainer* pinned_toolbar_actions_container() const {
    return pinned_toolbar_actions_container_;
  }
  MediaToolbarButtonView* media_button() const { return media_button_; }
  send_tab_to_self::SendTabToSelfToolbarIconView* send_tab_to_self_button()
      const {
    return send_tab_to_self_button_;
  }
  BrowserAppMenuButton* app_menu_button() const { return app_menu_button_; }
  HomeButton* home_button() const { return home_; }
  AppMenuIconController* app_menu_icon_controller() {
    return &app_menu_icon_controller_;
  }
  const ToolbarController* toolbar_controller() const {
    return toolbar_controller_.get();
  }

  views::View* new_tab_button_for_testing() { return new_tab_button_; }

  ManagementToolbarButton* management_toolbar_button() const {
    return management_toolbar_button_;
  }

  // LocationBarView::Delegate:
  content::WebContents* GetWebContents() override;
  LocationBarModel* GetLocationBarModel() override;
  const LocationBarModel* GetLocationBarModel() const override;
  ContentSettingBubbleModelDelegate* GetContentSettingBubbleModelDelegate()
      override;

  // CommandObserver:
  void EnabledStateChangedForCommand(int id, bool enabled) override;

  // ui::AcceleratorProvider:
  bool GetAcceleratorForCommandId(int command_id,
                                  ui::Accelerator* accelerator) const override;

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  gfx::Size GetMinimumSize() const override;
  void Layout(PassKey) override;
  void OnThemeChanged() override;
  bool AcceleratorPressed(const ui::Accelerator& acc) override;
  void ChildPreferredSizeChanged(views::View* child) override;

  friend class AvatarToolbarButtonBrowserTest;

 protected:
  // This controls Toolbar, LocationBar and CustomTabBar visibility.
  // If we don't set all three, tab navigation from the app menu breaks
  // on Chrome OS.
  void SetToolbarVisibility(bool visible);

 private:
  // Forwards view overrides to this class.
  class ContainerView;

  // AccessiblePaneView:
  views::View* GetDefaultFocusableChild() override;

  // AnimationDelegateViews:
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationProgressed(const gfx::Animation* animation) override;

  // Declarative layout for child controls.
  void InitLayout();

  // Logic that must be done on initialization and then on layout.
  void LayoutCommon();

  // AppMenuIconController::Delegate:
  void UpdateTypeAndSeverity(
      AppMenuIconController::TypeAndSeverity type_and_severity) override;

  // ToolbarButtonProvider:
  ExtensionsToolbarContainer* GetExtensionsToolbarContainer() override;
  gfx::Size GetToolbarButtonSize() const override;
  views::View* GetDefaultExtensionDialogAnchorView() override;
  PageActionIconView* GetPageActionIconView(PageActionIconType type) override;
  AppMenuButton* GetAppMenuButton() override;
  gfx::Rect GetFindBarBoundingBox(int contents_bottom) override;
  void FocusToolbar() override;
  views::AccessiblePaneView* GetAsAccessiblePaneView() override;
  views::View* GetAnchorView(std::optional<PageActionIconType> type) override;
  void ZoomChangedForActiveTab(bool can_show_bubble) override;
  AvatarToolbarButton* GetAvatarToolbarButton() override;
  ToolbarButton* GetBackButton() override;
  ReloadButton* GetReloadButton() override;
  IntentChipButton* GetIntentChipButton() override;
  DownloadToolbarButtonView* GetDownloadButton() override;

  // BrowserRootView::DropTarget
  std::optional<BrowserRootView::DropIndex> GetDropIndex(
      const ui::DropTargetEvent& event) override;
  BrowserRootView::DropTarget* GetDropTarget(
      gfx::Point loc_in_local_coords) override;
  views::View* GetViewForDrop() override;

  // Changes the visibility of the Chrome Labs entry point based on prefs.
  void OnChromeLabsPrefChanged();

  // Loads the images for all the child views.
  void LoadImages();

  void OnShowForwardButtonChanged();

  void OnShowHomeButtonChanged();

  void OnTouchUiChanged();

  void UpdateClipPath();

  // Called when active state for the window changes.
  void ActiveStateChanged();

  void NewTabButtonPressed(const ui::Event& event);

  gfx::SlideAnimation size_animation_{this};

  // Controls. Most of these can be null, e.g. in popup windows. Only
  // |location_bar_| is guaranteed to exist. These pointers are owned by the
  // view hierarchy.
  raw_ptr<ToolbarButton> back_ = nullptr;
  raw_ptr<ToolbarButton> forward_ = nullptr;
  raw_ptr<ReloadButton> reload_ = nullptr;
  raw_ptr<HomeButton> home_ = nullptr;
  raw_ptr<CustomTabBarView> custom_tab_bar_ = nullptr;
  raw_ptr<LocationBarView> location_bar_ = nullptr;
  raw_ptr<ExtensionsToolbarContainer> extensions_container_ = nullptr;
  raw_ptr<views::View> toolbar_divider_ = nullptr;
  raw_ptr<ChromeLabsButton> chrome_labs_button_ = nullptr;
  raw_ptr<BatterySaverButton> battery_saver_button_ = nullptr;
  raw_ptr<PerformanceInterventionButton> performance_intervention_button_ =
      nullptr;
  raw_ptr<media_router::CastToolbarButton> cast_ = nullptr;
  raw_ptr<PinnedToolbarActionsContainer> pinned_toolbar_actions_container_ =
      nullptr;
  raw_ptr<AvatarToolbarButton> avatar_ = nullptr;
  raw_ptr<ManagementToolbarButton> management_toolbar_button_ = nullptr;
  raw_ptr<MediaToolbarButtonView> media_button_ = nullptr;
  raw_ptr<send_tab_to_self::SendTabToSelfToolbarIconView>
      send_tab_to_self_button_ = nullptr;
  raw_ptr<BrowserAppMenuButton> app_menu_button_ = nullptr;
  raw_ptr<DownloadToolbarButtonView> download_button_ = nullptr;
  raw_ptr<views::View> new_tab_button_ = nullptr;

  const raw_ptr<Browser> browser_;
  const raw_ptr<BrowserView> browser_view_;

  raw_ptr<views::FlexLayout> layout_manager_ = nullptr;

  AppMenuIconController app_menu_icon_controller_;

  std::unique_ptr<ChromeLabsModel> chrome_labs_model_;
  std::unique_ptr<ExtensionsToolbarCoordinator> extensions_toolbar_coordinator_;

  BooleanPrefMember show_forward_button_;

  // Controls whether or not a home button should be shown on the toolbar.
  BooleanPrefMember show_home_button_;

  BooleanPrefMember show_chrome_labs_button_;

  // The display mode used when laying out the toolbar.
  const DisplayMode display_mode_;

  std::unique_ptr<ToolbarController> toolbar_controller_;

  base::CallbackListSubscription subscription_ =
      ui::TouchUiController::Get()->RegisterCallback(
          base::BindRepeating(&ToolbarView::OnTouchUiChanged,
                              base::Unretained(this)));

  // Whether this toolbar has been initialized.
  bool initialized_ = false;

  // container_view_ is transparent with the same dimensions as ToolbarView.
  // All children are added to container_view_ and layout_manager_ applies to
  // container_view_. The reason for this layer of indiretion is because
  // container_view_ has a clip path set in UpdateClipPath() which adds rounded
  // corners. This leaves some unpainted pixels, which are painted by
  // background_view_left_ and background_view_right_.
  // the future.
  raw_ptr<ContainerView> container_view_ = nullptr;

  // A chevron button that indicates some toolbar elements have overflowed
  // due to small toolbar view width. Visibility controlled by
  // `toolbar_controller_`.
  raw_ptr<OverflowButton> overflow_button_ = nullptr;

  // There are two situations where background_view_left_ and
  // background_view_right_ need be repainted: window active state change and
  // theme change. active_state_subscription_ handles the former, and the latter
  // causes the whole toolbar to be repainted so not special logic is necessary.
  raw_ptr<View> background_view_left_ = nullptr;
  raw_ptr<View> background_view_right_ = nullptr;

  // Listens to changes to active state to update background_view_right_ and
  // background_view_left_, as their background depends on active state.
  base::CallbackListSubscription active_state_subscription_;
};

extern const ui::ClassProperty<bool>* const kActionItemUnderlineIndicatorKey;

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_VIEW_H_
