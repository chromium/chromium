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
#include "chrome/browser/command_observer.h"
#include "chrome/browser/glic/browser_ui/glic_button_controller_delegate.h"
#include "chrome/browser/glic/browser_ui/glic_nudge_delegate.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/toolbar/app_menu_icon_controller.h"
#include "chrome/browser/ui/toolbar/back_forward_menu_model.h"
#include "chrome/browser/ui/views/frame/browser_root_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/intent_picker_bubble_view.h"
#include "chrome/browser/ui/views/location_bar/custom_tab_bar_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/avatar_toolbar_button_interface.h"
#include "chrome/browser/ui/views/toolbar/overflow_button.h"
#include "chrome/browser/ui/views/toolbar/pinned_action_toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions.h"
#include "chrome/browser/ui/views/toolbar/split_tabs_button.h"
#include "components/prefs/pref_member.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/animation/animation_delegate_views.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/mouse_watcher.h"
#include "ui/views/view.h"
#include "url/origin.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/ash/experiences/arc/intent_helper/arc_intent_helper_bridge.h"
#include "chromeos/ash/experiences/arc/mojom/intent_helper.mojom-forward.h"  // nogncheck https://crbug.com/784179
#endif  // BUILDFLAG(IS_CHROMEOS)

class AvatarToolbarButton;
class AvatarToolbarButtonInterface;
class BatterySaverButton;
class BrowserAppMenuButton;
class Browser;
class ExtensionsToolbarButton;
class ExtensionsToolbarDesktop;
class HomeButton;
class IntentChipButton;
class ExtensionsToolbarCoordinator;
class MediaToolbarButtonView;
class ReloadButton;
class WebUIToolbarWebView;
class PinnedToolbarActionsContainer;
class ToolbarButton;
class AvatarToolbarButtonBrowserTest;
class ToolbarController;
class ToolbarDivider;
class OverflowButton;
class PerformanceInterventionButton;

namespace views {
class FlexLayout;
}  // namespace views

namespace glic {
class ToolbarGlicButton;
class ToolbarGlicActorTaskIcon;
class GlicButtonInterface;
}  // namespace glic

class GlicAndActorButtonsContainer;

enum class ExpansionMode {
  kNone = 0,
  kWillShow,
  kWillHide,
};

