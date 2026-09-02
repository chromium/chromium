// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_TOOLBAR_WEB_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_TOOLBAR_WEB_VIEW_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions.h"
#include "chrome/browser/ui/views/toolbar/toolbar_controller.h"
#include "chrome/browser/ui/views/toolbar/webui_app_menu_control.h"
#include "chrome/browser/ui/views/toolbar/webui_avatar_toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/webui_back_forward_control.h"
#include "chrome/browser/ui/views/toolbar/webui_battery_saver_control.h"
#include "chrome/browser/ui/views/toolbar/webui_home_control.h"
#include "chrome/browser/ui/views/toolbar/webui_overflow_button.h"
#include "chrome/browser/ui/views/toolbar/webui_performance_intervention_control.h"
#include "chrome/browser/ui/views/toolbar/webui_pinned_toolbar_actions.h"
#include "chrome/browser/ui/views/toolbar/webui_reload_control.h"
#include "chrome/browser/ui/views/toolbar/webui_split_tabs_control.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_extensions_container_wrapper.h"
#include "chrome/browser/ui/webui/webui_toolbar/adapters/navigation_controls_state_fetcher.h"
#include "chrome/browser/ui/webui/webui_toolbar/browser_controls_service.h"
#include "chrome/browser/ui/webui/webui_toolbar/icon_table.h"
#include "chrome/browser/ui/webui/webui_toolbar/toolbar_ui_service.h"
#include "chrome/browser/ui/webui/webui_toolbar/webui_toolbar_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api.mojom.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom.h"
#include "content/public/browser/scoped_accessibility_mode.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/view.h"

class BrowserWindowInterface;
class ExtensionsContainerViews;
class MediaToolbarButton;
class WebUILocationBar;
class WebUIOverflowButton;
class WebUIToolbarUI;
class WebUIToolbarInternalWebView;

// This has to be forward declared and stored in unique_ptrs<> due to the
// separate toolbar/impl targets in BUILD.gn.
namespace browser_controls_api {
class BrowserControlsAdapterImpl;
}

namespace content {
struct ContextMenuParams;
}  // namespace content

namespace views {
struct ProposedLayout;
class WebView;
}  // namespace views

// An interface to allow toolbar controls to communicate with the parent
// WebUIToolbarWebView.
class WebUIToolbarControlDelegate {
 public:
  virtual ~WebUIToolbarControlDelegate() = default;

  virtual BrowserWindowInterface* GetBrowser() = 0;
  virtual chrome::BrowserCommandController* GetCommandController() = 0;
  virtual views::View* GetView() = 0;
  // Returns the internal view that's the actual WebView.
  virtual views::View* GetInternalWebView() = 0;
  virtual content::WebContents* GetWebContents() = 0;

  // Announces an alert to accessibility screen readers.
  virtual void AnnounceAlert(const std::u16string& announcement) = 0;

  virtual webui_toolbar::IconTable& GetIconTable() = 0;

  // Indicate preferred size of a toolbar control has changed. This results in
  // synchronously fully recalculating layout to see if anything needs to be
  // changed, so should only be called when something actually changed.
  virtual void OnPreferredSizeChanged() = 0;

