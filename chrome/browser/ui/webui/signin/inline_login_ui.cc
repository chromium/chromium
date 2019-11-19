// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/inline_login_ui.h"

#include <memory>

#include "base/bind.h"
#include "base/command_line.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/ui/webui/metrics_handler.h"
#include "chrome/browser/ui/webui/test_files_request_filter.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/content_switches.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/ui/webui/signin/inline_login_handler_chromeos.h"
#else
#include "chrome/browser/ui/webui/signin/inline_login_handler_impl.h"
#endif  // defined(OS_CHROMEOS)

namespace {

content::WebUIDataSource* CreateWebUIDataSource() {
  content::WebUIDataSource* source =
        content::WebUIDataSource::Create(chrome::kChromeUIChromeSigninHost);
  source->OverrideContentSecurityPolicyObjectSrc("object-src chrome:;");
  source->UseStringsJs();

  source->SetDefaultResource(IDR_INLINE_LOGIN_HTML);

  // Only add a filter when runing as test.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  const bool is_running_test = command_line->HasSwitch(::switches::kTestName) ||
                               command_line->HasSwitch(::switches::kTestType);
  if (is_running_test) {
    source->SetRequestFilter(test::GetTestShouldHandleRequest(),
                             test::GetTestFilesRequestFilter());
  }

  source->AddResourcePath("inline_login.css", IDR_INLINE_LOGIN_CSS);
  source->AddResourcePath("inline_login.js", IDR_INLINE_LOGIN_JS);
  source->AddResourcePath("gaia_auth_host.js", IDR_GAIA_AUTH_AUTHENTICATOR_JS);

  source->AddLocalizedString("title", IDS_CHROME_SIGNIN_TITLE);
  source->AddLocalizedString(
      "accessibleCloseButtonLabel", IDS_SIGNIN_ACCESSIBLE_CLOSE_BUTTON);
  source->AddLocalizedString(
      "accessibleBackButtonLabel", IDS_SIGNIN_ACCESSIBLE_BACK_BUTTON);
  return source;
}

// Returns whether |url| can be displayed in a chrome://chrome-signin tab,
// depending on the signin reason that is encoded in the url.
bool IsValidChromeSigninReason(const GURL& url) {
#if defined(OS_CHROMEOS)
  return true;
#else
  signin_metrics::Reason reason =
      signin::GetSigninReasonForEmbeddedPromoURL(url);

  switch (reason) {
    case signin_metrics::Reason::REASON_FORCED_SIGNIN_PRIMARY_ACCOUNT:
    case signin_metrics::Reason::REASON_UNLOCK:
      // Used by the user manager.
      return true;
    case signin_metrics::Reason::REASON_FETCH_LST_ONLY:
#if defined(OS_WIN)
      // Used by the Google Credential Provider for Windows.
      return true;
#else
      return false;
#endif
    case signin_metrics::Reason::REASON_SIGNIN_PRIMARY_ACCOUNT:
    case signin_metrics::Reason::REASON_ADD_SECONDARY_ACCOUNT:
    case signin_metrics::Reason::REASON_REAUTHENTICATION:
    case signin_metrics::Reason::REASON_UNKNOWN_REASON:
      return false;
    case signin_metrics::Reason::REASON_MAX:
      NOTREACHED();
      return false;
  }
  NOTREACHED();
#endif  // defined(OS_CHROMEOS)
}

}  // namespace

InlineLoginUI::InlineLoginUI(content::WebUI* web_ui) : WebDialogUI(web_ui) {
  if (!IsValidChromeSigninReason(web_ui->GetWebContents()->GetVisibleURL()))
    return;

  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, CreateWebUIDataSource());

#if defined(OS_CHROMEOS)
  web_ui->AddMessageHandler(
      std::make_unique<chromeos::InlineLoginHandlerChromeOS>(
          base::BindRepeating(&WebDialogUIBase::CloseDialog,
                              weak_factory_.GetWeakPtr(), nullptr /* args */)));
#else
  web_ui->AddMessageHandler(std::make_unique<InlineLoginHandlerImpl>());
#endif  // defined(OS_CHROMEOS)

  web_ui->AddMessageHandler(std::make_unique<MetricsHandler>());

  content::WebContents* contents = web_ui->GetWebContents();
  // Required for intercepting extension function calls when the page is loaded
  // in a bubble (not a full tab, thus tab helpers are not registered
  // automatically).
  extensions::ChromeExtensionWebContentsObserver::CreateForWebContents(
      contents);
  extensions::TabHelper::CreateForWebContents(contents);
  // Ensure that the login UI has a tab ID, which will allow the GAIA auth
  // extension's background script to tell it apart from iframes injected by
  // other extensions.
  SessionTabHelper::CreateForWebContents(contents);
}

InlineLoginUI::~InlineLoginUI() {}
