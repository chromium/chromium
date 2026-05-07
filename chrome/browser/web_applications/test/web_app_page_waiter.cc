// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/web_app_page_waiter.h"

#include <algorithm>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/test/run_until.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page.h"
#include "content/public/browser/page_manifest_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"

namespace web_app::test {

namespace {
std::string ToString(WebAppPageWaiter::Expectation expectation) {
  switch (expectation) {
    case WebAppPageWaiter::Expectation::kUnset:
      return "kUnset";
    case WebAppPageWaiter::Expectation::kManifest:
      return "kManifest";
    case WebAppPageWaiter::Expectation::kNoManifest:
      return "kNoManifest";
    case WebAppPageWaiter::Expectation::kManifestOrLoadedNoManifest:
      return "kManifestOrLoadedNoManifest";
  }
}
}  // namespace

WebAppPageWaiter::WebAppPageWaiter(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
  CHECK(web_contents);
}

WebAppPageWaiter::~WebAppPageWaiter() = default;

WebAppPageWaiter& WebAppPageWaiter::ExpectUrl(const GURL& url) {
  return ExpectAnyUrl({url});
}

WebAppPageWaiter& WebAppPageWaiter::ExpectAnyUrl(base::flat_set<GURL> urls) {
  expected_urls_ = std::move(urls);
  return *this;
}

WebAppPageWaiter& WebAppPageWaiter::ExpectManifest(
    std::optional<webapps::ManifestId> manifest_id) {
  CHECK_EQ(expectation_, Expectation::kUnset);
  expectation_ = Expectation::kManifest;
  expected_manifest_id_ = std::move(manifest_id);
  return *this;
}

WebAppPageWaiter& WebAppPageWaiter::ExpectNoManifest() {
  CHECK_EQ(expectation_, Expectation::kUnset);
  expectation_ = Expectation::kNoManifest;
  return *this;
}

WebAppPageWaiter& WebAppPageWaiter::ManifestOrLoadedNoManifest() {
  CHECK_EQ(expectation_, Expectation::kUnset);
  expectation_ = Expectation::kManifestOrLoadedNoManifest;
  return *this;
}

testing::AssertionResult WebAppPageWaiter::WaitAndFlushCommands() {
  CHECK_NE(expectation_, Expectation::kUnset);
  CHECK(!wait_called_) << "The WebAppPageWaiter can only be used once.";
  wait_called_ = true;

  DVLOG(1) << "WaitAndFlushCommands started. Expectation: "
           << ToString(expectation_) << ", expected_manifest_id: "
           << (expected_manifest_id_ ? expected_manifest_id_->spec() : "none")
           << ", Current URL: " << web_contents()->GetVisibleURL();

  if (destroyed_) {
    return testing::AssertionFailure()
           << "WebContents destroyed before Wait() called";
  }

  // Wait for the expected URL to load first (if set), otherwise just wait for
  // current load to stop.
  is_waiting_for_url_load_ = true;
  if (!expected_urls_.empty()) {
    DVLOG(1) << "Waiting for URL matching condition. Current URL: "
             << web_contents()->GetVisibleURL();
    bool waitSuccess = base::test::RunUntil([&]() {
      if (!web_contents()) {
        return true;
      }
      bool is_web_contents_considered_loaded =
          !web_contents()->IsLoading() ||
          web_contents()->GetVisibleURL().path() == "/hung";
      bool url_matches_and_web_contents_loaded =
          UrlMatches() && is_web_contents_considered_loaded;
      return url_matches_and_web_contents_loaded || load_failed_;
    });
    if (!waitSuccess) {
      return testing::AssertionFailure()
             << "Timed out waiting for URL. Current URL: "
             << web_contents()->GetVisibleURL()
             << ", Expected: " << base::ToString(expected_urls_);
    }
    if (destroyed_) {
      return testing::AssertionFailure()
             << "WebContents destroyed while waiting for URL";
    }
    if (load_failed_) {
      return testing::AssertionFailure() << "URL load failed. Current URL: "
                                         << web_contents()->GetVisibleURL();
    }
    DVLOG(1) << "URL matched and loaded. Final URL: "
             << web_contents()->GetVisibleURL();
  } else {
    DVLOG(1) << "No URL matcher. Waiting for load stop. Current URL: "
             << web_contents()->GetVisibleURL();
    content::WaitForLoadStop(web_contents());
    DVLOG(1) << "Load stopped. Current URL: "
             << web_contents()->GetVisibleURL();
    if (destroyed_) {
      return testing::AssertionFailure()
             << "WebContents destroyed during WaitForLoadStop";
    }
  }
  is_waiting_for_url_load_ = false;

  document_loaded_ =
      web_contents()->IsDocumentOnLoadCompletedInPrimaryMainFrame();

  WebAppTabHelper* tab_helper =
      WebAppTabHelper::FromWebContents(web_contents());

  if (!tab_helper) {
    switch (expectation_) {
      case Expectation::kUnset:
        break;
      case Expectation::kManifest:
        return testing::AssertionFailure()
               << "No WebAppTabHelper attached to WebContents, cannot wait for "
                  "manifest.";
      case Expectation::kNoManifest:
        break;
      case Expectation::kManifestOrLoadedNoManifest:
        break;
    }
  }

  // It's possible the manifest was already processed while we were waiting
  // for the load to stop. If so, we can note that down right away!
  if (tab_helper && tab_helper->manifest_processed_for_current_page()) {
    processed_manifest_id_ =
        tab_helper->last_processed_manifest_id_for_current_page();
    DVLOG(1) << "Manifest already processed for current page: "
             << (processed_manifest_id_ ? processed_manifest_id_->spec()
                                        : "none");
    if (!expected_manifest_id_.has_value() ||
        processed_manifest_id_ == expected_manifest_id_) {
      matching_manifest_processed_ = true;
    }
  }

  if (tab_helper) {
    DVLOG(1) << "Subscribing to OnManifestProcessed.";
    manifest_subscription_ =
        tab_helper->AddOnManifestProcessedCallbackForTesting(
            base::BindRepeating(&WebAppPageWaiter::OnManifestProcessed,
                                base::Unretained(this)));
  }

  MaybeQuit();

  DVLOG(1) << "Starting main run loop.";
  run_loop_.Run();
  DVLOG(1) << "Main run loop finished. destroyed: " << destroyed_;

  if (destroyed_) {
    return testing::AssertionFailure() << "WebContents destroyed while waiting";
  }

  bool has_manifest_url =
      web_contents()->GetPrimaryPage().GetManifestUrl().has_value();

  switch (expectation_) {
    case Expectation::kUnset:
      NOTREACHED();
    case Expectation::kManifest:
      if (!matching_manifest_processed_) {
        if (!has_manifest_url) {
          return testing::AssertionFailure()
                 << "Expected manifest but no manifest URL was found on the "
                    "page before loading completed or was stopped.";
        }
        if (processed_manifest_id_.has_value()) {
          return testing::AssertionFailure()
                 << "Expected manifest ID: " << expected_manifest_id_->spec()
                 << " but got: " << processed_manifest_id_->spec();
        }
        return testing::AssertionFailure()
               << "Expected manifest but none was processed";
      }
      CHECK(processed_manifest_id_)
          << "Cannot have a matching manifest without a processed manifest id.";

      break;
    case Expectation::kNoManifest:
      if (has_manifest_url) {
        return testing::AssertionFailure()
               << "Expected no manifest but found manifest URL: "
               << web_contents()->GetPrimaryPage().GetManifestUrl()->spec()
               << (processed_manifest_id_.has_value()
                       ? "With id " + processed_manifest_id_->spec()
                       : "");
      }
      break;
    case Expectation::kManifestOrLoadedNoManifest:
      if (!document_loaded_ && !load_failed_ && !matching_manifest_processed_) {
        return testing::AssertionFailure()
               << "Timed out waiting for page load or manifest processing.";
      }
      break;
  }
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());

