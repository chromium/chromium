// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/eche_app_ui/eche_app_ui.h"

#include <memory>

#include "chromeos/components/eche_app_ui/url_constants.h"
#include "chromeos/grit/chromeos_eche_app_resources.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

namespace chromeos {

EcheAppUI::EcheAppUI(content::WebUI* web_ui) : ui::MojoWebUIController(web_ui) {
  auto html_source =
      base::WrapUnique(content::WebUIDataSource::Create(kChromeUIEcheAppHost));

  html_source->AddResourcePath("", IDR_CHROMEOS_ECHE_APP_INDEX_HTML);
  html_source->AddResourcePath("app.js", IDR_CHROMEOS_ECHE_APP_APP_JS);
  html_source->AddResourcePath("app.css", IDR_CHROMEOS_ECHE_APP_APP_CSS);

  content::WebUIDataSource::Add(web_ui->GetWebContents()->GetBrowserContext(),
                                html_source.release());
}

EcheAppUI::~EcheAppUI() = default;

}  // namespace chromeos
