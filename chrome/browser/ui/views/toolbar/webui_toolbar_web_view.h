// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_TOOLBAR_WEB_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_TOOLBAR_WEB_VIEW_H_

#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions.h"
#include "chrome/browser/ui/views/toolbar/webui_avatar_toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/webui_back_forward_control.h"
#include "chrome/browser/ui/views/toolbar/webui_home_control.h"
#include "chrome/browser/ui/views/toolbar/webui_pinned_toolbar_actions.h"
#include "chrome/browser/ui/views/toolbar/webui_reload_control.h"
#include "chrome/browser/ui/views/toolbar/webui_split_tabs_control.h"
#include "chrome/browser/ui/webui/webui_toolbar/adapters/navigation_controls_state_fetcher.h"
#include "chrome/browser/ui/webui/webui_toolbar/browser_controls_service.h"
#include "chrome/browser/ui/webui/webui_toolbar/toolbar_ui_service.h"
#include "chrome/browser/ui/webui/webui_toolbar/webui_toolbar_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api.mojom.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/views/view.h"

class BrowserWindowInterface;
class WebUILocationBar;
class WebUIToolbarUI;
class WebUIToolbarInternalWebView;

namespace views {
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

  // Announces an alert to accessibility screen readers.
  virtual void AnnounceAlert(const std::u16string& announcement) = 0;

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
  virtual void OnOmniboxViewStateChanged(
      toolbar_ui_api::mojom::OmniboxViewStatePtr state) = 0;
  virtual void OnLocationBarFlagsChanged(
      toolbar_ui_api::mojom::LocationBarFlagsPtr state) = 0;
  virtual void OnLhsChipsStateChanged(
      toolbar_ui_api::mojom::LhsChipsStatePtr state) = 0;
  virtual void OnPinnedToolbarActionsStateChanged(
      std::vector<toolbar_ui_api::mojom::PinnedToolbarActionStatePtr>
          state) = 0;
  virtual void OnContentSettingChanged(
      std::vector<toolbar_ui_api::mojom::ContentSettingImageStatePtr>
          state) = 0;

