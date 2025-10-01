// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chrome_urls/chrome_urls_handler.h"

#include <vector>

#include "base/feature_list.h"
#include "base/strings/strcat.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/webui_url_constants.h"
#include "components/prefs/pref_service.h"
#include "components/webui/chrome_urls/pref_names.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/internal_webui_config.h"
#include "content/public/browser/webui_config_map.h"
#include "content/public/common/url_constants.h"
#include "url/gurl.h"

namespace chrome_urls {

namespace {

// Special case URLs that don't have a WebUIConfig (e.g., due to not being a
// real WebUI - see crbug.com/40089364) that should still be shown as WebUI URLs
// in chrome://chrome-urls. Note: Do not add more URLs to this list. Instead,
// use WebUIConfig to register new WebUIs in the WebUIConfigMap, which will
// automatically display them in chrome://chrome-urls.
base::span<const base::cstring_view> WebUIHostsWithoutConfigs() {
  static constexpr auto kHostsWithoutConfigs =
      std::to_array<base::cstring_view>({
          chrome::kChromeUIProfileInternalsHost,
          content::kChromeUIBlobInternalsHost,
          content::kChromeUIDinoHost,
          chrome::kChromeUIExtensionsInternalsHost,
      });
  return base::span(kHostsWithoutConfigs);
}

bool CompareWebuiUrlInfos(const chrome_urls::mojom::WebuiUrlInfoPtr& info1,
                          const chrome_urls::mojom::WebuiUrlInfoPtr& info2) {
  // Schemes must be either chrome:// or chrome-untrusted://
  CHECK(info1->url.SchemeIs(content::kChromeUIScheme) ||
        info1->url.SchemeIs(content::kChromeUIUntrustedScheme));
  CHECK(info2->url.SchemeIs(content::kChromeUIScheme) ||
        info2->url.SchemeIs(content::kChromeUIUntrustedScheme));
  // Sort chrome:// before chrome-untrusted://. If the schemes are not equal,
  // given the check above one must be chrome:// and one chrome-untrusted://.
  if (info1->url.GetScheme() != info2->url.GetScheme()) {
    return info1->url.SchemeIs(content::kChromeUIScheme);
  }
  return info1->url.GetHost() < info2->url.GetHost();
}

}  // namespace

ChromeUrlsHandler::ChromeUrlsHandler(
    mojo::PendingReceiver<chrome_urls::mojom::PageHandler> receiver,
    mojo::PendingRemote<chrome_urls::mojom::Page> page,
    content::BrowserContext* browser_context)
    : receiver_(this, std::move(receiver)),
      page_(std::move(page)),
      browser_context_(browser_context) {
}

ChromeUrlsHandler::~ChromeUrlsHandler() = default;

void ChromeUrlsHandler::GetUrls(GetUrlsCallback callback) {
  auto& map = content::WebUIConfigMap::GetInstance();
  std::vector<chrome_urls::mojom::WebuiUrlInfoPtr> webui_urls;
  std::vector<content::WebUIConfigInfo> info_list =
      map.GetWebUIConfigList(browser_context_);

  const base::span<const base::cstring_view> hosts = WebUIHostsWithoutConfigs();
  webui_urls.reserve(info_list.size() + hosts.size());
  for (base::cstring_view host : hosts) {
    GURL url(base::StrCat(
        {content::kChromeUIScheme, url::kStandardSchemeSeparator, host}));
    chrome_urls::mojom::WebuiUrlInfoPtr url_info(
        chrome_urls::mojom::WebuiUrlInfo::New());
    url_info->url = url;
    url_info->enabled = true;
    url_info->internal = false;
    webui_urls.push_back(std::move(url_info));
  }
  for (const content::WebUIConfigInfo& config_info : info_list) {
    chrome_urls::mojom::WebuiUrlInfoPtr url_info(
        chrome_urls::mojom::WebuiUrlInfo::New());
    url_info->url = config_info.origin.GetURL();
    url_info->enabled = config_info.enabled;
    url_info->internal = content::IsInternalWebUI(config_info.origin.GetURL());
    webui_urls.push_back(std::move(url_info));
  }
  // Sort the URLs.
  std::sort(webui_urls.begin(), webui_urls.end(), &CompareWebuiUrlInfos);

  chrome_urls::mojom::ChromeUrlsDataPtr result(
      chrome_urls::mojom::ChromeUrlsData::New());
  result->webui_urls = std::move(webui_urls);
  for (base::cstring_view url : chrome::ChromeDebugURLs()) {
    result->command_urls.emplace_back(url);
  }

  PrefService* local_state = g_browser_process->local_state();
  result->internal_debugging_uis_enabled =
      local_state->FindPreference(chrome_urls::kInternalOnlyUisEnabled) &&
      local_state->GetBoolean(chrome_urls::kInternalOnlyUisEnabled);
  std::move(callback).Run(std::move(result));
}

void ChromeUrlsHandler::SetDebugPagesEnabled(
    bool enabled,
    SetDebugPagesEnabledCallback callback) {
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetBoolean(chrome_urls::kInternalOnlyUisEnabled, enabled);
  std::move(callback).Run();
}

}  // namespace chrome_urls
