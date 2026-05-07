// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_PAGE_WAITER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_PAGE_WAITER_H_

#include <optional>

#include "base/callback_list.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback_forward.h"
#include "base/run_loop.h"
#include "base/timer/timer.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/web_contents_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
class WebContents;
}

namespace web_app::test {

// A friendly helper class for tests to wait for all web app activity on a page
// to settle down.
//
// Testing page loads and manifests can be tricky and prone to flakes! This
// class tries to make that easier by ensuring the page has fully stopped
// loading first (using `content::WaitForLoadStop()`). It then waits for the
// document's `onload` event and/or for manifest processing to complete,
// depending on the expectation specified in the factory methods below. Finally,
// it flushes any pending WebAppProvider commands so you can be sure the system
// is in a stable state for your test assertions.
//
// Please use this instead of arbitrary timeouts or assuming that navigation
// completion means everything is done. Your fellow developers (and the bots)
// will thank you!
//
// Note: This class will hang if it is waiting for a manifest, there is a
// manifest url on the page, but it fails to parse or has some other kind of
// error (like a network error).
class WebAppPageWaiter : public content::WebContentsObserver {
 public:
  enum class Expectation {
    kUnset,
    kManifest,
    kNoManifest,
    kManifestOrLoadedNoManifest,
  };

  explicit WebAppPageWaiter(content::WebContents* web_contents);
  ~WebAppPageWaiter() override;

  // Configures the waiter to expect the WebContents to navigate to and finish
  // loading the specified URL.
  //
  // If the page redirects during navigation, and the redirect starts from
  // the expected URL, the waiter will automatically update its expectation
  // to the redirect destination and log a warning.
  WebAppPageWaiter& ExpectUrl(const GURL& url);

  // Same as above, for any of the given urls.
  WebAppPageWaiter& ExpectAnyUrl(base::flat_set<GURL> urls);

  // Configures the waiter to expect a valid manifest to be discovered and
  // processed.
  //
  // `WaitAndFlushCommands()` will report a failure if:
  // - The page finishes loading but no manifest URL (that successfully parsed)
  //   was found.
  // - An expected `manifest_id` was provided, but the processed manifest's ID
  //   didn't match.
  WebAppPageWaiter& ExpectManifest(
      std::optional<webapps::ManifestId> manifest_id = std::nullopt);

  // Configures the waiter to expect that no manifest will be found on the page.
  //
  // `WaitAndFlushCommands()` will report a failure if a manifest URL is found
  // after the page completes loading.
  WebAppPageWaiter& ExpectNoManifest();

  // A flexible mode that is useful when you might or might not have a manifest.
  // It waits for a manifest to be processed if one is found, but will return
  // promptly if the page finishes loading without one.
  WebAppPageWaiter& ManifestOrLoadedNoManifest();

  // The main entry point! This method blocks until your expectations are met
  // or we determine that they won't be. It also ensures all spawned commands
  // in the WebAppProvider are complete before returning.
  // Returns an AssertionResult so you can use it with EXPECT_TRUE or
  // ASSERT_TRUE.
  [[nodiscard]] testing::AssertionResult WaitAndFlushCommands();

  // content::WebContentsObserver:
  void DidStopLoading() override;
  void DocumentOnLoadCompletedInPrimaryMainFrame() override;
  void DidFailLoad(content::RenderFrameHost* render_frame_host,
                   const GURL& validated_url,
                   int error_code) override;
  void WebContentsDestroyed() override;

 private:
  void OnManifestProcessed(const webapps::ManifestId& manifest_id);
  void MaybeQuit();

  bool UrlMatches() const;

  Expectation expectation_ = Expectation::kUnset;
  std::optional<webapps::ManifestId> expected_manifest_id_;
  base::flat_set<GURL> expected_urls_;
  std::optional<webapps::ManifestId> processed_manifest_id_;

  bool is_waiting_for_url_load_ = false;

  bool document_loaded_ = false;
  bool load_failed_ = false;
  bool matching_manifest_processed_ = false;
  bool destroyed_ = false;
  bool wait_called_ = false;

  base::CallbackListSubscription manifest_subscription_;
  base::RunLoop run_loop_;
};

}  // namespace web_app::test

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_PAGE_WAITER_H_
