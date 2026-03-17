// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_FEEDBACK_REPORT_UNSAFE_SITE_REPORT_UNSAFE_SITE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_FEEDBACK_REPORT_UNSAFE_SITE_REPORT_UNSAFE_SITE_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/feedback/screenshot_taker.h"
#include "chrome/browser/ui/webui/feedback/report_unsafe_site/report_unsafe_site.mojom.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}  // namespace content

// WebUI page handler for the chrome://feedback/report-unsafe-site.
class ReportUnsafeSitePageHandler
    : public feedback::report_unsafe_site::mojom::PageHandler {
 public:
  typedef feedback::report_unsafe_site::mojom::PageHandler::
      GetTriggeringPageInfoCallback GetTriggeringPageInfoCallback;

  ReportUnsafeSitePageHandler(
      base::WeakPtr<TopChromeWebUIController::Embedder> embedder,
      base::WeakPtr<content::WebContents> triggering_web_contents,
      std::unique_ptr<feedback::ScreenshotTaker> screenshot_taker,
      mojo::PendingReceiver<feedback::report_unsafe_site::mojom::PageHandler>
          receiver);

  ReportUnsafeSitePageHandler(const ReportUnsafeSitePageHandler&) = delete;
  ReportUnsafeSitePageHandler& operator=(const ReportUnsafeSitePageHandler&) =
      delete;

  ~ReportUnsafeSitePageHandler() override;

  // report_unsafe_site::mojom::PageHandler:
  void GetTriggeringPageInfo(GetTriggeringPageInfoCallback callback) override;
  void SendReport(bool include_screenshot,
                  SendReportCallback callback) override;
  void CloseDialog() override;

 private:
  const base::WeakPtr<TopChromeWebUIController::Embedder> embedder_;
  const base::WeakPtr<content::WebContents> triggering_web_contents_;
  std::unique_ptr<feedback::ScreenshotTaker> screenshot_taker_;
  const mojo::Receiver<feedback::report_unsafe_site::mojom::PageHandler>
      receiver_;

  void OnGotScreenshot(
      base::OnceCallback<void(const std::string&, const GURL&)> callback,
      const SkBitmap& screenshot);

  // Last committed URL.
  GURL page_url_;
  SkBitmap screenshot_;

  base::WeakPtrFactory<ReportUnsafeSitePageHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_FEEDBACK_REPORT_UNSAFE_SITE_REPORT_UNSAFE_SITE_HANDLER_H_
