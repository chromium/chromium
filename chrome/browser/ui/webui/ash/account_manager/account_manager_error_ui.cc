// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/account_manager/account_manager_error_ui.h"

#include "base/functional/bind.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/strings/grit/ui_strings.h"

namespace ash {

AccountManagerErrorUI::AccountManagerErrorUI(content::WebUI* web_ui)
    : ui::WebDialogUI(web_ui), weak_factory_(this) {
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::CreateAndAdd(
          Profile::FromWebUI(web_ui), chrome::kChromeUIAccountManagerErrorHost);
  webui::EnableTrustedTypesCSP(html_source);

  web_ui->RegisterMessageCallback(
      "closeDialog", base::BindRepeating(&WebDialogUI::CloseDialog,
                                         weak_factory_.GetWeakPtr()));

  html_source->UseStringsJs();
  html_source->EnableReplaceI18nInJS();

  html_source->AddLocalizedString(
      "secondaryAccountsDisabledErrorTitle",
      IDS_ACCOUNT_MANAGER_SECONDARY_ACCOUNTS_DISABLED_TITLE);
  html_source->AddLocalizedString(
      "secondaryAccountsDisabledErrorMessage",
      IDS_ACCOUNT_MANAGER_SECONDARY_ACCOUNTS_DISABLED_TEXT);

  html_source->AddLocalizedString("okButton", IDS_APP_OK);

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  html_source->AddResourcePath("googleg.svg",
                               IDR_ACCOUNT_MANAGER_WELCOME_GOOGLE_LOGO_SVG);
#endif

  // Add required resources.
  html_source->AddResourcePath("account_manager_shared.css.js",
                               IDR_ACCOUNT_MANAGER_SHARED_CSS_JS);
  html_source->AddResourcePath("account_manager_browser_proxy.js",
                               IDR_ACCOUNT_MANAGER_BROWSER_PROXY_JS);
  html_source->AddResourcePath("account_manager_error_app.js",
                               IDR_ACCOUNT_MANAGER_ERROR_APP_JS);
  html_source->AddResourcePath("account_manager_error_app.html.js",
                               IDR_ACCOUNT_MANAGER_ERROR_APP_HTML_JS);

  html_source->SetDefaultResource(IDR_ACCOUNT_MANAGER_ERROR_HTML);
}

AccountManagerErrorUI::~AccountManagerErrorUI() = default;

}  // namespace ash