  // Indicates a toolbar control's state has changed.
  virtual void OnReloadControlStateChanged(
      toolbar_ui_api::mojom::ReloadControlStatePtr state) = 0;
  virtual void OnSplitTabsControlStateChanged(
      toolbar_ui_api::mojom::SplitTabsControlStatePtr state) = 0;
  virtual void OnBackForwardStateChanged() = 0;
  virtual void OnHomeControlStateChanged(
      toolbar_ui_api::mojom::HomeControlStatePtr state) = 0;
  virtual void OnPerformanceInterventionControlStateChanged(
      toolbar_ui_api::mojom::PerformanceInterventionControlStatePtr state) = 0;
  virtual void OnAppMenuControlStateChanged(
      toolbar_ui_api::mojom::AppMenuControlStatePtr state) = 0;
  virtual void OnOverflowButtonControlStateChanged(
      toolbar_ui_api::mojom::OverflowButtonControlStatePtr state) = 0;
  virtual void OnBatterySaverControlStateChanged(bool is_showing) = 0;
  virtual void OnOmniboxViewStateChanged(
      toolbar_ui_api::mojom::OmniboxViewStatePtr state) = 0;
  virtual void OnLocationBarFlagsChanged(
      toolbar_ui_api::mojom::LocationBarFlagsPtr state) = 0;
  virtual void OnSelectedKeywordChanged(
      toolbar_ui_api::mojom::SelectedKeywordStatePtr state) = 0;
  virtual void OnLhsChipsStateChanged(
      toolbar_ui_api::mojom::LhsChipsStatePtr state) = 0;
  virtual void OnPinnedToolbarActionsStateChanged(
      std::vector<toolbar_ui_api::mojom::PinnedToolbarActionStatePtr>
          state) = 0;
  virtual void OnExtensionsStateChanged(
      std::vector<extensions_bar::mojom::ExtensionActionInfoPtr> state) = 0;
  virtual void OnContentSettingChanged(
      std::vector<toolbar_ui_api::mojom::ContentSettingImageStatePtr>
          state) = 0;
  virtual void OnPageActionChanged(
      std::vector<toolbar_ui_api::mojom::PageActionStatePtr> state) = 0;
  virtual void OnAvatarControlStateChanged(
      toolbar_ui_api::mojom::AvatarControlStatePtr state) = 0;
  virtual void OnFocusRequested(
      toolbar_ui_api::mojom::FocusRequestTarget target) = 0;

  virtual void OverflowButtonClicked(ui::ElementIdentifier identifier) = 0;

  virtual std::optional<GURL> ConsumeDroppedUrl(
      const gfx::PointF& drop_position) = 0;

  // Read the latest state.
  virtual const toolbar_ui_api::mojom::NavigationControlsState& GetState()
      const = 0;
};

