// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/print_preview_ui_untrusted.h"

#include <memory>

#include "chrome/browser/ui/webui/print_preview/data_request_filter.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"

namespace printing {

PrintPreviewUIUntrustedConfig::PrintPreviewUIUntrustedConfig()
    : WebUIConfig(content::kChromeUIUntrustedScheme,
                  chrome::kChromeUIPrintHost) {}

PrintPreviewUIUntrustedConfig::~PrintPreviewUIUntrustedConfig() = default;

std::unique_ptr<content::WebUIController>
PrintPreviewUIUntrustedConfig::CreateWebUIController(content::WebUI* web_ui) {
  return std::make_unique<PrintPreviewUIUntrusted>(web_ui);
}

PrintPreviewUIUntrusted::PrintPreviewUIUntrusted(content::WebUI* web_ui)
    : UntrustedWebUIController(web_ui) {
  std::unique_ptr<content::WebUIDataSource> source(
      content::WebUIDataSource::Create(chrome::kChromeUIUntrustedPrintURL));
  AddDataRequestFilter(*source);
  content::WebUIDataSource::Add(web_ui->GetWebContents()->GetBrowserContext(),
                                source.release());
}

PrintPreviewUIUntrusted::~PrintPreviewUIUntrusted() = default;

}  // namespace printing
