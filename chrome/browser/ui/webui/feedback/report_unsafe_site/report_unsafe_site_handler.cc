// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/feedback/report_unsafe_site/report_unsafe_site_handler.h"

#include "base/strings/utf_string_conversions.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/web_contents.h"

ReportUnsafeSitePageHandler::ReportUnsafeSitePageHandler(
    base::WeakPtr<TopChromeWebUIController::Embedder> embedder,
    base::WeakPtr<content::WebContents> triggering_web_contents,
    mojo::PendingReceiver<feedback::report_unsafe_site::mojom::PageHandler>
        receiver)
    : embedder_(embedder),
      triggering_web_contents_(triggering_web_contents),
      receiver_(this, std::move(receiver)) {}

ReportUnsafeSitePageHandler::~ReportUnsafeSitePageHandler() = default;

void ReportUnsafeSitePageHandler::GetPageUrl(
    feedback::report_unsafe_site::mojom::PageHandler::GetPageUrlCallback
        callback) {
  if (!triggering_web_contents_) {
    std::move(callback).Run("");
    return;
  }

  std::u16string formatted_origin = url_formatter::FormatUrlForSecurityDisplay(
      triggering_web_contents_->GetURL());
  std::move(callback).Run(base::UTF16ToUTF8(formatted_origin));
}

void ReportUnsafeSitePageHandler::CloseDialog() {
  if (embedder_) {
    embedder_->CloseUI();
  }
}
