// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/apc_internals/apc_internals_ui.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/apc_internals/apc_internals_handler.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/dev_ui_browser_resources.h"
#include "components/embedder_support/user_agent_utils.h"
#include "components/version_info/version_info.h"
#include "components/version_ui/version_handler_helper.h"
#include "components/version_ui/version_ui_constants.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

namespace {

content::WebUIDataSource* CreateAPCInternalsHTMLSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIAPCInternalsHost);
  source->AddResourcePath("apc_internals.js", IDR_APC_INTERNALS_JS);
  source->AddResourcePath("apc_internals.css", IDR_APC_INTERNALS_CSS);
  source->SetDefaultResource(IDR_APC_INTERNALS_HTML);
  // Data strings:
  source->AddString(version_ui::kVersion, version_info::GetVersionNumber());
  source->AddString(version_ui::kOfficial, version_info::IsOfficialBuild()
                                               ? "official"
                                               : "Developer build");
  source->AddString(version_ui::kVersionModifier,
                    chrome::GetChannelName(chrome::WithExtendedStable(true)));
  source->AddString(version_ui::kCL, version_info::GetLastChange());
  source->AddString(version_ui::kUserAgent, embedder_support::GetUserAgent());
  source->AddString("app_locale", g_browser_process->GetApplicationLocale());

  return source;
}

}  // namespace

APCInternalsUI::APCInternalsUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);

  // Create web page
  content::WebUIDataSource* source = CreateAPCInternalsHTMLSource();
  content::WebUIDataSource::Add(profile, source);

  // Add a message handler
  web_ui->AddMessageHandler(std::make_unique<APCInternalsHandler>());
}

APCInternalsUI::~APCInternalsUI() = default;
