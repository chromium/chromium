// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/devtools/devtools_ui.h"

#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/devtools/url_constants.h"
#include "chrome/browser/ui/webui/devtools/devtools_ui_data_source.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/user_agent.h"

// static
GURL DevToolsUI::GetProxyURL(const std::string& frontend_url) {
  GURL url(frontend_url);
  if (url.scheme() == content::kChromeDevToolsScheme &&
      url.host() == chrome::kChromeUIDevToolsHost)
    return GURL();
  if (!url.is_valid() || url.host() != kRemoteFrontendDomain)
    return GURL();
  return GURL(base::StringPrintf(
      "%s://%s/%s/%s?%s", content::kChromeDevToolsScheme,
      chrome::kChromeUIDevToolsHost, chrome::kChromeUIDevToolsRemotePath,
      url.path().substr(1).c_str(), url.query().c_str()));
}

// static
GURL DevToolsUI::GetRemoteBaseURL() {
  return GURL(base::StringPrintf("%s%s/%s/", kRemoteFrontendBase,
                                 kRemoteFrontendPath,
                                 content::GetChromiumGitRevision().c_str()));
}

// static
bool DevToolsUI::IsFrontendResourceURL(const GURL& url) {
  if (url.host_piece() == kRemoteFrontendDomain)
    return true;

  const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (cmd_line->HasSwitch(switches::kCustomDevtoolsFrontend)) {
    GURL custom_frontend_url =
        GURL(cmd_line->GetSwitchValueASCII(switches::kCustomDevtoolsFrontend));
    if (custom_frontend_url.is_valid() &&
        custom_frontend_url.scheme_piece() == url.scheme_piece() &&
        custom_frontend_url.host_piece() == url.host_piece() &&
        custom_frontend_url.EffectiveIntPort() == url.EffectiveIntPort() &&
        base::StartsWith(url.path_piece(), custom_frontend_url.path_piece(),
                         base::CompareCase::SENSITIVE)) {
      return true;
    }
  }
  return false;
}

DevToolsUI::DevToolsUI(content::WebUI* web_ui)
    : WebUIController(web_ui), bindings_(web_ui->GetWebContents()) {
  web_ui->SetBindings(content::BindingsPolicySet());
  auto factory = web_ui->GetWebContents()
                     ->GetBrowserContext()
                     ->GetDefaultStoragePartition()
                     ->GetURLLoaderFactoryForBrowserProcess();
  content::URLDataSource::Add(
      web_ui->GetWebContents()->GetBrowserContext(),
      std::make_unique<DevToolsDataSource>(std::move(factory)));
}

DevToolsUI::~DevToolsUI() = default;