// The Browser Window's toolbar.
class ToolbarView : public views::AccessiblePaneView,
                    public ui::AcceleratorProvider,
                    public views::AnimationDelegateViews,
                    public LocationBarView::Delegate,
                    public CommandObserver,
                    public views::MouseWatcherListener,
                    public AppMenuIconController::Delegate,
                    public ToolbarButtonProvider,
                    public BrowserRootView::DropTarget,
                    public glic::GlicButtonControllerDelegate,
                    public glic::GlicNudgeDelegate {
  METADATA_HEADER(ToolbarView, views::AccessiblePaneView)

 public:
  // Types of display mode this toolbar can have.
  enum class DisplayMode {
    kNormal,    // Normal toolbar with buttons, etc.
    kLocation,  // Slimline toolbar showing only compact location
                // bar, used for popups.
    kCustomTab  // Custom tab bar, used in PWAs when a location
                // needs to be displayed.
  };

  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kToolbarElementId);

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

  // Clears the current state for |tab|.
  void ResetTabState(content::WebContents* tab);

  // Set focus to the toolbar with complete keyboard access, with the
  // focus initially set to the app menu. Focus will be restored
  // to the last focused view if the user escapes.
  void SetPaneFocusAndFocusAppMenu();

  // Returns true if the app menu is focused.
  bool GetAppMenuFocused() const;

  WebUIToolbarWebView* GetWebUIToolbarViewForTesting() override;

  void ShowIntentPickerBubble(
      std::vector<IntentPickerBubbleView::AppInfo> app_info,
      bool show_stay_in_chrome,
      bool show_remember_selection,
      IntentPickerBubbleView::BubbleType bubble_type,
      const std::optional<url::Origin>& initiating_origin,
      IntentPickerResponse callback);

  // Shows a bookmark bubble and anchors it appropriately.
  void ShowBookmarkBubble(const GURL& url, bool already_bookmarked);

  // Used to test whether `test_point` should be treated as part of the caption
  // bar, which means it can be used to drag the window or open the window
  // context menu. Should only be called when the toolbar is in the caption
  // area.
  bool IsPositionInWindowCaption(const gfx::Point& test_point) const;

  // Accessors.
  Browser* browser() const { return browser_; }
  views::Button* GetChromeLabsButton() const;
  ExtensionsToolbarDesktop* extensions_container() const {
    return extensions_container_;
  }
  ToolbarButton* forward_button() const { return forward_; }
  ExtensionsToolbarButton* GetExtensionsButton() const;
  ReloadButton* reload_button() const { return reload_; }
  LocationBarView* location_bar_view() const { return location_bar_view_; }
  LocationBar* location_bar() const { return location_bar_; }
  CustomTabBarView* custom_tab_bar() { return custom_tab_bar_; }
  BatterySaverButton* battery_saver_button() const {
    return battery_saver_button_;
  }
  PerformanceInterventionButton* performance_intervention_button() const {
    return performance_intervention_button_;
  }
  MediaToolbarButtonView* media_button() const { return media_button_; }
  BrowserAppMenuButton* app_menu_button() const { return app_menu_button_; }
  HomeButton* home_button() const { return home_; }
  PinnedActionToolbarButton* tab_search_button() const {
    return tab_search_button_;
  }
  AppMenuIconController* app_menu_icon_controller() {
    return &app_menu_icon_controller_;
  }
  const ToolbarController* toolbar_controller() const {
    return toolbar_controller_.get();
  }

  WebUIToolbarWebView* detached_toolbar_webview_for_testing() {
    return detached_toolbar_webview_.get();
  }

  glic::ToolbarGlicActorTaskIcon* glic_actor_task_icon() {
    return glic_actor_task_icon_;
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

  friend class AvatarToolbarButtonBaseBrowserTest;

  // GlicNudgeDelegate:
  // Called when the glic nudge UI needs to be triggered. `label' holds the
  // nudge label. `anchored_message_text` and `prompt_suggestion` are unused in
  // this UI.
  void OnTriggerGlicNudgeUI(glic::NudgeParams params) override;
  // Called when the glic nudge UI needs to be hidden.
  void OnHideGlicNudgeUI() override;
  // Called when we want to check if the UI is currently showing.
  bool GetIsShowingGlicNudge() override;

  void ShowGlicActorTaskIcon();
  void HideGlicActorTaskIcon();
  bool GetIsShowingGlicActorTaskIconNudge();
  void TriggerGlicActorNudge(const std::u16string nudge_text);
  bool IsGlicAdded();

  // Updates glic button parenting after hiding glic actor task icon.
  void FinalizeHideGlicActorTaskIcon();

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
  ExtensionsToolbarDesktop* GetExtensionsToolbarDesktop() override;
  PinnedToolbarActions* GetPinnedToolbarActions() override;
  gfx::Size GetToolbarButtonSize() const override;
  views::BubbleAnchor GetDefaultExtensionDialogAnchor() override;
  PageActionIconView* GetPageActionIconView(PageActionIconType type) override;
  IconLabelBubbleView* GetPageActionView(actions::ActionId action_id) override;
  AppMenuControl* GetAppMenuControl() override;
  gfx::Rect GetFindBarBoundingBox(int contents_bottom) override;
  void FocusToolbar() override;
  views::AccessiblePaneView* GetAsAccessiblePaneView() override;
  views::BubbleAnchor GetBubbleAnchor(
      std::optional<actions::ActionId> action_id) override;
  void ZoomChangedForActiveTab(bool can_show_bubble) override;
  AvatarToolbarButtonInterface* GetAvatarToolbarButtonInterface() override;
  ToolbarButton* GetBackButton() override;
  ReloadControl* GetReloadButton() override;
  IntentChipButton* GetIntentChipButton() override;
  ToolbarButton* GetDownloadButton() override;

  // BrowserRootView::DropTarget
  std::optional<BrowserRootView::DropIndex> GetDropIndex(
      const ui::DropTargetEvent& event) override;
  BrowserRootView::DropTarget* GetDropTarget(
      gfx::Point loc_in_local_coords) override;
  views::View* GetViewForDrop() override;

  // GlicButtonControllerDelegate:
  void SetButtonController(glic::GlicButtonController* controller) override;
  void SetGlicShowState(bool show) override;
  void SetGlicPanelIsOpen(bool open) override;

  // views::MouseWatcherListener:
  void MouseMovedOutOfHost() override;

  // May return a View that is not drawn; prefer using GetBubbleAnchor().
  views::BubbleAnchor FindBubbleAnchor(
      std::optional<actions::ActionId> action_id);

  // Changes the visibility of the Chrome Labs entry point based on prefs.
  void OnChromeLabsPrefChanged();

  // Loads the images for all the child views.
  void LoadImages();

  void OnShowForwardButtonChanged();

  void OnShowHomeButtonChanged();

  void OnTouchUiChanged();

  void NewTabButtonPressed(const ui::Event& event);

  void InitGlicContainer();

  void OnVerticalTabStripModeChanged(
      tabs::VerticalTabStripStateController* controller);

  void SetForwardButtonVisibility(bool visible);

  gfx::Size GetBackForwardButtonSize(bool minimum_size = false) const;

  std::unique_ptr<glic::ToolbarGlicButton> CreateGlicButton();
  void OnGlicButtonClicked();
  void OnGlicButtonDismissed();
  void OnGlicButtonAnimationEnded();
  void ShowToolbarNudge(glic::GlicButtonInterface* button);
  void HideToolbarNudge(glic::GlicButtonInterface* button);
  void ShowGlicActorNudge(const std::u16string nudge_text);
  void ExecuteShowToolbarNudge(glic::GlicButtonInterface* button);
  void ExecuteHideToolbarNudge(glic::GlicButtonInterface* button);
  void UpdateGlicActorVisibility();
  void UpdateGlicButtonVisibility();
  void SetGlicActorShowState(bool show);
  void UpdateGlicActorButtonContainerBorders();

  std::unique_ptr<glic::ToolbarGlicActorTaskIcon> CreateGlicActorTaskIcon();
  void OnGlicActorTaskIconClicked();
  std::unique_ptr<GlicAndActorButtonsContainer>
  CreateGlicActorButtonContainer();

  // Update the expansion mode to be executed once the mouse is no longer over
  // the nudge. This button will be what is expanded, either the glic button or
  // actor button.
  void SetLockedExpansionMode(ExpansionMode mode,
                              glic::GlicButtonInterface* button);

  gfx::SlideAnimation size_animation_{this};

  // Controls. Most of these can be null, e.g. in popup windows. Only
  // |location_bar_| is guaranteed to exist. These pointers are owned by the
  // view hierarchy.
  raw_ptr<ToolbarButton> back_ = nullptr;
  raw_ptr<ToolbarButton> forward_ = nullptr;
  raw_ptr<ReloadButton> reload_ = nullptr;
  raw_ptr<WebUIToolbarWebView> toolbar_webview_ = nullptr;
  std::unique_ptr<WebUIToolbarWebView> detached_toolbar_webview_;
  raw_ptr<HomeButton> home_ = nullptr;
  raw_ptr<SplitTabsToolbarButton> split_tabs_ = nullptr;
  raw_ptr<CustomTabBarView> custom_tab_bar_ = nullptr;
  raw_ptr<LocationBarView> location_bar_view_ = nullptr;

  // An alias for `location_bar_view_` or `toolbar_webview_->GetLocationBar()`.
  raw_ptr<LocationBar> location_bar_ = nullptr;
  raw_ptr<ExtensionsToolbarDesktop> extensions_container_ = nullptr;
  raw_ptr<ToolbarDivider> toolbar_divider_ = nullptr;
  raw_ptr<BatterySaverButton> battery_saver_button_ = nullptr;
  raw_ptr<PerformanceInterventionButton> performance_intervention_button_ =
      nullptr;
  raw_ptr<PinnedToolbarActionsContainer> pinned_toolbar_actions_container_ =
      nullptr;

  // An alias for `pinned_toolbar_actions_container_` or
  // `toolbar_webview_->GetPinnedActionsContainer()`.
  raw_ptr<PinnedToolbarActions> pinned_toolbar_actions_ = nullptr;
  raw_ptr<AvatarToolbarButton> avatar_ = nullptr;
  raw_ptr<MediaToolbarButtonView> media_button_ = nullptr;
  raw_ptr<BrowserAppMenuButton> app_menu_button_ = nullptr;
  raw_ptr<PinnedActionToolbarButton> tab_search_button_ = nullptr;

  // The button currently holding the lock to be shown/hidden.
  raw_ptr<glic::GlicButtonInterface> locked_expansion_button_ = nullptr;
  raw_ptr<GlicAndActorButtonsContainer> glic_actor_button_container_ = nullptr;
  raw_ptr<glic::ToolbarGlicButton> glic_button_ = nullptr;
  raw_ptr<glic::ToolbarGlicActorTaskIcon> glic_actor_task_icon_ = nullptr;
  raw_ptr<ToolbarDivider> glic_button_divider_ = nullptr;
  raw_ptr<glic::GlicButtonController> button_controller_ = nullptr;

  // When locked, the container is unable to change its expanded state.
  // Changes will be staged until after this is unlocked.
  ExpansionMode locked_expansion_mode_ = ExpansionMode::kNone;

  // MouseWatcher is used to lock and unlock the expansion state of this
  // container.
  std::unique_ptr<views::MouseWatcher> mouse_watcher_;

  raw_ptr<ToolbarButton> ai_overlay_dialog_button_ = nullptr;

  const raw_ptr<Browser> browser_;
  const raw_ptr<BrowserView> browser_view_;

  // ToolbarView may or may not serve as the `ToolbarButtonProvider` for a given
  // browser instance depending on the browser type (e.g. WebApp browsers set
  // their own in `WebAppFrameToolbarView`). Make this optional to allow
  // conditionally configuring this as the `ToolbarButtonProvider`.
  std::optional<ui::ScopedUnownedUserData<ToolbarButtonProvider>>
      scoped_unowned_user_data_;

  raw_ptr<views::FlexLayout> layout_manager_ = nullptr;

  AppMenuIconController app_menu_icon_controller_;

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

  // A chevron button that indicates some toolbar elements have overflowed
  // due to small toolbar view width. Visibility controlled by
  // `toolbar_controller_`.
  raw_ptr<OverflowButton> overflow_button_ = nullptr;

  // Subscription for when tab strip mode changes
  base::CallbackListSubscription vertical_tab_subscription_;

  bool should_display_vertical_tabs_ = false;
  bool should_show_glic_button_ = false;
  bool should_show_glic_actor_ = false;
};

extern const ui::ClassProperty<bool>* const kActionItemUnderlineIndicatorKey;

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_VIEW_H_
