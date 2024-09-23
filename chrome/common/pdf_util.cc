// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/pdf_util.h"

#include "chrome/grit/renderer_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/jstemplate_builder.h"
#include "ui/base/webui/web_ui_util.h"
#include "url/gurl.h"

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
