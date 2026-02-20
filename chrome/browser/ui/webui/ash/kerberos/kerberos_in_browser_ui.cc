// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/kerberos/kerberos_in_browser_ui.h"

#include <memory>

#include "ash/constants/webui_url_constants.h"
#include "base/functional/bind.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/kerberos_resources.h"
#include "chrome/grit/kerberos_resources_map.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/webui_util.h"

namespace ash {

KerberosInBrowserUIConfig::KerberosInBrowserUIConfig()
    : ChromeOSWebUIConfig(content::kChromeUIScheme,
                          ash::kChromeUIKerberosInBrowserHost) {}

KerberosInBrowserUI::KerberosInBrowserUI(content::WebUI* web_ui)
    : WebDialogUI(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, ash::kChromeUIKerberosInBrowserHost);

  webui::SetupWebUIDataSource(source, kKerberosResources,
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
