// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/whats_new/whats_new_util.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_version.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace whats_new {
const int64_t kMaxDownloadBytes = 1024 * 1024;

const char kChromeWhatsNewURL[] = "https://www.google.com/chrome/whats-new/";
const char kChromeWhatsNewURLShort[] = "google.com/chrome/whats-new/";

bool g_is_remote_content_disabled = false;

void DisableRemoteContentForTests() {
  g_is_remote_content_disabled = true;
}

bool IsRemoteContentDisabled() {
  return g_is_remote_content_disabled;
}

bool ShouldShowForState(PrefService* local_state) {
  if (!local_state)
    return false;

  if (!base::FeatureList::IsEnabled(features::kChromeWhatsNewUI))
    return false;

  // M97 does not have a What's New page. Handling this separately here, to
  // enable early removal of retry logic in the handler and to allow time to
  // figure out how to query the server side before opening the tab.
  // To keep consistency for tests, don't follow this special case if remote
  // content is disabled for testing.
  if (CHROME_VERSION_MAJOR == 97 && !IsRemoteContentDisabled())
    return false;

  // Show What's New if the page hasn't yet been shown for the current
  // milestone.
  int last_version = local_state->GetInteger(prefs::kLastWhatsNewVersion);
  return CHROME_VERSION_MAJOR > last_version;
}

void SetLastVersion(PrefService* local_state) {
  if (!local_state) {
    return;
  }

  local_state->SetInteger(prefs::kLastWhatsNewVersion, CHROME_VERSION_MAJOR);
}

void LogLoadEvent(whats_new::LoadEvent event) {
  base::UmaHistogramEnumeration("WhatsNew.LoadEvent", event);
}

std::string GetURLForVersion(int version) {
  // Versions prior to m98 don't respect the query parameter. There is no
  // version for M97. We should never be automatically loading the page for M97,
  // see ShouldShowForState() in whats_new_util.cc.
  // TODO (https://crbug.com/1219381): Remove this logic in M98.
  return version < 98
             ? base::StringPrintf("%sm%d", whats_new::kChromeWhatsNewURL, 96)
             : base::StringPrintf("%s/?version=m%d",
                                  whats_new::kChromeWhatsNewURL, version);
}

// TODO (https://crbug.com/1255463): Run this logic before opening the tab at
// all.
WhatsNewFetcher::WhatsNewFetcher(int version,
                                 bool is_auto,
                                 OnFetchResultCallback on_result)
    : is_auto_(is_auto), callback_(std::move(on_result)) {
  LogLoadEvent(whats_new::LoadEvent::kLoadStart);
  auto traffic_annotation =
      net::DefineNetworkTrafficAnnotation("whats_new_handler", R"(
        semantics {
          sender: "What's New Page"
          description: "Attempts to fetch the content for the What's New page "
            "to ensure it loads successfully."
          trigger:
            "Restarting Chrome after an update. Desktop only."
          data:
            "No data sent, other than URL of What's New. "
            "Data does not contain PII."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "None"
          chrome_policy {
            PromotionalTabsEnabled {
              PromotionalTabsEnabled: false
            }
          }
        })");
  network::mojom::URLLoaderFactory* loader_factory =
      g_browser_process->system_network_context_manager()
          ->GetURLLoaderFactory();
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = GURL(GetURLForVersion(version));
  simple_loader_ =
      network::SimpleURLLoader::Create(std::move(request), traffic_annotation);
  simple_loader_->DownloadToString(
      loader_factory,
      base::BindOnce(&WhatsNewFetcher::OnResponseLoaded,
                     base::Unretained(this)),
      kMaxDownloadBytes);
}

WhatsNewFetcher::~WhatsNewFetcher() = default;

void WhatsNewFetcher::OnResponseLoaded(std::unique_ptr<std::string> body) {
  int response_code = simple_loader_->NetError();
  const auto& headers = simple_loader_->ResponseInfo()
                            ? simple_loader_->ResponseInfo()->headers
                            : nullptr;
  bool success = response_code == net::OK && headers;
  if (headers) {
    response_code =
        net::HttpUtil::MapStatusCodeForHistogram(headers->response_code());
  }

  base::UmaHistogramSparse("WhatsNew.LoadResponseCode", response_code);
  success = success && response_code >= 200 && response_code <= 299 && body;
  bool page_not_found = !success && headers && headers->response_code() == 404;

  // Update pref if shown automatically. Do this even if the load failed - we
  // only want to try once, so that we don't have to re-query for What's New
  // every time the browser opens.
  if (is_auto_) {
    SetLastVersion(g_browser_process->local_state());
  }

  // Running this callback might destroy |this| so don't do anything else
  // afterward.
  std::move(callback_).Run(is_auto_, success, page_not_found, std::move(body));
}

}  // namespace whats_new
