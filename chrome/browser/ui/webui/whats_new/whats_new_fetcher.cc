// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/whats_new/whats_new_fetcher.h"

#include <optional>
#include <string>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/whats_new/whats_new_util.h"
#include "chrome/common/chrome_version.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/reduce_accept_language_controller_delegate.h"
#include "net/base/url_util.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace whats_new {
const char kChromeWhatsNewURL[] = "https://www.google.com/chrome/whats-new/";
const char kChromeWhatsNewRefreshURL[] =
    "https://www.google.com/chrome/wn-2025/whats-new/";
const char kChromeWhatsNewStagingURL[] =
    "https://chrome-staging.corp.google.com/chrome/whats-new/";
const char kChromeWhatsNewRefreshStagingURL[] =
    "https://chrome-staging.corp.google.com/chrome/wn-2025/whats-new/";

const int64_t kMaxDownloadBytes = 1024 * 1024;

GURL AddVersionParameter(const GURL& base_url) {
  return net::AppendQueryParameter(base_url, "version",
                                   base::NumberToString(CHROME_VERSION_MAJOR));
}

GURL GetServerLegacyURL(bool is_staging) {
  const GURL base_url =
      is_staging ? GURL(kChromeWhatsNewStagingURL) : GURL(kChromeWhatsNewURL);
  return AddVersionParameter(base_url);
}

GURL GetServerRefreshURL(bool is_staging) {
  const GURL base_url = is_staging ? GURL(kChromeWhatsNewRefreshStagingURL)
                                   : GURL(kChromeWhatsNewRefreshURL);
  return AddVersionParameter(base_url);
}

GURL GetServerURL(bool is_staging) {
  return base::FeatureList::IsEnabled(features::kWhatsNewDesktopRefresh)
             ? GetServerRefreshURL(is_staging)
             : GetServerLegacyURL(is_staging);
}

GURL GetServerURLForRender(const WhatsNewRegistry& whats_new_registry,
                           bool is_staging) {
  GURL url = GetServerURL(is_staging);
  const auto active_features = whats_new_registry.GetActiveFeatureNames();
  if (!active_features.empty()) {
    url = net::AppendQueryParameter(
        url, "enabled", base::JoinString(active_features, std::string(",")));
  }

  const auto rolled_features = whats_new_registry.GetRolledFeatureNames();
  if (!rolled_features.empty()) {
    url = net::AppendQueryParameter(
        url, "rolled", base::JoinString(rolled_features, std::string(",")));
  }

  const auto customizations = whats_new_registry.GetCustomizations();
  if (!customizations.empty()) {
    url = net::AppendQueryParameter(
        url, "customization",
        base::JoinString(customizations, std::string(",")));
  }

  return net::AppendQueryParameter(url, "internal", "true");
}

namespace {

class WhatsNewFetcher : public BrowserListObserver {
 public:
  explicit WhatsNewFetcher(Browser* browser) : browser_(browser) {
    BrowserList::AddObserver(this);
    browser_did_become_active_subscription_ = browser_->RegisterDidBecomeActive(
        base::BindRepeating(&WhatsNewFetcher::OnBrowserDidBecomeActive,
                            base::Unretained(this)));
    browser_did_become_inactive_subscription_ =
        browser_->RegisterDidBecomeActive(
            base::BindRepeating(&WhatsNewFetcher::OnBrowserDidBecomeInactive,
                                base::Unretained(this)));

    GURL server_url = GetServerURL();
    startup_url_ = GetWebUIStartupURL();

    if (IsRemoteContentDisabled()) {
      // Don't fetch network content if this is the case, just pretend the tab
      // was retrieved successfully. Do so asynchronously to simulate the
      // production code better.
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&WhatsNewFetcher::OpenWhatsNewTabForTest,
                                    base::Unretained(this)));
      return;
    }