// A view that displays one or more adjacent controls on the toolbar as a single
// WebView. Which controls are displayed is controlled by the IsWebUI*Enabled()
// series of methods declared in chrome/browser/ui/ui_features.h. Enabling
// features to display toolbar components that are non-adjacent will result in
// incorrect display order of buttons on the toolbar.
class WebUIToolbarWebView
    : public views::View,
      public content::WebContentsObserver,
      public toolbar_ui_api::ToolbarUIService::ToolbarUIServiceDelegate,
      public ToolbarController::WebUIToolbarControllerDelegate,
      public browser_controls_api::BrowserControlsService::
          BrowserControlsServiceDelegate,
      public WebUIToolbarUI::DependencyProvider,
      public WebUIToolbarControlDelegate,
      public webui_toolbar::IconTable::Delegate {
  METADATA_HEADER(WebUIToolbarWebView, views::View)

 public:
  // `location_bar` may be null.
  WebUIToolbarWebView(BrowserWindowInterface* browser,
                      chrome::BrowserCommandController* controller,
                      std::unique_ptr<WebUILocationBar> location_bar);
  WebUIToolbarWebView(const WebUIToolbarWebView&) = delete;
  WebUIToolbarWebView& operator=(const WebUIToolbarWebView&) = delete;
  ~WebUIToolbarWebView() override;

  ReloadControl* GetReloadControl();
  WebUIPinnedToolbarActions* GetPinnedToolbarActions() {
    return &pinned_toolbar_actions_;
  }
  AvatarToolbarButtonInterface* GetAvatarToolbarButtonInterface();
  MediaToolbarButton* GetMediaToolbarButton();
  WebUIAppMenuControl* GetAppMenuControl() { return &app_menu_control_; }
  ExtensionsContainerViews* extensions_container_views();
  const WebUIAppMenuControl* GetAppMenuControl() const {
    return &app_menu_control_;
  }
  WebUIOverflowButton& overflow_button_for_testing() {
    return overflow_button_;
  }

  void SetIsMaximizedOrFullscreen(bool maximized_or_fullscreen);
  void SetBackForwardEnabled(int command_id, bool enabled);
  void SetForwardVisible(bool visible);

  // May be nullptr.
  WebUILocationBar* GetLocationBar() { return location_bar_.get(); }

  // WebUIToolbarUI::DependencyProvider:
  base::WeakPtr<DependencyProvider> GetWeakPtr() override;
  browser_controls_api::BrowserControlsService::BrowserControlsServiceDelegate*
  GetBrowserControlsDelegate() override;
  toolbar_ui_api::ToolbarUIService::ToolbarUIServiceDelegate*
  GetToolbarUIServiceDelegate() override;
  std::unique_ptr<toolbar_ui_api::NavigationControlsStateFetcher>
  GetNavigationControlsStateFetcher() override;
  std::unique_ptr<toolbar_ui_api::IconTableFetcher> GetIconTableFetcher()
      override;
  CommandUpdater* GetCommandUpdater() override;
  OmniboxController* GetOmniboxController() override;

  // ToolbarUIService::ToolbarUIServiceDelegate:
  void HandleContextMenu(toolbar_ui_api::mojom::ContextMenuType menu_type,
                         const gfx::RectF& bounds_in_css_pixels,
                         ui::mojom::MenuSourceType source) override;
  void ShowOverflowMenu(
      std::vector<toolbar_ui_api::mojom::OverflowMenuItemPtr> controls,
      const gfx::RectF& bounds_in_css_pixels,
      ui::mojom::MenuSourceType source,
      toolbar_ui_api::mojom::ToolbarUIService::ShowOverflowMenuCallback
          callback) override;
  void ShowContentSettingsBubble(
      ::toolbar_ui_api::mojom::ContentSettingImageType type,
      bool is_pointer_interaction,
      toolbar_ui_api::mojom::ToolbarUIService::ShowContentSettingsBubbleCallback
          callback) override;
  void OnContentSettingImagePointerDown(
      ::toolbar_ui_api::mojom::ContentSettingImageType type) override;
  void OnContentSettingImageAnimationEnded(
      ::toolbar_ui_api::mojom::ContentSettingImageType type) override;
  void OnPageActionPointerDown(
      ::toolbar_ui_api::mojom::PageActionId action_id) override;
  void OnPageActionClick(
      ::toolbar_ui_api::mojom::PageActionId action_id,
      ::toolbar_ui_api::mojom::PageActionTrigger trigger,
      ::toolbar_ui_api::mojom::ToolbarUIService::OnPageActionClickCallback
          callback) override;
  void OnPageActionChipShowingChanged(
      ::toolbar_ui_api::mojom::PageActionId action_id,
      ::toolbar_ui_api::mojom::ToolbarUIService::
          OnPageActionChipShowingChangedCallback callback) override;
  void OnPageInitialized() override;
  void InvokePinnedToolbarAction(
      toolbar_ui_api::mojom::PinnedToolbarAction action_id) override;
  void OnLocationBarFocusWithinChanged(bool focused) override;
  void MovePinnedToolbarAction(
      toolbar_ui_api::mojom::PinnedToolbarAction action_id,
      int32_t target_index) override;
  void MovePinnedToolbarActionBy(
      toolbar_ui_api::mojom::PinnedToolbarAction action_id,
      int32_t delta) override;
  void MoveExtensionAction(const std::string& extension_id,
                           int32_t target_index) override;
  void MoveExtensionActionBy(const std::string& extension_id,
                             int32_t delta) override;
  void OnLhsChipMousePressed(
      toolbar_ui_api::mojom::LhsChipIdentifier identifier,
      bool is_middle_click) override;
  void OnLhsChipClicked(toolbar_ui_api::mojom::LhsChipIdentifier identifier,
                        bool is_mouse_interaction) override;
  void OnLhsChipPointerEntered(
      toolbar_ui_api::mojom::LhsChipIdentifier identifier) override;
  void OnLhsChipPointerExited(
      toolbar_ui_api::mojom::LhsChipIdentifier identifier) override;
  void OnLhsChipExpandAnimationEnded(
      toolbar_ui_api::mojom::LhsChipIdentifier identifier) override;
  void OnLhsChipCollapseAnimationEnded(
      toolbar_ui_api::mojom::LhsChipIdentifier identifier) override;
  void OnLhsChipDrag(toolbar_ui_api::mojom::LhsChipIdentifier identifier,
                     ui::mojom::DragEventSource source) override;
  void OnHomeButtonDropUrl(const GURL& url) override;
  void OnHomeButtonDropFile(const gfx::PointF& drop_position) override;
  void OnToolbarDropFile(const gfx::PointF& drop_position) override;
  base::expected<std::monostate, mojo_base::mojom::ErrorPtr> OnOmniboxAction(
      toolbar_ui_api::mojom::OmniboxActionPtr action) override;
  void ShowAvatarMenu() override;
  void SetAvatarButtonHovered(bool hovered) override;
  void SetAvatarButtonFocused(bool focused) override;
  void SetAvatarButtonIPHPromoShowing(bool showing) override;
  void OnAppMenuFocusChanged(bool focused) override;
  void ExecuteExtensionAction(const std::string& extension_id) override;
  void ShowExtensionContextMenu(const std::string& extension_id,
                                ui::mojom::MenuSourceType source) override;
  base::expected<toolbar_ui_api::mojom::AdjustOmniboxTextForCopyResultPtr,
                 mojo_base::mojom::ErrorPtr>
  AdjustOmniboxTextForCopy(const std::u16string& text,
                           int32_t selection_start) override;
  void OnPerformanceInterventionButtonClicked(
      bool is_mouse_interaction) override;
  void OnPerformanceInterventionButtonMousePressed() override;

  // BrowserControlsService::BrowserControlsServiceDelegate:
  void PermitLaunchUrl() override;
  base::TimeTicks GetNavigationStartTicks() const override;

  // views::View:
  void AddedToWidget() override;
  void OnThemeChanged() override;
  gfx::Size GetMinimumSize() const override;
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  void PreferredSizeChanged() override;
  void OnBlur() override;

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFirstVisuallyNonEmptyPaint() override;
  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override;

  // ToolbarController::WebUIToolbarControllerDelegate:
  bool IsOverflowed(
      ui::ElementIdentifier identifier,
      const views::ProposedLayout* proposed_layout) const override;
  bool IsEnabled(ui::ElementIdentifier identifier) const override;
  void OverflowButtonClicked(ui::ElementIdentifier identifier) override;

  // webui_toolbar::IconTable::Delegate:
  const ui::ColorProvider* GetColorProvider() const override;
  float GetScaleFactor() const override;

  // Returns the FlexSpecification for determining the size of `this`. The
  // returned value must not outlive `this`, since it includes a bound callback.
  //
  // `navigation_button_flex_order` and `location_bar_flex_order`, are the
  // FlexLayout orders that would be used by the home and forward buttons and
  // the non-WebUI views for the location bar, respectively, if WebUI controls
  // are not enabled. Note that `location_bar_flex_order` is actually between
  // the home and forward button orders, so it's the right order if only one of
  // the two is being handled by WebUI. `location_bar_flex_order` may be either
  // higher or lower than `navigation_button_flex_order`.
  //
  // The WebUI toolbar is a single View within the toolbar's FlexLayout object
  // so can have only a single order/priority in any single layout call by the
  // FlexLayout. However, it can contain elements that should have
  // non-consecutive priorities when it comes to using available horizontal
  // space in the toolbar to reach their preferred sizes. To handle this, the
  // FlexSpecification uses a vector of RuleAndPredicates to determine which
  // single order to use for the WebUIToolbarWebView as a whole.
  //
  // When there's not a lot of space, the WebUIToolbarWebView as a whole uses
  // the lower order of the two passed in (the higher priority one), to try and
  // claim as much space for the higher priority controls as it can. When
  // there's enough space to fit the higher priority controls, the
  // WebUIToolbarWebView uses the lower priority, but returns a FlexLayoutRule
  // that forces the higher priority controls to always be visible, even when
  // calculating the minimum size.
  //
  // Note that this function call records whether `location_bar_flex_order` is
  // higher or lower than `navigation_button_flex_order`, and ComputeLayout()'s
  // behavior will vary accordingly.
  //
  // If features::IsWebUIToolbarFullyEnabled()) is true, which means the WebUI
  // toolbar is managing all controls, then this method must not be called,
  // since layout will be handled in Javascript, instead of by FlexLayout.
  views::FlexSpecification GetFlexSpecification(
      int navigation_button_flex_order,
      int location_bar_flex_order);

  // If we have the focus, adjust the JS focus to be appropriate for focus
  // toolbar operation.
  void AdjustForToolbarFocus();

  // Special path for omnibox context menu, since it needs the `params`.
  void HandleOmniboxContextMenu(const gfx::Point& point,
                                ui::mojom::MenuSourceType source_type,
                                content::ContextMenuParams params);

  void SetDidFirstNonEmptyPaintCallbackForTesting(base::OnceClosure callback);
  void SetTickClockForTesting(const base::TickClock* clock);
  views::WebView* GetWebViewForTesting();
  WebUIPerformanceInterventionControl*
  GetPerformanceInterventionControlForTesting() {
    return &performance_intervention_control_;
  }
  bool IsPendingForTesting() const {
    return initialization_state_ == InitializationState::kPending;
  }

  // Returns the current width of the WebUI location bar. Does this by computing
  // the layout, given the current WebUI toolbar size, rather than by inspecting
  // any location bar state. May only be called when the WebUI location bar is
  // enabled.
  int GetLocationBarWidthForTesting() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(WebUIToolbarWebViewPixelBrowserTest,
                           CheckReloadButtonColor);
  FRIEND_TEST_ALL_PREFIXES(WebUIToolbarWebViewPixelBrowserTest,
                           CheckBackButtonColor);
  FRIEND_TEST_ALL_PREFIXES(WebUIToolbarWebViewPixelBrowserTest,
                           CheckForwardButtonColor);
  FRIEND_TEST_ALL_PREFIXES(WebUIToolbarWebViewPixelBrowserTest,
                           CheckSplitTabsButtonColor);
  FRIEND_TEST_ALL_PREFIXES(WebUIToolbarWebViewPixelBrowserTest,
                           CheckHomeButtonColor);
  FRIEND_TEST_ALL_PREFIXES(WebUIToolbarWebViewPixelBrowserTest,
                           CheckBatterySaverButtonShowHide);
  FRIEND_TEST_ALL_PREFIXES(WebUIToolbarWebViewSplitTabsBrowserTest,
                           CheckSplitTabsButtonSourceType);
  FRIEND_TEST_ALL_PREFIXES(WebUIToolbarRightClickContextMenuTest,
                           RightClickShowsContextMenu);
  FRIEND_TEST_ALL_PREFIXES(WebUIToolbarWebViewHomeButtonBrowserTest,
                           LongPressHomeButton);
  FRIEND_TEST_ALL_PREFIXES(WebUIToolbarWebViewHomeButtonBrowserTest,
                           PressAndDragDownHomeButton);
  FRIEND_TEST_ALL_PREFIXES(WebUIToolbarButtonPressAndDragTest,
                           PressAndDragDown);
  FRIEND_TEST_ALL_PREFIXES(WebUIAppMenuBrowserTest, AppMenuState);
  FRIEND_TEST_ALL_PREFIXES(WebUIToolbarWebViewHomeButtonBrowserTest,
                           DropFileOnHomeButtonAndUndo);
  FRIEND_TEST_ALL_PREFIXES(WebUIToolbarWebViewPixelBrowserTest,
                           BackForwardButtonsModifierClick);
  FRIEND_TEST_ALL_PREFIXES(WebUIToolbarSurfaceSyncBrowserTest,
                           SetsDeadlineOnInit);
  FRIEND_TEST_ALL_PREFIXES(WebUIHomeControlInteractiveUiTest,
                           LongPressHomeButton);
  FRIEND_TEST_ALL_PREFIXES(HomeButtonUiTest, ShowMenu);
  friend class WebUIHomeControlTestBase;
  friend class WebUIToolbarWebViewTestBase;
  friend class WebUIToolbarWebViewBrowserTest;
  friend class WebUIToolbarWebViewInteractiveUiTest;

  // WebUIToolbarControlDelegate:
  BrowserWindowInterface* GetBrowser() override;
  chrome::BrowserCommandController* GetCommandController() override;
  views::View* GetView() override;
  views::View* GetInternalWebView() override;
  content::WebContents* GetWebContents() override;
  void AnnounceAlert(const std::u16string& announcement) override;
  webui_toolbar::IconTable& GetIconTable() override;
  void OnPreferredSizeChanged() override;
  void OnReloadControlStateChanged(
      toolbar_ui_api::mojom::ReloadControlStatePtr state) override;
  void OnSplitTabsControlStateChanged(
      toolbar_ui_api::mojom::SplitTabsControlStatePtr state) override;
  void OnBackForwardStateChanged() override;
  void OnHomeControlStateChanged(
      toolbar_ui_api::mojom::HomeControlStatePtr state) override;
  void OnPerformanceInterventionControlStateChanged(
      toolbar_ui_api::mojom::PerformanceInterventionControlStatePtr state)
      override;
  void OnAppMenuControlStateChanged(
      toolbar_ui_api::mojom::AppMenuControlStatePtr state) override;
  void OnOverflowButtonControlStateChanged(
      toolbar_ui_api::mojom::OverflowButtonControlStatePtr state) override;
  void OnBatterySaverControlStateChanged(bool is_showing) override;
  void OnOmniboxViewStateChanged(
      toolbar_ui_api::mojom::OmniboxViewStatePtr state) override;
  void OnLocationBarFlagsChanged(
      toolbar_ui_api::mojom::LocationBarFlagsPtr state) override;
  void OnSelectedKeywordChanged(
      toolbar_ui_api::mojom::SelectedKeywordStatePtr state) override;
  void OnLhsChipsStateChanged(
      toolbar_ui_api::mojom::LhsChipsStatePtr state) override;
  void OnPinnedToolbarActionsStateChanged(
      std::vector<toolbar_ui_api::mojom::PinnedToolbarActionStatePtr> state)
      override;
  void OnExtensionsStateChanged(
      std::vector<extensions_bar::mojom::ExtensionActionInfoPtr> state)
      override;
  void OnContentSettingChanged(
      std::vector<toolbar_ui_api::mojom::ContentSettingImageStatePtr> state)
      override;
  void OnPageActionChanged(
      std::vector<toolbar_ui_api::mojom::PageActionStatePtr> state) override;
  const toolbar_ui_api::mojom::NavigationControlsState& GetState()
      const override;
  void OnAvatarControlStateChanged(
      toolbar_ui_api::mojom::AvatarControlStatePtr state) override;
  void OnFocusRequested(
      toolbar_ui_api::mojom::FocusRequestTarget target) override;
  std::optional<GURL> ConsumeDroppedUrl(
      const gfx::PointF& drop_position) override;

  toolbar_ui_api::mojom::NavigationControlsStatePtr
  GetNavigationControlsState();

  // Reloads the WebUI toolbar to recover from crashes or unresponsiveness.
  void RecoverFromRendererCrashOrUnresponsiveness();

  // Tracks the initialization stages.
  enum class InitializationState {
    // Created but initialization not yet started.
    kUninitialized,

    // Initialization started but not yet complete.
    kPending,

    // WebUI is ready and controls are initialized.
    kInitialized,
  };

  void SetInitializationState(InitializationState new_state);

  // Applies the specified surface synchronization deadline in frames to both
  // the toolbar and the active main content's RenderWidgetHostView.
  void SetSurfaceSyncDeadline(std::optional<uint32_t> deadline_in_frames);

  // Resolves the initial deadline from features and applies it if enabled.
  void ApplyInitialSurfaceSyncDeadline();

  // Returns the active WebUI toolbar controller (const-safe).
  // Robust against teardown as it uses the observed WebContents.
  WebUIToolbarUI* GetWebUIToolbarUI() const;

  void OnTouchUiChanged();
  void PostPushNavigationState();
  void MaybeInitializePageDependentControls();
  void PushNavigationState();
  toolbar_ui_api::mojom::BackForwardControlStatePtr GetBackForwardState() const;

  // Which buttons have overflowed, and the size of the location bar, if
  // applicable. Allows ComputeLayout() to be const, and usable both for
  // computing putative sizes during layout, and updating which buttons have
  // overflowed when the View is actually resized.
  //
  // Note that if `is_webui_toolbar_fully_enabled_` is true, the logic to
  // calculate `is_*_overflowed` values is not accurate, and what has overflowed
  // should only computed in Javascript.
  //
  // TODO(crbug.com/538175276): When `is_webui_toolbar_fully_enabled_` is true,
  // perform all layout in Javascript, and don't even populate this structure.
  struct ButtonOverflowInfo {
    bool is_forward_button_overflowed = false;
    bool is_home_button_overflowed = false;

    int location_bar_width = 0;
  };

  // Computes the layout of elements displayed in the toolbar such that they fit
  // in the available width, if possible. If `width` is 0, returns the min size,
  // if it's unbounded, returns the size needed to everything. Returns the
  // actual size <= the passed in `available_width` that `this` can usefully
  // use.
  //
  // If `button_overflow_info` is non-null, writes which buttons should be
  // overflowed to it.
  //
  // `force_navigation_buttons` forces the home and forward buttons to be
  // displayed, if pinned, and `force_location_bar` forces the location bar to
  // be at least its preferred width, if enabled.
  gfx::Size ComputeLayout(views::SizeBound available_width,
                          ButtonOverflowInfo* button_overflow_info = nullptr,
                          bool force_navigation_buttons = false,
                          bool force_location_bar = false) const;

  // Uses ComputeLayout() to figure out which buttons should be moved to the
  // overflow menu, given current dimensions, and informs those buttons that
  // they should not be on the overflow menu. Should be called on bounds
  // changes, and when a button is pinned/unpinned or its desired visibility
  // state changes for any other reason.
  void UpdateButtonOverflowState();

  // The FlexRule callback used to create a FlexSpecification that computes the
  // size to use given the provided bounds. This returns what the actual size
  // would be given `bounds`, but does not change internal state or hide any
  // overflowed buttons, since this does not indicate an actual change in the
  // size of the toolbar. Due to that, it neither reads nor writes the overflow
  // state of any buttons, though it does rely on calculating how many buttons
  // would be hidden if the provided bounds were all the available space.
  //
  // `force_navigation_buttons` forces the home and forward buttons to be
  // displayed if pinned (or, more accurately, width to be allocated to display
  // them, regardless of `bounds`), and `force_location_bar` forces the location
  // bar to be at least its preferred width, if enabled.
  gfx::Size FlexLayoutRule(bool force_navigation_buttons,
                           bool force_location_bar,
                           const views::View*,
                           const views::SizeBounds& bounds);

  // The RuleEnabledPredicate used for the higher-priority rule (lower order) in
  // the multi-order FlexSpecification. Returns true to enable the rule, false
  // if the lower-priority rule should be used instead, given `bounds`.
  bool RuleEnabledPredicate(int current_flex_order,
                            const views::SizeBounds& bounds);

  // Converts bounding rectangle coordinates in CSS pixels relative to the
  // viewport origin into absolute screen rectangle coordinates in DIPs.
  gfx::Rect ConvertBoundsFromCssPixelsToScreenCoords(
      const gfx::RectF& bounds_in_css_pixels) const;

  // Whether all controls are being managed by WebUI.
  const bool is_webui_toolbar_fully_enabled_ =
      features::IsWebUIToolbarFullyEnabled();

  // The most recent NavigationControlsState, consisting of the state of all
  // controls managed by the toolbar. This may or may not have been sent to
  // `web_ui`. If this state has not yet been sent, then there must be a pending
  // PushNavigationState() call.
  toolbar_ui_api::mojom::NavigationControlsState last_queued_state_;

  InitializationState initialization_state_ =
      InitializationState::kUninitialized;

  // The WebView displaying the toolbar. Initialized during construction, and
  // not modified afterwards. Cannot be null.
  raw_ptr<WebUIToolbarInternalWebView> web_view_;

  const raw_ptr<BrowserWindowInterface> browser_;
  const raw_ptr<chrome::BrowserCommandController> controller_;

  // Used to handle clicks from the overflow menu, through the
  // ToolbarUIServiceDelegate interface. Once the overflow menus are converted
  // to use WebUI themselves, this should no longer be needed.
  std::unique_ptr<browser_controls_api::BrowserControlsAdapterImpl>
      browser_controls_adapter_;

  webui_toolbar::IconTable icon_table_;

  // Classes that manage individual controls. They are responsible for informing
  // `this` when the state of the control changes. Though most are statically
  // declared here, they should not be active unless their Init() method is called
  // which is only done if their corresponding feature flag is enabled.
  WebUIReloadControl reload_control_;
  WebUISplitTabsControl split_tabs_control_;
  WebUIHomeControl home_control_;
  WebUIPerformanceInterventionControl performance_intervention_control_;
  WebUIAppMenuControl app_menu_control_;
  WebUIBatterySaverControl battery_saver_control_;
  WebUIAvatarToolbarButton avatar_control_;
  // This is null if WebUILocationBar is off, or the window is in one of the
  // modes (e.g. popup) that don't use it yet.
  std::unique_ptr<WebUILocationBar> location_bar_;
  WebUIToolbarExtensionsContainerWrapper extensions_container_;
  WebUIBackForwardControl back_control_;
  WebUIBackForwardControl forward_control_;
  WebUIPinnedToolbarActions pinned_toolbar_actions_;
  WebUIOverflowButton overflow_button_;

  raw_ptr<const base::TickClock> clock_;
  base::OnceClosure did_first_non_empty_paint_callback_;
  bool has_finished_first_non_empty_paint_ = false;

  // How many times the toolbar WebUI has crashed. Cleared after 10s has passed
  // without a crash.
  uint32_t crash_count_ = 0;

  base::TimeTicks last_crash_time_;

  base::CallbackListSubscription touch_ui_subscription_;

  // True if the window is maximized or fullscreen.
  bool window_is_maximized_or_fullscreen_ = false;

  // Tracks if synchronous sub-controls have been initialized once.
  bool sub_controls_initialized_ = false;

  // True if the WebContents was pre-loaded.
  bool is_preloaded_ = false;

  std::unique_ptr<content::ScopedAccessibilityMode> scoped_accessibility_mode_;

  // True if the location bar has a higher priority (lower order) than the home
  // or forward buttons. Calculated based on the passed in priority for the
  // location bar, though in practice, should mirror the value of
  // features::kOmniboxResizingPrioritization.
  //
  // See GetFlexSpecification() for more information.
  bool location_bar_takes_priority_ = false;

  // Pending focus request when focus is requested before WebUI page is
  // initialized.
  std::optional<toolbar_ui_api::mojom::FocusRequestTarget>
      pending_focus_request_;

  base::WeakPtrFactory<DependencyProvider> weak_factory_{this};

  // This WeakPtrFactory is used to keep tabs on pending state pushes, and then
  // used to cancel them if the state is later updated again before we post a
  // later PushNavigationState().
  base::WeakPtrFactory<WebUIToolbarWebView> state_push_weak_ptr_factory_{this};

  base::WeakPtrFactory<WebUIToolbarWebView> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_TOOLBAR_WEB_VIEW_H_
