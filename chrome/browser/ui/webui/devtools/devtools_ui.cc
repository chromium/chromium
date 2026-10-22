// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/devtools/devtools_ui.h"

#include "base/command_line.h"
#include "base/strings/string_util.h"
#include "build/util/chromium_git_revision.h"
#include "chrome/browser/devtools/devtools_availability_checker.h"
#include "chrome/browser/devtools/url_constants.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/devtools/devtools_ui_data_source.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/bindings_policy.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"

// static
GURL DevToolsUI::GetProxyURL(const std::string& frontend_url) {
  GURL url(frontend_url);
  if (url.scheme() == content::kChromeDevToolsScheme &&
      url.host() == chrome::kChromeUIDevToolsHost) {
    return GURL();
  }
  if (!url.is_valid() || url.host() != kRemoteFrontendDomain) {
    return GURL();
  }

  return GURL(absl::StrFormat(
      "%s://%s/%s/%s?%s", content::kChromeDevToolsScheme,
      chrome::kChromeUIDevToolsRemoteHost, chrome::kChromeUIDevToolsRemotePath,
      url.path().substr(1), url.query()));
}

// static
GURL DevToolsUI::GetRemoteBaseURL() {
  return GURL(absl::StrFormat("%s%s/%s/", kRemoteFrontendBase,
                              kRemoteFrontendPath, CHROMIUM_GIT_REVISION));
}

// static
bool DevToolsUI::IsFrontendResourceURL(const GURL& url) {
  if (url.host() == kRemoteFrontendDomain) {
    return true;
  }

  const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (cmd_line->HasSwitch(switches::kCustomDevtoolsFrontend)) {
    GURL custom_frontend_url =
        GURL(cmd_line->GetSwitchValueASCII(switches::kCustomDevtoolsFrontend));
    if (custom_frontend_url.is_valid() &&
        custom_frontend_url.scheme() == url.scheme() &&
        custom_frontend_url.host() == url.host() &&
        custom_frontend_url.EffectiveIntPort() == url.EffectiveIntPort() &&
        base::StartsWith(url.path(), custom_frontend_url.path(),
                         base::CompareCase::SENSITIVE)) {
      return true;
    }
  }
  return false;
}

DevToolsUI::DevToolsUI(content::WebUI* web_ui)
    : WebUIController(web_ui), bindings_(web_ui->GetWebContents()) {
  web_ui->SetBindings(content::BindingsPolicySet());
  content::BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();
  if (IsInspectionAllowed(Profile::FromBrowserContext(browser_context),
                          static_cast<content::WebContents*>(nullptr))) {
    auto factory = browser_context->GetDefaultStoragePartition()
                       ->GetURLLoaderFactoryForBrowserProcess();
    content::URLDataSource::Add(
        browser_context,
        std::make_unique<DevToolsDataSource>(factory,
                                             chrome::kChromeUIDevToolsHost));
    content::URLDataSource::Add(
        browser_context,
        std::make_unique<DevToolsDataSource>(
            std::move(factory), chrome::kChromeUIDevToolsRemoteHost));
  }
}

DevToolsUI::~DevToolsUI() = default;

content::WebUIController::TrustPolicy DevToolsUI::GetTrustPolicy() {
  return bindings_.IsTrustedLocalFrontend()
             ? content::WebUIController::TrustPolicy::kTrusted
             : content::WebUIController::TrustPolicy::kUntrusted;
}
