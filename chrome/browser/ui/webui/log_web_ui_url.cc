// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/log_web_ui_url.h"

#include <stdint.h>

#include <variant>

#include "base/hash/hash.h"
#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/url_constants.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/webui/top_chrome/webui_contents_preload_manager.h"
#endif  // !BUILDFLAG(IS_ANDROID)

namespace {

const GURL& GetWebUIUrlForLogging(content::WebUI* web_ui) {
  // This returns the actual WebUI url which can differ from the visible
  // URL (e.g. chrome://newtab can be rewrote to chrome://new-tab-page or
  // chrome://new-tab-page-third-party)
  return web_ui->GetRenderFrameHost()->GetSiteInstance()->GetSiteURL();
}

bool ShouldLogUrl(const GURL& web_ui_url) {
  return web_ui_url.SchemeIs(content::kChromeUIScheme) ||
         web_ui_url.SchemeIs(content::kChromeUIUntrustedScheme) ||
         web_ui_url.SchemeIs(content::kChromeDevToolsScheme);
}

}  // namespace

namespace webui {

const char kWebUICreatedForUrl[] = "WebUI.CreatedForUrl";
const char kWebUIShownUrl[] = "WebUI.ShownUrl";

bool LogWebUICreated(const GURL& web_ui_url) {
  if (!ShouldLogUrl(web_ui_url)) {
    return false;
  }

  uint32_t hash = base::Hash(web_ui_url.DeprecatedGetOriginAsURL().spec());
  base::UmaHistogramSparse(kWebUICreatedForUrl,
                           static_cast<base::HistogramBase::Sample32>(hash));
  return true;
}

bool LogWebUIShown(const GURL& web_ui_url) {
  if (!ShouldLogUrl(web_ui_url)) {
    return false;
  }

  uint32_t hash = base::Hash(web_ui_url.DeprecatedGetOriginAsURL().spec());
  base::UmaHistogramSparse(kWebUIShownUrl,
                           static_cast<base::HistogramBase::Sample32>(hash));
  return true;
}

void LogWebUIUsage(std::variant<content::WebUI*, GURL> webui_variant) {
  const GURL& web_ui_url =
      std::holds_alternative<GURL>(webui_variant)
          ? std::get<GURL>(webui_variant)
          : GetWebUIUrlForLogging(std::get<content::WebUI*>(webui_variant));
  LogWebUICreated(web_ui_url);

#if !BUILDFLAG(IS_ANDROID)
  auto* preload_manager = WebUIContentsPreloadManager::GetInstance();
  auto* web_contents =
      std::holds_alternative<content::WebUI*>(webui_variant)
          ? std::get<content::WebUI*>(webui_variant)->GetWebContents()
          : nullptr;

  // A preloaded WebUI may be not yet shown.
  // The preload manager will log the URL when the WebUI is requested.
  if (!preload_manager->WasPreloaded(web_contents) ||
      preload_manager->GetRequestTime(web_contents).has_value()) {
    LogWebUIShown(web_ui_url);
  }
#endif  // !BUILDFLAG(IS_ANDROID)
}

}  // namespace webui
