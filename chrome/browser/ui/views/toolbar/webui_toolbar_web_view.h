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
#include "chrome/browser/ui/webui/webui_toolbar/browser_controls_service.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

class WebUIToolbarUI;
class BrowserWindowInterface;

namespace views {
class WebView;
}  // namespace views

// A view that displays the toolbar as a WebView.
class WebUIToolbarWebView
    : public views::View,
      public content::WebContentsObserver,
      public BrowserControlsService::BrowserControlsServiceDelegate {
  METADATA_HEADER(WebUIToolbarWebView, views::View)

 public:
  WebUIToolbarWebView(BrowserWindowInterface* browser,
                      chrome::BrowserCommandController* controller);
  WebUIToolbarWebView(const WebUIToolbarWebView&) = delete;
  WebUIToolbarWebView& operator=(const WebUIToolbarWebView&) = delete;
  ~WebUIToolbarWebView() override;

  ReloadControl* GetReloadControl();

  // BrowserControlsService::BrowserControlsServiceDelegate:
  void HandleContextMenu(browser_controls_api::mojom::ContextMenuType menu_type,
                         gfx::Point viewport_coordinate_css_pixels,
                         ui::mojom::MenuSourceType source) override;
  void OnPageInitialized() override;
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
  friend WebUIReloadControl;
  friend WebUISplitTabsControl;

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

  InitializationState initialization_state_ =
      InitializationState::kUninitialized;

  raw_ptr<views::WebView> web_view_ = nullptr;
  const raw_ptr<BrowserWindowInterface> browser_;
  const raw_ptr<chrome::BrowserCommandController> controller_;
  WebUIReloadControl reload_control_;
  WebUISplitTabsControl split_tabs_control_;
  raw_ptr<const base::TickClock> clock_;
  base::OnceClosure did_first_non_empty_paint_callback_;
  bool has_finished_first_non_empty_paint_ = false;
  uint32_t crash_count_ = 0;
  base::TimeTicks last_crash_time_;

  base::WeakPtrFactory<WebUIToolbarWebView> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_TOOLBAR_WEB_VIEW_H_
