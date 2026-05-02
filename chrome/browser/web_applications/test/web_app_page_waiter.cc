// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/web_app_page_waiter.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "content/public/browser/page.h"
#include "content/public/browser/page_manifest_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"

namespace web_app::test {

WebAppPageWaiter::WebAppPageWaiter(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
  CHECK(web_contents);
}

WebAppPageWaiter::~WebAppPageWaiter() = default;

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

  if (destroyed_) {
    return testing::AssertionFailure()
           << "WebContents destroyed before Wait() called";
  }

  // We always start by ensuring the page has stopped loading. This gives us a
  // stable baseline and avoids flakes from ongoing navigations.
  content::WaitForLoadStop(web_contents());

  if (destroyed_) {
    return testing::AssertionFailure()
           << "WebContents destroyed during WaitForLoadStop";
  }

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
    if (!expected_manifest_id_.has_value() ||
        processed_manifest_id_ == expected_manifest_id_) {
      matching_manifest_processed_ = true;
    }
  }

  if (tab_helper) {
    manifest_subscription_ =
        tab_helper->AddOnManifestProcessedCallbackForTesting(
            base::BindRepeating(&WebAppPageWaiter::OnManifestProcessed,
                                base::Unretained(this)));
  }

  MaybeQuit();

  run_loop_.Run();

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

  // Wait for all commands to complete in the command manager to ensure any
  // spawned tasks are done.
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  WebAppProvider* provider = WebAppProvider::GetForTest(profile);
  if (provider) {
    provider->command_manager().AwaitAllCommandsCompleteForTesting();
  }

  return testing::AssertionSuccess();
}

void WebAppPageWaiter::DidStopLoading() {
  MaybeQuit();
}

void WebAppPageWaiter::DocumentOnLoadCompletedInPrimaryMainFrame() {
  MaybeQuit();
}

void WebAppPageWaiter::DidFailLoad(content::RenderFrameHost* render_frame_host,
                                   const GURL& validated_url,
                                   int error_code) {
  if (render_frame_host && render_frame_host->IsInPrimaryMainFrame()) {
    load_failed_ = true;
    MaybeQuit();
  }
}

void WebAppPageWaiter::WebContentsDestroyed() {
  destroyed_ = true;
  Observe(nullptr);
  run_loop_.Quit();
}

void WebAppPageWaiter::OnManifestProcessed(
    const webapps::ManifestId& manifest_id) {
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
  if (destroyed_) {
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
  switch (expectation_) {
    case Expectation::kManifestOrLoadedNoManifest:
      if (matching_manifest_processed_) {
        run_loop_.Quit();
        return;
      }
      if (has_load_completed_enough_for_manifest_url_check) {
        bool webapps_enabled_for_web_contents = !!manifest_subscription_;
        if (webapps_enabled_for_web_contents && has_manifest_url &&
            !matching_manifest_processed_) {
          // Keep waiting for manifest.
          return;
        }
        run_loop_.Quit();
      }
      return;
    case Expectation::kManifest:
      // If the manifest url doesn't exist and load has completed, assume that
      // there will not be a manifest set on the page, and fail early so we
      // don't time out.
      if (matching_manifest_processed_ ||
          (has_load_completed_enough_for_manifest_url_check &&
           !has_manifest_url)) {
        run_loop_.Quit();
      }
      return;
    case Expectation::kNoManifest:
      if (has_load_completed_enough_for_manifest_url_check ||
          has_manifest_url) {
        run_loop_.Quit();
      }
      break;
    case Expectation::kUnset:
      NOTREACHED();
  }
}

}  // namespace web_app::test
