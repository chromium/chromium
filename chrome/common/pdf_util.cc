// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/pdf_util.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/renderer_resources.h"
#include "components/strings/grit/components_strings.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/jstemplate_builder.h"
#include "ui/base/webui/web_ui_util.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/constants.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

void ReportPDFLoadStatus(PDFLoadStatus status) {
  UMA_HISTOGRAM_ENUMERATION("PDF.LoadStatus", status,
                            PDFLoadStatus::kPdfLoadStatusCount);
}

std::string GetPDFPlaceholderHTML(const GURL& pdf_url) {
  std::string template_html =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_PDF_PLUGIN_HTML);
  webui::AppendWebUiCssTextDefaults(&template_html);

  base::Value::Dict values;
  values.Set("fileName", pdf_url.ExtractFileName());
  values.Set("open", l10n_util::GetStringUTF8(IDS_ACCNAME_OPEN));
  values.Set("pdfUrl", pdf_url.spec());

  return webui::GetI18nTemplateHtml(template_html, values);
}

bool IsPdfExtensionOrigin(const url::Origin& origin) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  return origin.scheme() == extensions::kExtensionScheme &&
         origin.host() == extension_misc::kPdfExtensionId;
#else
  return false;
#endif
}

bool IsPdfInternalPluginAllowedOrigin(const url::Origin& origin) {
  if (IsPdfExtensionOrigin(origin))
    return true;

  // Allow embedding the internal PDF plugin in chrome://print.
  if (origin == url::Origin::Create(GURL(chrome::kChromeUIPrintURL)))
    return true;

  // Only allow the PDF plugin in the known, trustworthy origins that are
  // allowlisted above.  See also https://crbug.com/520422 and
  // https://crbug.com/1027173.
  return false;
}