  // Stop observing to prevent re-entry if the commands or test messes with this
  // web contents too. (e.g. a command navigates it, or the test has this waiter
  // still constructed / on the stack and continues to use the web contents.
  Observe(nullptr);

  // Wait for all commands to complete in the command manager to ensure any
  // spawned tasks are done.
  WebAppProvider* provider = WebAppProvider::GetForTest(profile);
  if (provider) {
    provider->command_manager().AwaitAllCommandsCompleteForTesting();
  }

  return testing::AssertionSuccess();
}

void WebAppPageWaiter::DidStopLoading() {
  DVLOG(1) << "DidStopLoading. Current URL: "
           << web_contents()->GetVisibleURL();
  MaybeQuit();
}

void WebAppPageWaiter::DocumentOnLoadCompletedInPrimaryMainFrame() {
  DVLOG(1) << "DocumentOnLoadCompletedInPrimaryMainFrame. Current URL: "
           << web_contents()->GetVisibleURL();
  MaybeQuit();
}

void WebAppPageWaiter::DidFailLoad(content::RenderFrameHost* render_frame_host,
                                   const GURL& validated_url,
                                   int error_code) {
  DVLOG(1) << "DidFailLoad. URL: " << validated_url
           << ", error_code: " << error_code << ", IsInPrimaryMainFrame: "
           << (render_frame_host && render_frame_host->IsInPrimaryMainFrame());
  if (render_frame_host && render_frame_host->IsInPrimaryMainFrame()) {
    load_failed_ = true;
    MaybeQuit();
  }
}

void WebAppPageWaiter::WebContentsDestroyed() {
  DVLOG(1) << "WebContentsDestroyed.";
  destroyed_ = true;
  Observe(nullptr);
  run_loop_.Quit();
}

