// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/whats_new/whats_new_util.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/url_util.h"
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

void LogStartupType(StartupType type) {
  base::UmaHistogramEnumeration("WhatsNew.StartupType", type);
}

bool IsRemoteContentDisabled() {
  return g_is_remote_content_disabled;
}

bool ShouldShowForState(PrefService* local_state,
                        bool promotional_tabs_enabled) {
  LogStartupType(StartupType::kCalledShouldShow);

  if (!promotional_tabs_enabled) {
    whats_new::LogStartupType(whats_new::StartupType::kPromotionalTabsDisabled);
    return false;
  }

  if (!local_state ||
      !local_state->FindPreference(prefs::kLastWhatsNewVersion)) {
    LogStartupType(StartupType::kInvalidState);
    return false;
  }

  // Allow disabling the What's New experience in tests using the standard
  // kNoFirstRun switch. This behavior can be overridden using the
  // kForceWhatsNew switch for the What's New experience integration tests.
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if ((command_line->HasSwitch(switches::kNoFirstRun) &&
       !command_line->HasSwitch(switches::kForceWhatsNew)) ||
      !base::FeatureList::IsEnabled(features::kChromeWhatsNewUI)) {
    LogStartupType(StartupType::kFeatureDisabled);
    return false;
  }

  int last_version = local_state->GetInteger(prefs::kLastWhatsNewVersion);

  // Don't show What's New if it's already been shown for the current major
  // milestone.
  if (CHROME_VERSION_MAJOR <= last_version) {
    LogStartupType(StartupType::kAlreadyShown);
    return false;
  }

  // Set the last version here to indicate that What's New should not attempt
  // to display again for this milestone. This prevents the page from
  // potentially displaying multiple times in a given milestone, e.g. for
  // multiple profile relaunches (see https://crbug.com/1274313).
  local_state->SetInteger(prefs::kLastWhatsNewVersion, CHROME_VERSION_MAJOR);
  return true;
}

GURL GetServerURL(bool may_redirect) {
  const GURL url =
      may_redirect
          ? net::AppendQueryParameter(
                GURL(kChromeWhatsNewURL), "version",
                base::NumberToString(CHROME_VERSION_MAJOR))
          : GURL(kChromeWhatsNewURL)
                .Resolve(base::StringPrintf("m%d", CHROME_VERSION_MAJOR));
  return net::AppendQueryParameter(url, "internal", "true");
}

GURL GetWebUIStartupURL() {
  return net::AppendQueryParameter(GURL(chrome::kChromeUIWhatsNewURL), "auto",
                                   "true");
}

namespace {

void AddWhatsNewTab(Browser* browser) {
  chrome::AddTabAt(browser, GetWebUIStartupURL(), 0, true);
  browser->tab_strip_model()->ActivateTabAt(
      browser->tab_strip_model()->IndexOfFirstNonPinnedTab());
}

class WhatsNewFetcher : public BrowserListObserver {
 public:
  explicit WhatsNewFetcher(Browser* browser) : browser_(browser) {
    BrowserList::AddObserver(this);
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
        net::DefineNetworkTrafficAnnotation("whats_new_handler", R"(
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
    // Don't allow redirects when checking if the page is valid for the current
    // milestone.
    request->url = GetServerURL(false);
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

  // BrowserListObserver:
  void OnBrowserRemoved(Browser* browser) override {
    if (browser != browser_)
      return;

    browser_closed_or_inactive_ = true;
    BrowserList::RemoveObserver(this);
    browser_ = nullptr;
  }

  void OnBrowserNoLongerActive(Browser* browser) override {
    if (browser == browser_)
      browser_closed_or_inactive_ = true;
  }

  void OnBrowserSetLastActive(Browser* browser) override {
    if (browser == browser_)
      browser_closed_or_inactive_ = false;
  }

 private:
  static void LogLoadEvent(LoadEvent event) {
    base::UmaHistogramEnumeration("WhatsNew.LoadEvent", event);
  }

  void OpenWhatsNewTabForTest() {
    if (browser_closed_or_inactive_)
      return;

    AddWhatsNewTab(browser_);
    delete this;
  }

  void OnResponseLoaded(std::unique_ptr<std::string> body) {
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
    success = success && error_or_response_code >= 200 &&
              error_or_response_code <= 299 && body;

    // If the browser was closed or moved to the background while What's New was
    // loading, return early before recording that the user saw the page.
    if (browser_closed_or_inactive_)
      return;

    DCHECK(browser_);

    LogLoadEvent(success ? LoadEvent::kLoadSuccess
                         : LoadEvent::kLoadFailAndDoNotShow);
    if (success)
      AddWhatsNewTab(browser_);
    delete this;
  }

  std::unique_ptr<network::SimpleURLLoader> simple_loader_;
  raw_ptr<Browser> browser_;
  bool browser_closed_or_inactive_ = false;
};

}  // namespace

void StartWhatsNewFetch(Browser* browser) {
  new WhatsNewFetcher(browser);
}

}  // namespace whats_new
