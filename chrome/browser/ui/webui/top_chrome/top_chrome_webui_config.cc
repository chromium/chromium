// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_config.h"

#include <optional>
#include <set>
#include <string_view>

#include "base/functional/function_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/webui_config_map.h"
#include "content/public/common/url_constants.h"

namespace {

using TopChromeWebUIConfigSet = std::set<raw_ptr<content::WebUIConfig>>;

TopChromeWebUIConfigSet& GetTopChromeWebUIConfigSet() {
  static base::NoDestructor<TopChromeWebUIConfigSet> s_config_set;
  return *s_config_set;
}

// Returns the process lock host for a Top Chrome WebUI. Only trusted
// `chrome://` WebUIs may share the Top Chrome renderer process;
// `chrome-untrusted://` WebUIs are always locked to their own site URL.
std::optional<std::string_view> GetProcessLockHost(
    std::string_view scheme,
    TopChromeWebUIConfig::ProcessModel process_model) {
  if (scheme == content::kChromeUIScheme &&
      process_model == TopChromeWebUIConfig::ProcessModel::kTopChromeProcess) {
    return chrome::kChromeUITopChromeDomain;
  }
  return std::nullopt;
}

}  // namespace

TopChromeWebUIConfig::TopChromeWebUIConfig(std::string_view scheme,
                                           std::string_view host,
                                           ProcessModel process_model)
    : WebUIConfig(scheme, host, GetProcessLockHost(scheme, process_model)) {
  GetTopChromeWebUIConfigSet().insert(this);
}

TopChromeWebUIConfig::~TopChromeWebUIConfig() {
  GetTopChromeWebUIConfigSet().erase(this);
}

bool TopChromeWebUIConfig::SupportsInProcessResourceLoadingV2() const {
  return true;
}

std::optional<GURL> TopChromeWebUIConfig::GetPreloadEngagementAliasURL() const {
  return std::nullopt;
}

// static
TopChromeWebUIConfig* TopChromeWebUIConfig::From(
    content::BrowserContext* browser_context,
    const GURL& url) {
  content::WebUIConfig* config =
      content::WebUIConfigMap::GetInstance().GetConfig(browser_context, url);
  return GetTopChromeWebUIConfigSet().contains(config)
             ? static_cast<TopChromeWebUIConfig*>(config)
             : nullptr;
}

// static
void TopChromeWebUIConfig::ForEachConfig(
    base::FunctionRef<void(TopChromeWebUIConfig*)> on_config) {
  for (content::WebUIConfig* config : GetTopChromeWebUIConfigSet()) {
    on_config(static_cast<TopChromeWebUIConfig*>(config));
  }
}