void WebAppPageWaiter::OnManifestProcessed(
    const webapps::ManifestId& manifest_id) {
  DVLOG(1) << "OnManifestProcessed: " << manifest_id.spec()
           << ", expected_manifest_id: "
           << (expected_manifest_id_ ? expected_manifest_id_->spec() : "none");
  processed_manifest_id_ = manifest_id;
  if (!expected_manifest_id_.has_value() ||
      processed_manifest_id_ == expected_manifest_id_) {
    matching_manifest_processed_ = true;
  }
  // MaybeQuit & the waiter should always quit if we are waiting for a manifest
  // and it matches - thus we don't need to handle the case where
  // matching_manifest_processed_ was set to 'true' BEFORE this method call.
  MaybeQuit();
}

void WebAppPageWaiter::MaybeQuit() {
  if (is_waiting_for_url_load_) {
    DVLOG(1) << "MaybeQuit: Skipping because still waiting for URL load.";
    return;
  }

  if (destroyed_) {
    DVLOG(1) << "MaybeQuit: Quitting because destroyed.";
    run_loop_.Quit();
    return;
  }

  document_loaded_ =
      web_contents()->IsDocumentOnLoadCompletedInPrimaryMainFrame();

  // Note: This method is NOT responsible for making test assertions, it is
  // responsible ending waiting, and `WaitAndFlushCommands` is responsible
  // for checking test assertions.
  bool has_manifest_url =
      web_contents()->GetPrimaryPage().GetManifestUrl().has_value();
  bool has_load_completed_enough_for_manifest_url_check =
      document_loaded_ || load_failed_;

  DVLOG(2) << "MaybeQuit. Expectation: " << ToString(expectation_)
           << ", document_loaded: " << document_loaded_
           << ", load_failed: " << load_failed_
           << ", matching_manifest_processed: " << matching_manifest_processed_
           << ", has_manifest_url: " << has_manifest_url
           << ", webapps_enabled: " << (!!manifest_subscription_);

  switch (expectation_) {
    case Expectation::kManifestOrLoadedNoManifest:
      if (matching_manifest_processed_) {
        DVLOG(1) << "MaybeQuit: Quitting because matching manifest processed.";
        run_loop_.Quit();
        return;
      }
      if (has_load_completed_enough_for_manifest_url_check) {
        bool webapps_enabled_for_web_contents = !!manifest_subscription_;
        if (webapps_enabled_for_web_contents && has_manifest_url &&
            !matching_manifest_processed_) {
          DVLOG(1) << "MaybeQuit: Keeping waiting because has manifest URL but "
                      "not processed yet.";
          // Keep waiting for manifest.
          return;
        }
        DVLOG(1)
            << "MaybeQuit: Quitting because load completed and no manifest "
               "to wait for.";
        run_loop_.Quit();
      }
      return;
    case Expectation::kManifest:
      // If the manifest url doesn't exist and load has completed, assume that
      // there will not be a manifest set on the page, and fail early so we
      // don't time out.
      if (matching_manifest_processed_) {
        DVLOG(1) << "MaybeQuit: Quitting because matching manifest processed "
                    "(kManifest).";
        run_loop_.Quit();
      } else if (has_load_completed_enough_for_manifest_url_check &&
                 !has_manifest_url) {
        DVLOG(1)
            << "MaybeQuit: Quitting because load completed and no manifest "
               "URL found.";
        run_loop_.Quit();
      }
      return;
    case Expectation::kNoManifest:
      if (has_load_completed_enough_for_manifest_url_check ||
          has_manifest_url) {
        DVLOG(1) << "MaybeQuit: Quitting (kNoManifest). has_load_completed: "
                 << has_load_completed_enough_for_manifest_url_check
                 << ", has_manifest_url: " << has_manifest_url;
        run_loop_.Quit();
      }
      break;
    case Expectation::kUnset:
      NOTREACHED();
  }
}

bool WebAppPageWaiter::UrlMatches() const {
  CHECK(!expected_urls_.empty());
  content::NavigationController& controller = web_contents()->GetController();
  content::NavigationEntry* entry = controller.GetVisibleEntry();
  if (!entry) {
    return false;
  }
  if (expected_urls_.contains(entry->GetURL())) {
    return true;
  }
  // It is likely a mistake if the expected url was redirected, so print a
  // warning.
  if (expected_urls_.contains(entry->GetOriginalRequestURL())) {
    DLOG(WARNING)
        << "WebAppPageWaiter: Match found via OriginalRequestURL: "
        << entry->GetOriginalRequestURL() << " (URL is " << entry->GetURL()
        << "). Tests may want to explicitly expect this redirected-to url.";
    return true;
  }

  const std::vector<GURL>& chain = entry->GetRedirectChain();
  for (const GURL& redirect_chain_url : chain) {
    if (expected_urls_.contains(redirect_chain_url)) {
      DLOG(WARNING)
          << "WebAppPageWaiter: Match found in redirect chain containing "
          << redirect_chain_url << " (Current URL is " << entry->GetURL()
          << "). Tests may want to explicitly expect this redirected-to url.";
      return true;
    }
  }
  return false;
}

}  // namespace web_app::test
