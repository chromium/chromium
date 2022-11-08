// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_VIEW_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/command_observer.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/toolbar/app_menu_icon_controller.h"
#include "chrome/browser/ui/toolbar/back_forward_menu_model.h"
#include "chrome/browser/ui/views/frame/browser_root_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/intent_picker_bubble_view.h"
#include "chrome/browser/ui/views/location_bar/custom_tab_bar_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/chrome_labs_bubble_view_model.h"
#include "chrome/browser/ui/views/toolbar/side_panel_toolbar_button.h"
#include "chrome/browser/upgrade_detector/upgrade_observer.h"
#include "components/prefs/pref_member.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
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
class MediaToolbarButtonView;
class ReloadButton;
class ToolbarButton;
class AvatarToolbarButtonBrowserTest;

namespace bookmarks {
class BookmarkBubbleObserver;
}

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
                    public UpgradeObserver,
                    public ToolbarButtonProvider,
                    public BrowserRootView::DropTarget {
 public:
  METADATA_HEADER(ToolbarView);

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
      const absl::optional<url::Origin>& initiating_origin,
      IntentPickerResponse callback);

  // Shows a bookmark bubble and anchors it appropriately.
  void ShowBookmarkBubble(const GURL& url,
                          bool already_bookmarked,
                          bookmarks::BookmarkBubbleObserver* observer);

  // Accessors.
  Browser* browser() const { return browser_; }
  ChromeLabsButton* chrome_labs_button() const { return chrome_labs_button_; }
  ChromeLabsBubbleViewModel* chrome_labs_model() const {
    return chrome_labs_model_.get();
  }
  DownloadToolbarButtonView* download_button() const {
    return download_button_;
  }
  ExtensionsToolbarContainer* extensions_container() const {
    return extensions_container_;
  }
  ExtensionsToolbarButton* GetExtensionsButton() const;
  ReloadButton* reload_button() const { return reload_; }
  ToolbarButton* left_side_panel_button() { return left_side_panel_button_; }
  LocationBarView* location_bar() const { return location_bar_; }
  CustomTabBarView* custom_tab_bar() { return custom_tab_bar_; }
  BatterySaverButton* battery_saver_button() const {
    return battery_saver_button_;
  }
  media_router::CastToolbarButton* cast_button() const { return cast_; }
  SidePanelToolbarButton* side_panel_button() const {
    return side_panel_button_;
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

  // LocationBarView::Delegate:
  content::WebContents* GetWebContents() override;
  LocationBarModel* GetLocationBarModel() override;
  const LocationBarModel* GetLocationBarModel() const override;
  ContentSettingBubbleModelDelegate* GetContentSettingBubbleModelDelegate()
      override;

  // CommandObserver:
  void EnabledStateChangedForCommand(int id, bool enabled) override;

  // UpgradeObserver toolbar_button_view_provider.
  void OnOutdatedInstall() override;
  void OnOutdatedInstallNoAutoUpdate() override;
  void OnCriticalUpgradeInstalled() override;

  // ui::AcceleratorProvider:
  bool GetAcceleratorForCommandId(int command_id,
                                  ui::Accelerator* accelerator) const override;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  gfx::Size GetMinimumSize() const override;
  void Layout() override;
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
  // AccessiblePaneView:
  views::View* GetDefaultFocusableChild() override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

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
  SkColor GetDefaultColorForSeverity(
      AppMenuIconController::Severity severity) const override;

  // ToolbarButtonProvider:
  ExtensionsToolbarContainer* GetExtensionsToolbarContainer() override;
  gfx::Size GetToolbarButtonSize() const override;
  views::View* GetDefaultExtensionDialogAnchorView() override;
  PageActionIconView* GetPageActionIconView(PageActionIconType type) override;
  AppMenuButton* GetAppMenuButton() override;
  gfx::Rect GetFindBarBoundingBox(int contents_bottom) override;
  void FocusToolbar() override;
  views::AccessiblePaneView* GetAsAccessiblePaneView() override;
  views::View* GetAnchorView(PageActionIconType type) override;
  void ZoomChangedForActiveTab(bool can_show_bubble) override;
  SidePanelToolbarButton* GetSidePanelButton() override;
  AvatarToolbarButton* GetAvatarToolbarButton() override;
  ToolbarButton* GetBackButton() override;
  ReloadButton* GetReloadButton() override;
  IntentChipButton* GetIntentChipButton() override;
  DownloadToolbarButtonView* GetDownloadButton() override;

  // BrowserRootView::DropTarget
  BrowserRootView::DropIndex GetDropIndex(
      const ui::DropTargetEvent& event) override;
  BrowserRootView::DropTarget* GetDropTarget(
      gfx::Point loc_in_local_coords) override;
  views::View* GetViewForDrop() override;

  // Changes the visibility of the Chrome Labs entry point based on prefs.
  void OnChromeLabsPrefChanged();

  // Loads the images for all the child views.
  void LoadImages();

  // Shows the critical notification bubble against the app menu.
  void ShowCriticalNotification();

  // Shows the outdated install notification bubble against the app menu.
  // |auto_update_enabled| is set to true when auto-upate is on.
  void ShowOutdatedInstallNotification(bool auto_update_enabled);

  void OnShowHomeButtonChanged();

  void OnTouchUiChanged();

  gfx::SlideAnimation size_animation_{this};

  // Controls. Most of these can be null, e.g. in popup windows. Only
  // |location_bar_| is guaranteed to exist. These pointers are owned by the
  // view hierarchy.
  raw_ptr<ToolbarButton> left_side_panel_button_ = nullptr;
  raw_ptr<ToolbarButton> back_ = nullptr;
  raw_ptr<ToolbarButton> forward_ = nullptr;
  raw_ptr<ReloadButton> reload_ = nullptr;
  raw_ptr<HomeButton> home_ = nullptr;
  raw_ptr<CustomTabBarView> custom_tab_bar_ = nullptr;
  raw_ptr<LocationBarView> location_bar_ = nullptr;
  raw_ptr<ExtensionsToolbarContainer> extensions_container_ = nullptr;
  raw_ptr<ChromeLabsButton> chrome_labs_button_ = nullptr;
  raw_ptr<BatterySaverButton> battery_saver_button_ = nullptr;
  raw_ptr<media_router::CastToolbarButton> cast_ = nullptr;
  raw_ptr<SidePanelToolbarButton> side_panel_button_ = nullptr;
  raw_ptr<AvatarToolbarButton> avatar_ = nullptr;
  raw_ptr<MediaToolbarButtonView> media_button_ = nullptr;
  raw_ptr<send_tab_to_self::SendTabToSelfToolbarIconView>
      send_tab_to_self_button_ = nullptr;
  raw_ptr<BrowserAppMenuButton> app_menu_button_ = nullptr;
  raw_ptr<DownloadToolbarButtonView> download_button_ = nullptr;

  const raw_ptr<Browser> browser_;
  const raw_ptr<BrowserView> browser_view_;

  raw_ptr<views::FlexLayout> layout_manager_ = nullptr;

  AppMenuIconController app_menu_icon_controller_;

  std::unique_ptr<ChromeLabsBubbleViewModel> chrome_labs_model_;

  // Controls whether or not a home button should be shown on the toolbar.
  BooleanPrefMember show_home_button_;

  BooleanPrefMember show_chrome_labs_button_;

  // The display mode used when laying out the toolbar.
  const DisplayMode display_mode_;

  base::CallbackListSubscription subscription_ =
      ui::TouchUiController::Get()->RegisterCallback(
          base::BindRepeating(&ToolbarView::OnTouchUiChanged,
                              base::Unretained(this)));

  // Whether this toolbar has been initialized.
  bool initialized_ = false;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_VIEW_H_
