// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_TOOLBAR_WEB_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_TOOLBAR_WEB_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ui/browser_command_controller.h"
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

namespace views {
class WebView;
}  // namespace views

// A view that displays the toolbar as a WebView.
class WebUIToolbarWebView
    : public views::View,
      public content::WebContentsObserver,
      public toolbar_ui_api::ToolbarUIService::ToolbarUIServiceDelegate,
      public browser_controls_api::BrowserControlsService::
          BrowserControlsServiceDelegate,
      public WebUIToolbarUI::DependencyProvider {
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

  // May be nullptr.
  WebUILocationBar* GetLocationBar() { return location_bar_.get(); }

  // WebUIToolbarUI::DependencyProvider:
  browser_controls_api::BrowserControlsService::BrowserControlsServiceDelegate*
  GetBrowserControlsDelegate() override;
  toolbar_ui_api::ToolbarUIService::ToolbarUIServiceDelegate*
  GetToolbarUIServiceDelegate() override;
  std::unique_ptr<toolbar_ui_api::NavigationControlsStateFetcher>
  GetNavigationControlsStateFetcher() override;

  // ToolbarUIService::ToolbarUIServiceDelegate:
  void HandleContextMenu(toolbar_ui_api::mojom::ContextMenuType menu_type,
                         gfx::Point viewport_coordinate_css_pixels,
                         ui::mojom::MenuSourceType source) override;
  void OnPageInitialized() override;

  // BrowserControlsService::BrowserControlsServiceDelegate:
  void PermitLaunchUrl() override;

  // views::View:
  void AddedToWidget() override;
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
  views::WebView* GetWebViewForTesting() { return web_view_; }
  bool IsPendingForTesting() const {
    return initialization_state_ == InitializationState::kPending;
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(WebUIToolbarWebViewPixelBrowserTest,
                           CheckSplitTabsButtonColor);
  FRIEND_TEST_ALL_PREFIXES(WebUIToolbarWebViewSplitTabsBrowserTest,
                           CheckSplitTabsButtonSourceType);
  friend WebUIReloadControl;
  friend WebUISplitTabsControl;

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

  chrome::BrowserCommandController* controller() { return controller_; }
  WebUIToolbarUI* GetWebUIToolbarUI();

  // Called by friended controls to push state.
  void OnReloadControlStateChanged(
      toolbar_ui_api::mojom::ReloadControlStatePtr state);
  void OnSplitTabsControlStateChanged(
      toolbar_ui_api::mojom::SplitTabsControlStatePtr state);

  void OnTouchUiChanged();
  void PostPushNavigationState();
  void PushNavigationState(uint64_t state_generation);
  toolbar_ui_api::mojom::NavigationControlsState last_queued_state_;
  uint64_t current_state_generation_ = 0;

  InitializationState initialization_state_ =
      InitializationState::kUninitialized;

  raw_ptr<views::WebView> web_view_ = nullptr;
  const raw_ptr<BrowserWindowInterface> browser_;
  const raw_ptr<chrome::BrowserCommandController> controller_;
  WebUIReloadControl reload_control_;
  WebUISplitTabsControl split_tabs_control_;
  std::unique_ptr<WebUILocationBar> location_bar_;
  raw_ptr<const base::TickClock> clock_;
  base::OnceClosure did_first_non_empty_paint_callback_;
  bool has_finished_first_non_empty_paint_ = false;
  uint32_t crash_count_ = 0;
  base::TimeTicks last_crash_time_;
  base::CallbackListSubscription touch_ui_subscription_;

  base::WeakPtrFactory<WebUIToolbarWebView> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_TOOLBAR_WEB_VIEW_H_
