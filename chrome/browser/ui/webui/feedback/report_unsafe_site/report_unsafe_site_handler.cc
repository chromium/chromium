// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/feedback/report_unsafe_site/report_unsafe_site_handler.h"

#include "base/strings/utf_string_conversions.h"
#include "components/url_formatter/elide_url.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/gfx/geometry/rect.h"

namespace {
void OnGotScreenshot(
    base::OnceCallback<void(const std::string&, const GURL&)> callback,
    std::string formatted_origin,
    const SkBitmap& screenshot) {
  GURL screenshot_data_uri(webui::GetBitmapDataUrl(screenshot));
  std::move(callback).Run(formatted_origin, screenshot_data_uri);
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

  std::u16string formatted_origin = url_formatter::FormatUrlForSecurityDisplay(
      triggering_web_contents_->GetURL());
  screenshot_taker_->SetCallback(
      base::BindOnce(&OnGotScreenshot, std::move(callback),
                     base::UTF16ToUTF8(formatted_origin)));
}

void ReportUnsafeSitePageHandler::CloseDialog() {
  if (embedder_) {
    embedder_->CloseUI();
  }
}
