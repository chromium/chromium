// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/ash/kerberos/kerberos_in_browser_ui.h"

#include <memory>

#include "base/functional/bind.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/kerberos_resources.h"
#include "chrome/grit/kerberos_resources_map.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "net/base/features.h"

namespace ash {

KerberosInBrowserUIConfig::KerberosInBrowserUIConfig()
    : ChromeOSWebUIConfig(content::kChromeUIScheme,
                          chrome::kChromeUIKerberosInBrowserHost) {}

bool KerberosInBrowserUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return base::FeatureList::IsEnabled(
      net::features::kKerberosInBrowserRedirect);
}

KerberosInBrowserUI::KerberosInBrowserUI(content::WebUI* web_ui)
    : WebDialogUI(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUIKerberosInBrowserHost);

  webui::SetupWebUIDataSource(
      source, base::make_span(kKerberosResources, kKerberosResourcesSize),
      IDR_KERBEROS_KERBEROS_IN_BROWSER_DIALOG_HTML);

  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"kerberosInBrowserTitle", IDS_SETTINGS_KERBEROS_IN_BROWSER_DIALOG_TITLE},
      {"kerberosInBrowserDescription",
       IDS_SETTINGS_KERBEROS_IN_BROWSER_DIALOG_DESCRIPTION},
      {"kerberosInBrowserVisitWithoutTicket",
       IDS_SETTINGS_KERBEROS_IN_BROWSER_DIALOG_VISIT_WITHOUT_TICKET_BUTTON},
      {"kerberosInBrowserManageTickets",
       IDS_SETTINGS_KERBEROS_IN_BROWSER_DIALOG_MANAGE_TICKETS_BUTTON}};
  source->AddLocalizedStrings(kLocalizedStrings);
}

KerberosInBrowserUI::~KerberosInBrowserUI() = default;

}  // namespace ash
