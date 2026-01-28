// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/notimplemented.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_initialize.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/waap/initial_web_ui_manager.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/browser/ui/webui/webui_toolbar/webui_toolbar_ui.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/result_codes.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace {

constexpr char kHistogramToolbarRenderProcessGone[] =
    "InitialWebUI.Toolbar.RenderProcessGone";
constexpr char kHistogramToolbarRenderProcessGoneExceedingRecoveryLimit[] =
    "InitialWebUI.Toolbar.RenderProcessGoneExceedingRecoveryLimit";

class WebUIToolbarInternalWebView : public views::WebView {
  METADATA_HEADER(WebUIToolbarInternalWebView, views::WebView)

 public:
  explicit WebUIToolbarInternalWebView(content::BrowserContext* browser_context)
      : views::WebView(browser_context) {}
  ~WebUIToolbarInternalWebView() override = default;

  // views::WebView:
  void RendererUnresponsive(
      content::WebContents* source,
      content::RenderWidgetHost* render_widget_host,
      base::RepeatingClosure hang_monitor_restarter) override {
    // TODO(crbug.com/475397687): Consider using a more aggressive timeout to
    // trigger this.
    if (features::kWebUIReloadButtonRestartUnresponsive.Get()) {
      // Force shutting down the renderer process when the WebUI toolbar
      // is unresponsive. It will be restarted by the WebUIToolbarWebView.
      if (auto* process = render_widget_host->GetProcess()) {
        process->Shutdown(content::RESULT_CODE_KILLED);
      }
      return;
    }

    views::WebView::RendererUnresponsive(source, render_widget_host,
                                         std::move(hang_monitor_restarter));
  }
};

BEGIN_METADATA(WebUIToolbarInternalWebView)
END_METADATA

}  // namespace

WebUIToolbarWebView::WebUIToolbarWebView(
    BrowserWindowInterface* browser,
    chrome::BrowserCommandController* controller)
    : browser_(browser), controller_(controller), reload_control_(this) {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  auto web_view =
      std::make_unique<WebUIToolbarInternalWebView>(browser->GetProfile());
  auto* web_contents =
      web_view->GetWebContents(GURL(chrome::kChromeUIWebUIToolbarURL));
  // PLM has to be initialized before loading the URL.
  InitializePageLoadMetricsForWebContents(web_contents);

  const int size = GetLayoutConstant(LayoutConstant::kToolbarButtonHeight);
  web_view->SetPreferredSize(gfx::Size(size, size));
  web_contents->SetPageBaseBackgroundColor(SK_ColorTRANSPARENT);
  web_view->SetID(VIEW_ID_RELOAD_BUTTON);

  // We must save the pointer to the WebView so we can load the URL after the
  // view is added to a widget.
  web_view_ = AddChildView(std::move(web_view));
  web_contents->SetDelegate(this);
  Observe(web_contents);

  // The accessibility and tooltip attributes are handled by the WebUI.
  SetProperty(views::kElementIdentifierKey, kReloadButtonElementId);
}

WebUIToolbarWebView::~WebUIToolbarWebView() = default;

void WebUIToolbarWebView::AddedToWidget() {
  CHECK(web_view_);
  if (reload_control_.is_initialized()) {
    return;
  }

  // Ensure the browser window interface is associated with the WebContents
  // before the WebUI acts on it.
  webui::SetBrowserWindowInterface(web_view_->GetWebContents(), browser_);
  web_view_->LoadInitialURL(GURL(chrome::kChromeUIWebUIToolbarURL));
  reload_control_.Init();
}

bool WebUIToolbarWebView::HandleContextMenu(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params) {
  gfx::Point screen_location = GetBoundsInScreen().origin();
  screen_location.Offset(params.x, params.y);

  // TODO(crbug.com/470955454): Dispatch context menu based on which context
  // menu was triggered.
  return reload_control_.HandleContextMenu(GetWidget(), screen_location,
                                           params);
}

void WebUIToolbarWebView::RendererUnresponsive(
    content::WebContents* source,
    content::RenderWidgetHost* render_widget_host,
    base::RepeatingClosure hang_monitor_restarter) {
  web_view_->RendererUnresponsive(source, render_widget_host,
                                  std::move(hang_monitor_restarter));
}

void WebUIToolbarWebView::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  InitialWebUIManager::From(browser_)->OnWebUIToolbarLoaded();
}

ReloadControl* WebUIToolbarWebView::GetReloadControl() {
  return &reload_control_;
}

void WebUIToolbarWebView::DidFirstVisuallyNonEmptyPaint() {
  has_finished_first_non_empty_paint_ = true;
  if (did_first_non_empty_paint_callback_) {
    std::move(did_first_non_empty_paint_callback_).Run();
  }
}

void WebUIToolbarWebView::PrimaryMainFrameRenderProcessGone(
    base::TerminationStatus status) {
  // TODO(crbug.com/474228715): update the enum definition from
  // //base/process/kill.h to be a enum class, and add IFTTT lint.
  base::UmaHistogramEnumeration(kHistogramToolbarRenderProcessGone, status,
                                base::TERMINATION_STATUS_MAX_ENUM);

  // No recovery if it's a normal termination.
  if (status == base::TERMINATION_STATUS_NORMAL_TERMINATION ||
      status == base::TERMINATION_STATUS_STILL_RUNNING) {
    return;
  }

  // Reset the crash count if when the reset interval is reached.
  if (base::TimeTicks::Now() - last_crash_time_ >=
      features::kWebUIReloadButtonCrashRecoverResetInterval.Get()) {
    crash_count_ = 0;
  }
  last_crash_time_ = base::TimeTicks::Now();

  if (++crash_count_ <=
      base::checked_cast<uint32_t>(
          features::kWebUIReloadButtonMaxCrashRecoveryTimes.Get())) {
    // TODO(crbug.com/474228715): keep the previous rendered pixels when the
    // WebUI toolbar crashed so there is no visual glitch during recovery.

    // PostTask to avoid re-entrancy into RenderProcessHost during its death.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&WebUIToolbarWebView::ReloadWebContents,
                                  weak_ptr_factory_.GetWeakPtr()));
  } else {
    // TODO(crbug.com/474228715): if the crash_count exceeds the threshold, we
    // should consider fall back to the C++ view or start a periodic attempt to
    // recover.
    base::UmaHistogramBoolean(
        kHistogramToolbarRenderProcessGoneExceedingRecoveryLimit, true);
  }
}

void WebUIToolbarWebView::ReloadWebContents() {
  CHECK(web_view_);
  web_view_->web_contents()->GetController().Reload(content::ReloadType::NORMAL,
                                                    /*check_for_repost=*/false);
}

void WebUIToolbarWebView::SetDidFirstNonEmptyPaintCallbackForTesting(
    base::OnceClosure callback) {
  if (callback.is_null()) {
    return;
  }
  if (has_finished_first_non_empty_paint_) {
    std::move(callback).Run();
    return;
  }
  did_first_non_empty_paint_callback_ = std::move(callback);
}

WebUIToolbarUI* WebUIToolbarWebView::GetWebUIToolbarUI() {
  return web_view_->GetWebContents()
      ->GetWebUI()
      ->GetController()
      ->GetAs<WebUIToolbarUI>();
}

BEGIN_METADATA(WebUIToolbarWebView)
END_METADATA
