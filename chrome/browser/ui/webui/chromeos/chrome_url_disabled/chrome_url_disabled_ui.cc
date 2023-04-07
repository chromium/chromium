// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/chrome_url_disabled/chrome_url_disabled_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui_data_source.h"

namespace chromeos {

ChromeURLDisabledUI::ChromeURLDisabledUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui), weak_factory_(this) {
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::CreateAndAdd(Profile::FromWebUI(web_ui),
                                             chrome::kChromeUIAppDisabledHost);

  html_source->UseStringsJs();

  html_source->AddLocalizedString("disabledPageHeader",
                                  IDS_CHROME_URLS_DISABLED_PAGE_HEADER);
  html_source->AddLocalizedString("disabledPageTitle",
                                  IDS_CHROME_URLS_DISABLED_PAGE_TITLE);
  html_source->AddLocalizedString("disabledPageMessage",
                                  IDS_CHROME_URLS_DISABLED_PAGE_MESSAGE);
  html_source->SetDefaultResource(IDR_CHROME_URLS_DISABLED_PAGE_HTML);

  html_source->SetDefaultResource(IDR_CHROME_URLS_DISABLED_PAGE_HTML);
}

ChromeURLDisabledUI::~ChromeURLDisabledUI() = default;

}  // namespace chromeos
