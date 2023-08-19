// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/devtools/cast_devtools_manager_delegate.h"

#include "build/build_config.h"
#include "chromecast/app/grit/shell_resources.h"
#include "content/public/browser/devtools_agent_host.h"
#include "ui/base/resource/resource_bundle.h"

namespace chromecast {
namespace shell {

namespace {
CastDevToolsManagerDelegate* g_devtools_manager_delegate = nullptr;
}  // namespace

// static
CastDevToolsManagerDelegate* CastDevToolsManagerDelegate::GetInstance() {
  DCHECK(g_devtools_manager_delegate);
  return g_devtools_manager_delegate;
}

CastDevToolsManagerDelegate::CastDevToolsManagerDelegate() {
  DCHECK(!g_devtools_manager_delegate);
  g_devtools_manager_delegate = this;
}

CastDevToolsManagerDelegate::~CastDevToolsManagerDelegate() {
  DCHECK_EQ(this, g_devtools_manager_delegate);
  g_devtools_manager_delegate = nullptr;
}

content::DevToolsAgentHost::List
CastDevToolsManagerDelegate::RemoteDebuggingTargets(
    content::DevToolsManagerDelegate::TargetType target_type) {
  content::DevToolsAgentHost::List enabled_hosts;
  for (auto* web_contents : enabled_webcontents_) {
    enabled_hosts.push_back(
        target_type == content::DevToolsManagerDelegate::kTab
            ? content::DevToolsAgentHost::GetOrCreateForTab(web_contents)
            : content::DevToolsAgentHost::GetOrCreateFor(web_contents));
  }
  return enabled_hosts;
}

void CastDevToolsManagerDelegate::EnableWebContentsForDebugging(
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  enabled_webcontents_.insert(web_contents);
}

void CastDevToolsManagerDelegate::DisableWebContentsForDebugging(
    content::WebContents* web_contents) {
  enabled_webcontents_.erase(web_contents);
}

bool CastDevToolsManagerDelegate::HasEnabledWebContents() const {
  return !enabled_webcontents_.empty();
}

std::string CastDevToolsManagerDelegate::GetDiscoveryPageHTML() {
#if BUILDFLAG(IS_ANDROID)
  return std::string();
#else
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
      IDR_CAST_SHELL_DEVTOOLS_DISCOVERY_PAGE);
#endif
}

}  // namespace shell
}  // namespace chromecast
