// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_FEEDBACK_REPORT_UNSAFE_SITE_REPORT_UNSAFE_SITE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_FEEDBACK_REPORT_UNSAFE_SITE_REPORT_UNSAFE_SITE_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/feedback/report_unsafe_site/report_unsafe_site.mojom.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace content {
class WebContents;
}  // namespace content

// WebUI page handler for the chrome://feedback/report-unsafe-site.
class ReportUnsafeSitePageHandler
    : public feedback::report_unsafe_site::mojom::PageHandler {
 public:
  ReportUnsafeSitePageHandler(
      base::WeakPtr<TopChromeWebUIController::Embedder> embedder,
      base::WeakPtr<content::WebContents> triggering_web_contents,
      mojo::PendingReceiver<feedback::report_unsafe_site::mojom::PageHandler>
          receiver);

  ReportUnsafeSitePageHandler(const ReportUnsafeSitePageHandler&) = delete;
  ReportUnsafeSitePageHandler& operator=(const ReportUnsafeSitePageHandler&) =
      delete;

  ~ReportUnsafeSitePageHandler() override;

  // report_unsafe_site::mojom::PageHandler:
  void GetPageUrl(
      feedback::report_unsafe_site::mojom::PageHandler::GetPageUrlCallback
          callback) override;
  void CloseDialog() override;

 private:
  const base::WeakPtr<TopChromeWebUIController::Embedder> embedder_;
  const base::WeakPtr<content::WebContents> triggering_web_contents_;
  const mojo::Receiver<feedback::report_unsafe_site::mojom::PageHandler>
      receiver_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_FEEDBACK_REPORT_UNSAFE_SITE_REPORT_UNSAFE_SITE_HANDLER_H_
