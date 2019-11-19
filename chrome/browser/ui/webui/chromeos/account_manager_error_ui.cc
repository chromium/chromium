// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/account_manager_error_ui.h"

#include "base/bind.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/strings/grit/ui_strings.h"

namespace chromeos {

AccountManagerErrorUI::AccountManagerErrorUI(content::WebUI* web_ui)
    : ui::WebDialogUI(web_ui), weak_factory_(this) {
  content::WebUIDataSource* html_source = content::WebUIDataSource::Create(
      chrome::kChromeUIAccountManagerErrorHost);

  web_ui->RegisterMessageCallback(
      "closeDialog", base::BindRepeating(&WebDialogUI::CloseDialog,
                                         weak_factory_.GetWeakPtr()));

  html_source->UseStringsJs();

  html_source->AddLocalizedString(
      "errorTitle", IDS_ACCOUNT_MANAGER_SECONDARY_ACCOUNTS_DISABLED_TITLE);
  html_source->AddLocalizedString(
      "errorMessage", IDS_ACCOUNT_MANAGER_SECONDARY_ACCOUNTS_DISABLED_TEXT);
  html_source->AddLocalizedString("okButton", IDS_APP_OK);
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  html_source->AddResourcePath("googleg.svg",
                               IDR_ACCOUNT_MANAGER_WELCOME_GOOGLE_LOGO_SVG);
#endif

  // Add required resources.
  html_source->AddResourcePath("account_manager_shared.css",
                               IDR_ACCOUNT_MANAGER_SHARED_CSS);
  html_source->AddResourcePath("account_manager_error.js",
                               IDR_ACCOUNT_MANAGER_ERROR_JS);

  html_source->SetDefaultResource(IDR_ACCOUNT_MANAGER_ERROR_HTML);

  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, html_source);
}

AccountManagerErrorUI::~AccountManagerErrorUI() = default;

}  // namespace chromeos
