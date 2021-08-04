// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/whats_new/whats_new_handler.h"

#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/webui/whats_new/whats_new_util.h"
#include "chrome/common/chrome_version.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

const int64_t kMaxDownloadBytes = 1024 * 1024;

namespace whats_new {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class LoadEvent {
  kLoadStart = 0,
  kLoadSuccess = 1,
  kLoadFailAndShowError = 2,
  kLoadFailAndFallbackToNtp = 3,
  kMaxValue = kLoadFailAndFallbackToNtp,
};

}  // namespace whats_new

namespace {

void LogLoadEvent(whats_new::LoadEvent event) {
  base::UmaHistogramEnumeration("WhatsNew.LoadEvent", event);
}

}  // namespace

WhatsNewHandler::WhatsNewHandler() = default;

WhatsNewHandler::~WhatsNewHandler() = default;

void WhatsNewHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "initialize", base::BindRepeating(&WhatsNewHandler::HandleInitialize,
                                        base::Unretained(this)));
}

void WhatsNewHandler::OnJavascriptAllowed() {}

void WhatsNewHandler::OnJavascriptDisallowed() {
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void WhatsNewHandler::HandleInitialize(const base::ListValue* args) {
  CHECK_EQ(2U, args->GetSize());
  std::string callback_id;
  CHECK(args->GetString(0, &callback_id));
  bool is_auto;
  CHECK(args->GetBoolean(1, &is_auto));

  AllowJavascript();
  if (whats_new::g_force_enable_for_tests) {
    // Just resolve with failure. This shows the error page which is all local
    // content, so that we don't trigger potentially flaky network requests in
    // tests.
    ResolveJavascriptCallback(base::Value(callback_id), base::Value());
    return;
  }

  Fetch(GURL(whats_new::kChromeWhatsNewURL),
        base::BindOnce(&WhatsNewHandler::OnFetchResult,
                       weak_ptr_factory_.GetWeakPtr(), callback_id, is_auto));
}

void WhatsNewHandler::Fetch(const GURL& url, OnFetchResultCallback on_result) {
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
  Profile* profile = Profile::FromWebUI(web_ui());
  auto url_loader_factory = profile->GetDefaultStoragePartition()
                                ->GetURLLoaderFactoryForBrowserProcess();
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = url;
  auto loader =
      network::SimpleURLLoader::Create(std::move(request), traffic_annotation);
  loader->DownloadToString(url_loader_factory.get(),
                           base::BindOnce(&WhatsNewHandler::OnResponseLoaded,
                                          weak_ptr_factory_.GetWeakPtr(),
                                          loader.get(), std::move(on_result)),
                           kMaxDownloadBytes);
  loader_map_.insert({loader.get(), std::move(loader)});
}

void WhatsNewHandler::OnResponseLoaded(const network::SimpleURLLoader* loader,
                                       OnFetchResultCallback on_result,
                                       std::unique_ptr<std::string> body) {
  bool success = loader->NetError() == net::OK && loader->ResponseInfo() &&
                 loader->ResponseInfo()->headers &&
                 loader->ResponseInfo()->headers->response_code() >= 200 &&
                 loader->ResponseInfo()->headers->response_code() <= 299 &&
                 body;
  std::move(on_result).Run(success, std::move(body));
  loader_map_.erase(loader);
}

void WhatsNewHandler::OnFetchResult(const std::string& callback_id,
                                    bool is_auto,
                                    bool success,
                                    std::unique_ptr<std::string> body) {
  if (!success && is_auto) {
    // Open NTP if the page wasn't retrieved and What's New was opened
    // automatically.
    Browser* browser = chrome::FindLastActive();
    if (!browser)
      return;

    LogLoadEvent(whats_new::LoadEvent::kLoadFailAndFallbackToNtp);
    content::OpenURLParams params(GURL(chrome::kChromeUINewTabPageURL),
                                  content::Referrer(),
                                  WindowOpenDisposition::CURRENT_TAB,
                                  ui::PAGE_TRANSITION_AUTO_BOOKMARK, false);
    browser->OpenURL(params);
  } else {
    LogLoadEvent(success ? whats_new::LoadEvent::kLoadSuccess
                         : whats_new::LoadEvent::kLoadFailAndShowError);
    // Update pref if successfully shown automatically.
    if (success && is_auto) {
      whats_new::SetLastVersion(g_browser_process->local_state());
    }
    ResolveJavascriptCallback(
        base::Value(callback_id),
        success ? base::Value(whats_new::kChromeWhatsNewURL) : base::Value());
  }
}