  // Read the latest pinned toolbar actions state.
  virtual const std::vector<toolbar_ui_api::mojom::PinnedToolbarActionStatePtr>&
  GetPinnedToolbarActionsState() const = 0;
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
      public browser_controls_api::BrowserControlsService::
          BrowserControlsServiceDelegate,
      public WebUIToolbarUI::DependencyProvider,
      public WebUIToolbarControlDelegate {
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

  void SetBackButtonLeadingMargin(int margin);
  void SetBackForwardEnabled(int command_id, bool enabled);
  void SetForwardVisible(bool visible);

  // May be nullptr.
  WebUILocationBar* GetLocationBar() { return location_bar_.get(); }

  // WebUIToolbarUI::DependencyProvider:
  browser_controls_api::BrowserControlsService::BrowserControlsServiceDelegate*
  GetBrowserControlsDelegate() override;
  toolbar_ui_api::ToolbarUIService::ToolbarUIServiceDelegate*
  GetToolbarUIServiceDelegate() override;
  std::unique_ptr<toolbar_ui_api::NavigationControlsStateFetcher>
  GetNavigationControlsStateFetcher() override;
  CommandUpdater* GetCommandUpdater() override;

  // ToolbarUIService::ToolbarUIServiceDelegate:
  void HandleContextMenu(toolbar_ui_api::mojom::ContextMenuType menu_type,
                         const gfx::RectF& bounds_in_css_pixels,
                         ui::mojom::MenuSourceType source) override;
  void ShowContentSettingsBubble(
      ::toolbar_ui_api::mojom::ContentSettingImageType type,
      toolbar_ui_api::mojom::ToolbarUIService::ShowContentSettingsBubbleCallback
          callback) override;
  void OnPageInitialized() override;
  void InvokePinnedToolbarAction(
      toolbar_ui_api::mojom::PinnedToolbarAction action_id) override;
  void OnLhsChipMousePressed(
      toolbar_ui_api::mojom::LhsChipIdentifier identifier) override;
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
  void OnHomeButtonDropUrl(const GURL& url) override;
  void OnHomeButtonDropFile(const gfx::PointF& drop_position) override;
  void OnOmniboxAction(toolbar_ui_api::mojom::OmniboxActionPtr action) override;

  // BrowserControlsService::BrowserControlsServiceDelegate:
  void PermitLaunchUrl() override;

  // views::View:
  void AddedToWidget() override;
  void OnThemeChanged() override;
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFirstVisuallyNonEmptyPaint() override;
  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override;

  void SetDidFirstNonEmptyPaintCallbackForTesting(base::OnceClosure callback);
  void SetTickClockForTesting(const base::TickClock* clock);
  views::WebView* GetWebViewForTesting();
  WebUIHomeControl* GetHomeControlForTesting() { return &home_control_; }
  bool IsPendingForTesting() const {
    return initialization_state_ == InitializationState::kPending;
  }

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
  FRIEND_TEST_ALL_PREFIXES(WebUIToolbarWebViewSplitTabsBrowserTest,
                           CheckSplitTabsButtonSourceType);
  FRIEND_TEST_ALL_PREFIXES(WebUIToolbarWebViewSplitTabsBrowserTest,
                           RightClickSplitTabsButton);
  FRIEND_TEST_ALL_PREFIXES(WebUIToolbarWebViewHomeButtonBrowserTest,
                           RightClickHomeButton);
  FRIEND_TEST_ALL_PREFIXES(WebUIToolbarWebViewHomeButtonBrowserTest,
                           LongPressHomeButton);
  FRIEND_TEST_ALL_PREFIXES(WebUIToolbarWebViewHomeButtonBrowserTest,
                           PressAndDragDownHomeButton);
  FRIEND_TEST_ALL_PREFIXES(WebUIToolbarButtonPressAndDragTest,
                           PressAndDragDown);
  FRIEND_TEST_ALL_PREFIXES(WebUIToolbarWebViewHomeButtonBrowserTest,
                           DropFileOnHomeButtonAndUndo);
  FRIEND_TEST_ALL_PREFIXES(WebUIToolbarWebViewPixelBrowserTest,
                           BackForwardButtonsModifierClick);

  // WebUIToolbarControlDelegate:
  BrowserWindowInterface* GetBrowser() override;
  chrome::BrowserCommandController* GetCommandController() override;
  views::View* GetView() override;
  void AnnounceAlert(const std::u16string& announcement) override;
  void OnPreferredSizeChanged() override;
  void OnReloadControlStateChanged(
      toolbar_ui_api::mojom::ReloadControlStatePtr state) override;
  void OnSplitTabsControlStateChanged(
      toolbar_ui_api::mojom::SplitTabsControlStatePtr state) override;
  void OnBackForwardStateChanged() override;
  void OnHomeControlStateChanged(
      toolbar_ui_api::mojom::HomeControlStatePtr state) override;
  void OnOmniboxViewStateChanged(
      toolbar_ui_api::mojom::OmniboxViewStatePtr state) override;
  void OnLocationBarFlagsChanged(
      toolbar_ui_api::mojom::LocationBarFlagsPtr state) override;
  void OnLhsChipsStateChanged(
      toolbar_ui_api::mojom::LhsChipsStatePtr state) override;
  void OnPinnedToolbarActionsStateChanged(
      std::vector<toolbar_ui_api::mojom::PinnedToolbarActionStatePtr> state)
      override;
  void OnContentSettingChanged(
      std::vector<toolbar_ui_api::mojom::ContentSettingImageStatePtr> state)
      override;
  const std::vector<toolbar_ui_api::mojom::PinnedToolbarActionStatePtr>&
  GetPinnedToolbarActionsState() const override;

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

  WebUIToolbarUI* GetWebUIToolbarUI();

  void OnTouchUiChanged();
  void PostPushNavigationState();
  void PushNavigationState();
  toolbar_ui_api::mojom::BackForwardControlStatePtr GetBackForwardState() const;

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

  // Classes that manage individual controls. They are responsible for informing
  // `this` when the state of the control changes. Though most are statically
  // declared here, they should not be active unless their Init() method is called
  // which is only done if their corresponding feature flag is enabled.
  WebUIReloadControl reload_control_;
  WebUISplitTabsControl split_tabs_control_;
  WebUIHomeControl home_control_;
  WebUIAvatarToolbarButton avatar_control_;
  std::unique_ptr<WebUILocationBar> location_bar_;
  WebUIBackForwardControl back_control_;
  WebUIBackForwardControl forward_control_;
  WebUIPinnedToolbarActions pinned_toolbar_actions_;

  raw_ptr<const base::TickClock> clock_;
  base::OnceClosure did_first_non_empty_paint_callback_;
  bool has_finished_first_non_empty_paint_ = false;

  // How many times the toolbar WebUI has crashed. Cleared after 10s has passed
  // without a crash.
  uint32_t crash_count_ = 0;

  base::TimeTicks last_crash_time_;

  base::CallbackListSubscription touch_ui_subscription_;

  // Extra space to put before the back button, which is the first button.
  int back_button_leading_margin_ = 0;

  // True if the WebContents was pre-warmed and injected.
  bool is_preloaded_ = false;

  // This WeakPtrFactory is used to keep tabs on pending state pushes, and then
  // used to cancel them if the state is later updated again before we post a
  // later PushNavigationState().
  base::WeakPtrFactory<WebUIToolbarWebView> state_push_weak_ptr_factory_{this};

  base::WeakPtrFactory<WebUIToolbarWebView> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_TOOLBAR_WEB_VIEW_H_
