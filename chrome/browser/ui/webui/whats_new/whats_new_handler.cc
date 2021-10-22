// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/whats_new/whats_new_handler.h"

#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/webui/whats_new/whats_new_util.h"
#include "chrome/common/chrome_version.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/storage_partition.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

const int64_t kMaxDownloadBytes = 1024 * 1024;

// The maximum number of times to try loading prior versions if What's New fails
// to load for the current version. Used only if the user tries to open the page
// manually. What's New generally has new content every other milestone, so only
// 1 retry should be needed most of the time; conservatively set the max to 2.
const int kMaxRetries = 2;

namespace whats_new {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class LoadEvent {
  kLoadStart = 0,
  kLoadSuccess = 1,
  kLoadFailAndShowError = 2,
  kLoadFailAndFallbackToNtp = 3,
  kLoadFailAndCloseTab = 4,
  kMaxValue = kLoadFailAndCloseTab,
};

}  // namespace whats_new

namespace {

void LogLoadEvent(whats_new::LoadEvent event) {
  base::UmaHistogramEnumeration("WhatsNew.LoadEvent", event);
}

std::string GetURLForVersion(int version) {
  // Versions prior to m96 didn't use an extra path.
  // TODO (https://crbug.com/1219381): Remove the < 96 special logic once the
  // M96 page has been created by the server team.
  return version < 96 ? whats_new::kChromeWhatsNewURL
                      : base::StringPrintf(
                            "%sm%d", whats_new::kChromeWhatsNewURL, version);
}

}  // namespace

WhatsNewHandler::WhatsNewHandler() = default;

WhatsNewHandler::~WhatsNewHandler() = default;

void WhatsNewHandler::RegisterMessages() {
  web_ui()->RegisterDeprecatedMessageCallback(
      "initialize", base::BindRepeating(&WhatsNewHandler::HandleInitialize,
                                        base::Unretained(this)));
}

void WhatsNewHandler::OnJavascriptAllowed() {}

void WhatsNewHandler::OnJavascriptDisallowed() {
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void WhatsNewHandler::HandleInitialize(const base::ListValue* args) {
  CHECK_EQ(2U, args->GetList().size());
  std::string callback_id;
  CHECK(args->GetString(0, &callback_id));
  bool is_auto;
  CHECK(args->GetBoolean(1, &is_auto));

  AllowJavascript();
  if (whats_new::IsRemoteContentDisabled()) {
    // Just resolve with failure. This shows the error page which is all local
    // content, so that we don't trigger potentially flaky network requests in
    // tests.
    ResolveJavascriptCallback(base::Value(callback_id), base::Value());
    return;
  }

  Fetch(GURL(GetURLForVersion(CHROME_VERSION_MAJOR)),
        base::BindOnce(&WhatsNewHandler::OnFetchResult,
                       weak_ptr_factory_.GetWeakPtr(), callback_id, is_auto));
}

// TODO (https://crbug.com/1255463): See if there is a way to factor this fetch
// logic out of the handler so it can be performed by the startup tab provider,
// so that the tab does not open at all if What's New fails to load.
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
  int response_code = loader->NetError();
  const auto& headers =
      loader->ResponseInfo() ? loader->ResponseInfo()->headers : nullptr;
  bool success = response_code == net::OK && headers;
  if (headers) {
    response_code =
        net::HttpUtil::MapStatusCodeForHistogram(headers->response_code());
  }

  base::UmaHistogramSparse("WhatsNew.LoadResponseCode", response_code);
  success = success && response_code >= 200 && response_code <= 299 && body;
  bool page_not_found = !success && headers && headers->response_code() == 404;
  // Erase the loader first because |on_result| can destroy this handler.
  loader_map_.erase(loader);
  std::move(on_result).Run(success, page_not_found, std::move(body));
}

void WhatsNewHandler::OnFetchResult(const std::string& callback_id,
                                    bool is_auto,
                                    bool success,
                                    bool page_not_found,
                                    std::unique_ptr<std::string> body) {
  // Update pref if shown automatically. Do this even if the load failed - we
  // only want to try once, so that we don't have to re-query for What's New
  // every time the browser opens.
  if (is_auto) {
    whats_new::SetLastVersion(g_browser_process->local_state());
  }

  if (!success && is_auto) {
    Browser* browser = chrome::FindLastActive();
    if (!browser)
      return;

    if (browser->tab_strip_model()->count() == 1) {
      // Don't close the tab if this is the only tab as doing so will close the
      // browser right after the user tried to start it. Instead, load the NTP
      // as a fallback startup experience if What's New failed to load when
      // being shown automatically.
      LogLoadEvent(whats_new::LoadEvent::kLoadFailAndFallbackToNtp);
      content::OpenURLParams params(GURL(chrome::kChromeUINewTabPageURL),
                                    content::Referrer(),
                                    WindowOpenDisposition::CURRENT_TAB,
                                    ui::PAGE_TRANSITION_AUTO_BOOKMARK, false);
      browser->OpenURL(params);
    } else {
      // If other startup tabs already exist, close the tab in order to show
      // them. This destroys the handler so don't do anything else after this.
      LogLoadEvent(whats_new::LoadEvent::kLoadFailAndCloseTab);
      content::WebContents* contents = web_ui()->GetWebContents();
      chrome::CloseWebContents(browser, contents, /* add_to_history= */ false);
    }
  } else if (!success && page_not_found && num_retries_ < kMaxRetries) {
    // If the user opened the page manually and the error was that the page does
    // not exist, try to load the page for a previous milestone. It might just
    // not have a new version for the current one. We still want to show the
    // most recent What's New (even if it isn't exactly for this milestone) if
    // the user intentionally navigated there.
    num_retries_++;
    Fetch(GURL(GetURLForVersion(CHROME_VERSION_MAJOR - num_retries_)),
          base::BindOnce(&WhatsNewHandler::OnFetchResult,
                         weak_ptr_factory_.GetWeakPtr(), callback_id, is_auto));
  } else {
    LogLoadEvent(success ? whats_new::LoadEvent::kLoadSuccess
                         : whats_new::LoadEvent::kLoadFailAndShowError);
    ResolveJavascriptCallback(
        base::Value(callback_id),
        success
            ? base::Value(GetURLForVersion(CHROME_VERSION_MAJOR - num_retries_))
            : base::Value());
  }
}
