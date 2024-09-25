// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/signin_email_confirmation_ui.h"

#include <string>

#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/constrained_web_dialog_ui.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/signin_resources.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/webui/resource_path.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/web_dialogs/web_dialog_delegate.h"

bool SigninEmailConfirmationUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  return !profile->IsOffTheRecord();
}

SigninEmailConfirmationUI::SigninEmailConfirmationUI(content::WebUI* web_ui)
    : ConstrainedWebDialogUI(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);

  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUISigninEmailConfirmationHost);
  webui::EnableTrustedTypesCSP(source);
  source->UseStringsJs();
  source->EnableReplaceI18nInJS();
  source->SetDefaultResource(
      IDR_SIGNIN_SIGNIN_EMAIL_CONFIRMATION_SIGNIN_EMAIL_CONFIRMATION_HTML);

  static constexpr webui::ResourcePath kResources[] = {
      {"signin_email_confirmation_app.js",
       IDR_SIGNIN_SIGNIN_EMAIL_CONFIRMATION_SIGNIN_EMAIL_CONFIRMATION_APP_JS},
      {"signin_email_confirmation_app.css.js",
       IDR_SIGNIN_SIGNIN_EMAIL_CONFIRMATION_SIGNIN_EMAIL_CONFIRMATION_APP_CSS_JS},
      {"signin_email_confirmation_app.html.js",
       IDR_SIGNIN_SIGNIN_EMAIL_CONFIRMATION_SIGNIN_EMAIL_CONFIRMATION_APP_HTML_JS},
      {"signin_shared.css.js", IDR_SIGNIN_SIGNIN_SHARED_CSS_JS},
      {"signin_vars.css.js", IDR_SIGNIN_SIGNIN_VARS_CSS_JS},
  };
  source->AddResourcePaths(kResources);

  static constexpr webui::LocalizedString kStrings[] = {
      {"signinEmailConfirmationTitle", IDS_SIGNIN_EMAIL_CONFIRMATION_TITLE},
      {"signinEmailConfirmationCreateProfileButtonTitle",
       IDS_SIGNIN_EMAIL_CONFIRMATION_CREATE_PROFILE_RADIO_BUTTON_TITLE},
      {"signinEmailConfirmationCreateProfileButtonSubtitle",
       IDS_SIGNIN_EMAIL_CONFIRMATION_CREATE_PROFILE_RADIO_BUTTON_SUBTITLE},
      {"signinEmailConfirmationStartSyncButtonTitle",
       IDS_SIGNIN_EMAIL_CONFIRMATION_START_SYNC_RADIO_BUTTON_TITLE},
      {"signinEmailConfirmationStartSyncButtonSubtitle",
       IDS_SIGNIN_EMAIL_CONFIRMATION_START_SYNC_RADIO_BUTTON_SUBTITLE},
      {"signinEmailConfirmationConfirmLabel",
       IDS_SIGNIN_EMAIL_CONFIRMATION_CONFIRM_BUTTON_LABEL},
      {"signinEmailConfirmationCloseLabel",
       IDS_SIGNIN_EMAIL_CONFIRMATION_CLOSE_BUTTON_LABEL},
  };
  source->AddLocalizedStrings(kStrings);

  base::Value::Dict strings;
  webui::SetLoadTimeDataDefaults(g_browser_process->GetApplicationLocale(),
                                 &strings);
  source->AddLocalizedStrings(strings);
}

SigninEmailConfirmationUI::~SigninEmailConfirmationUI() {}

void SigninEmailConfirmationUI::Close() {
  ConstrainedWebDialogDelegate* delegate = GetConstrainedDelegate();
  if (delegate) {
    delegate->GetWebDialogDelegate()->OnDialogClosed(std::string());
    delegate->OnDialogCloseFromWebUI();
  }
}
