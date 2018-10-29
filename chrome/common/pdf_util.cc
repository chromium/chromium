// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/pdf_util.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/grit/renderer_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/jstemplate_builder.h"
#include "ui/base/webui/web_ui_util.h"
#include "url/gurl.h"

void ReportPDFLoadStatus(PDFLoadStatus status) {
  UMA_HISTOGRAM_ENUMERATION("PDF.LoadStatus", status,
                            PDFLoadStatus::kPdfLoadStatusCount);
}

std::string GetPDFPlaceholderHTML(const GURL& pdf_url) {
  std::string template_html = ui::ResourceBundle::GetSharedInstance()
                                  .GetRawDataResource(IDR_PDF_PLUGIN_HTML)
                                  .as_string();
  webui::AppendWebUiCssTextDefaults(&template_html);

  base::DictionaryValue values;
  values.SetString("fileName", pdf_url.ExtractFileName());
  values.SetString("open", l10n_util::GetStringUTF8(IDS_ACCNAME_OPEN));
  values.SetString("pdfUrl", pdf_url.spec());

  return webui::GetI18nTemplateHtml(template_html, &values);
}
