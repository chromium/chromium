// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/feedback/report_unsafe_site/report_unsafe_site_handler.h"

#include "base/strings/utf_string_conversions.h"
#include "components/safe_browsing/content/browser/client_side_detection_host.h"
#include "components/safe_browsing/content/browser/safe_browsing_tab_observer.h"
#include "components/url_formatter/elide_url.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/gfx/geometry/rect.h"

namespace {
safe_browsing::ClientSideDetectionHost* GetClientSideDetectionHost(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return nullptr;
  }

  auto* observer =
      safe_browsing::SafeBrowsingTabObserver::FromWebContents(web_contents);
  if (!observer) {
    return nullptr;
  }
  return observer->client_side_detection_host();
}

}  // anonymous namespace

ReportUnsafeSitePageHandler::ReportUnsafeSitePageHandler(
    base::WeakPtr<TopChromeWebUIController::Embedder> embedder,
    base::WeakPtr<content::WebContents> triggering_web_contents,
    std::unique_ptr<feedback::ScreenshotTaker> screenshot_taker,
    mojo::PendingReceiver<feedback::report_unsafe_site::mojom::PageHandler>
        receiver)
    : embedder_(embedder),
      triggering_web_contents_(triggering_web_contents),
      screenshot_taker_(std::move(screenshot_taker)),
      receiver_(this, std::move(receiver)) {}

ReportUnsafeSitePageHandler::~ReportUnsafeSitePageHandler() = default;

void ReportUnsafeSitePageHandler::GetTriggeringPageInfo(
    GetTriggeringPageInfoCallback callback) {
  if (!triggering_web_contents_ || !screenshot_taker_) {
    std::move(callback).Run("", GURL());
    return;
  }

  page_url_ = triggering_web_contents_->GetLastCommittedURL();
  if (!page_url_.is_valid()) {
    std::move(callback).Run("", GURL());
    return;
  }

  screenshot_taker_->SetCallback(
      base::BindOnce(&ReportUnsafeSitePageHandler::OnGotScreenshot,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ReportUnsafeSitePageHandler::SendReport(bool include_screenshot,
                                             SendReportCallback callback) {
  // Dialog is tab modal and thus should close if the underlying page navigates
  // while the dialog is shown.
  if (!page_url_.is_valid() ||
      page_url_ != triggering_web_contents_->GetLastCommittedURL()) {
    std::move(callback).Run();
    return;
  }
  auto* client_side_detection_host =
      GetClientSideDetectionHost(triggering_web_contents_.get());
  if (!client_side_detection_host) {
    std::move(callback).Run();
    return;
  }
  if (!include_screenshot) {
    screenshot_ = SkBitmap();
  }
  client_side_detection_host->ReportUnsafeSite(screenshot_);
  std::move(callback).Run();
}

void ReportUnsafeSitePageHandler::CloseDialog() {
  if (embedder_) {
    embedder_->CloseUI();
  }
}

void ReportUnsafeSitePageHandler::OnGotScreenshot(
    base::OnceCallback<void(const std::string&, const GURL&)> callback,
    const SkBitmap& screenshot) {
  screenshot_ = screenshot;

  std::string formatted_origin =
      base::UTF16ToUTF8(url_formatter::FormatUrlForSecurityDisplay(page_url_));
  GURL screenshot_data_uri(webui::GetBitmapDataUrl(screenshot));
  std::move(callback).Run(formatted_origin, screenshot_data_uri);
}