    LogLoadEvent(LoadEvent::kLoadStart);
    auto traffic_annotation =
        net::DefineNetworkTrafficAnnotation("whats_new_fetcher", R"(
          semantics {
            sender: "What's New Page"
            description: "Attempts to fetch the content for the What's New "
              "page to ensure it loads successfully."
            trigger:
              "Restarting Chrome after an update. Desktop only."
            data:
              "No data sent, other than URL of What's New. "
              "Data does not contain PII."
            destination: GOOGLE_OWNED_SERVICE
            internal {
              contacts {
                email: "frizzle-team@google.com"
              }
            }
            user_data {
              type: NONE
            }
            last_reviewed: "2024-05-22"
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

    // Inform the server of the top browser language via the
    // Accept-Language header.
    if (auto* profile = browser->profile()) {
      if (auto* delegate =
              profile->GetReduceAcceptLanguageControllerDelegate()) {
        auto languages = delegate->GetUserAcceptLanguages();
        if (!languages.empty()) {
          request->headers.SetHeader(request->headers.kAcceptLanguage,
                                     languages.front());
        }
      }
    }

    // Don't allow redirects when checking if the page is valid for the current
    // milestone.
    request->url = server_url;
    simple_loader_ = network::SimpleURLLoader::Create(std::move(request),
                                                      traffic_annotation);
    // base::Unretained is safe here because only OnResponseLoaded deletes
    // |this|.
    simple_loader_->DownloadToString(
        loader_factory,
        base::BindOnce(&WhatsNewFetcher::OnResponseLoaded,
                       base::Unretained(this)),
        kMaxDownloadBytes);
  }

  ~WhatsNewFetcher() override { BrowserList::RemoveObserver(this); }

  void OnBrowserDidBecomeActive(BrowserWindowInterface* browser) {
    browser_closed_or_inactive_ = false;
  }

  void OnBrowserDidBecomeInactive(BrowserWindowInterface* browser) {
    browser_closed_or_inactive_ = true;
  }

  // BrowserListObserver:
  void OnBrowserRemoved(Browser* browser) override {
    if (browser != browser_) {
      return;
    }

    browser_closed_or_inactive_ = true;
    BrowserList::RemoveObserver(this);
    browser_did_become_active_subscription_ = {};
    browser_did_become_inactive_subscription_ = {};
    browser_ = nullptr;
  }

 private:
  void AddWhatsNewTab(Browser* browser) {
    chrome::AddTabAt(browser, startup_url_, 0, true);
    browser->tab_strip_model()->ActivateTabAt(
        browser->tab_strip_model()->IndexOfFirstNonPinnedTab());
  }

  static void LogLoadEvent(LoadEvent event) {
    base::UmaHistogramEnumeration("WhatsNew.LoadEvent", event);
  }

  void OpenWhatsNewTabForTest() {
    if (browser_closed_or_inactive_) {
      return;
    }

    AddWhatsNewTab(browser_);
    delete this;
  }

  void OnResponseLoaded(std::optional<std::string> body) {
    int error_or_response_code = simple_loader_->NetError();
    const auto& headers = simple_loader_->ResponseInfo()
                              ? simple_loader_->ResponseInfo()->headers
                              : nullptr;
    bool success = error_or_response_code == net::OK && headers;
    if (headers) {
      error_or_response_code =
          net::HttpUtil::MapStatusCodeForHistogram(headers->response_code());
    }

    base::UmaHistogramSparse("WhatsNew.LoadResponseCode",
                             error_or_response_code);

    // The server may respond with a 302 to indicate the requested
    // page version does not exist but a suitable page has been found.
    // This should not result in an error since the auto-opened page
    // can still access a relevant resource.
    success = success && error_or_response_code >= 200 &&
              error_or_response_code <= 302 && body;

    // If the browser was closed or moved to the background while What's New was
    // loading, return early before recording that the user saw the page.
    if (browser_closed_or_inactive_) {
      LogLoadEvent(LoadEvent::kLoadAbort);
      return;
    }

    DCHECK(browser_);

    LogLoadEvent(success ? LoadEvent::kLoadSuccess
                         : LoadEvent::kLoadFailAndDoNotShow);

    if (success) {
      AddWhatsNewTab(browser_);
    }
    delete this;
  }

  std::unique_ptr<network::SimpleURLLoader> simple_loader_;
  raw_ptr<Browser> browser_;
  bool browser_closed_or_inactive_ = false;
  GURL startup_url_;
  base::CallbackListSubscription browser_did_become_active_subscription_;
  base::CallbackListSubscription browser_did_become_inactive_subscription_;
};

}  // namespace

void StartWhatsNewFetch(Browser* browser) {
  new WhatsNewFetcher(browser);
}

}  // namespace whats_new
