// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/content/connection_help_ui.h"

#include "build/build_config.h"
#include "components/grit/components_resources.h"
#include "components/security_interstitials/content/urls.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "net/base/net_errors.h"
#include "ui/base/l10n/l10n_util.h"

namespace security_interstitials {

ConnectionHelpUI::ConnectionHelpUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::CreateAndAdd(
          web_ui->GetWebContents()->GetBrowserContext(),
          kChromeUIConnectionHelpHost);

  // JS code needs these constants to decide which section to expand.
  html_source->AddInteger("certCommonNameInvalid",
                          net::ERR_CERT_COMMON_NAME_INVALID);
  html_source->AddInteger("certExpired", net::ERR_CERT_DATE_INVALID);
  html_source->AddInteger("certAuthorityInvalid",
                          net::ERR_CERT_AUTHORITY_INVALID);
  html_source->AddInteger("certWeakSignatureAlgorithm",
                          net::ERR_CERT_WEAK_SIGNATURE_ALGORITHM);
  html_source->AddInteger("certKnownInterceptionBlocked",
                          net::ERR_CERT_KNOWN_INTERCEPTION_BLOCKED);

  html_source->AddLocalizedString("connectionHelpTitle",
                                  IDS_CONNECTION_HELP_TITLE);
  html_source->AddLocalizedString("connectionHelpHeading",
                                  IDS_CONNECTION_HELP_HEADING);
  html_source->AddLocalizedString("connectionHelpGeneralHelp",
                                  IDS_CONNECTION_HELP_GENERAL_HELP);
  html_source->AddLocalizedString("connectionHelpSpecificErrorHeading",
                                  IDS_CONNECTION_HELP_SPECIFIC_ERROR_HEADING);
  html_source->AddLocalizedString(
      "connectionHelpConnectionNotPrivateTitle",
      IDS_CONNECTION_HELP_CONNECTION_NOT_PRIVATE_TITLE);
  html_source->AddLocalizedString(
      "connectionHelpConnectionNotPrivateDetails",
      IDS_CONNECTION_HELP_CONNECTION_NOT_PRIVATE_DETAILS);
  html_source->AddLocalizedString("connectionHelpConnectToNetworkTitle",
                                  IDS_CONNECTION_HELP_CONNECT_TO_NETWORK_TITLE);
  html_source->AddLocalizedString(
      "connectionHelpConnectToNetworkDetails",
      IDS_CONNECTION_HELP_CONNECT_TO_NETWORK_DETAILS);
  html_source->AddLocalizedString("connectionHelpIncorrectClockTitle",
                                  IDS_CONNECTION_HELP_INCORRECT_CLOCK_TITLE);
  html_source->AddLocalizedString("connectionHelpIncorrectClockDetails",
                                  IDS_CONNECTION_HELP_INCORRECT_CLOCK_DETAILS);

// The superfish section should only be added on Windows.
#if BUILDFLAG(IS_WIN)
  html_source->AddBoolean("isWindows", true);
  html_source->AddLocalizedString("connectionHelpMitmSoftwareTitle",
                                  IDS_CONNECTION_HELP_MITM_SOFTWARE_TITLE);
  html_source->AddLocalizedString("connectionHelpMitmSoftwareDetails",
                                  IDS_CONNECTION_HELP_MITM_SOFTWARE_DETAILS);
#else
  html_source->AddBoolean("isWindows", false);
  html_source->AddString("connectionHelpMitmSoftwareTitle", "");
  html_source->AddString("connectionHelpMitmSoftwareDetails", "");
#endif

  html_source->AddLocalizedString("connectionHelpShowMore",
                                  IDS_CONNECTION_HELP_SHOW_MORE);
  html_source->AddLocalizedString("connectionHelpShowLess",
                                  IDS_CONNECTION_HELP_SHOW_LESS);

  html_source->UseStringsJs();

  html_source->AddResourcePath("interstitial_core.css",
                               IDR_SECURITY_INTERSTITIAL_CORE_CSS);
  html_source->AddResourcePath("interstitial_common.css",
                               IDR_SECURITY_INTERSTITIAL_COMMON_CSS);
  html_source->AddResourcePath("connection_help.css",
                               IDR_SECURITY_INTERSTITIAL_CONNECTION_HELP_CSS);
  html_source->AddResourcePath("connection_help.js",
                               IDR_SECURITY_INTERSTITIAL_CONNECTION_HELP_JS);
  html_source->SetDefaultResource(
      IDR_SECURITY_INTERSTITIAL_CONNECTION_HELP_HTML);
}

ConnectionHelpUI::~ConnectionHelpUI() = default;

}  // namespace security_interstitials
