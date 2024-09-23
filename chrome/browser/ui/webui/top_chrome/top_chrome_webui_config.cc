// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_config.h"

#include <set>

#include "base/containers/contains.h"
#include "base/functional/function_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "content/public/browser/webui_config_map.h"

namespace {

using TopChromeWebUIConfigSet = std::set<raw_ptr<content::WebUIConfig>>;

TopChromeWebUIConfigSet& GetTopChromeWebUIConfigSet() {
  static base::NoDestructor<TopChromeWebUIConfigSet> s_config_set;
  return *s_config_set;
}

}  // namespace

TopChromeWebUIConfig::TopChromeWebUIConfig(std::string_view scheme,
                                           std::string_view host)
    : WebUIConfig(scheme, host) {
  GetTopChromeWebUIConfigSet().insert(this);
}

TopChromeWebUIConfig::~TopChromeWebUIConfig() {
  GetTopChromeWebUIConfigSet().erase(this);
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
