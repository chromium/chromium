// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/parse_manifest_from_manifest_url_command.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_lock.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_contents/web_contents_manager.h"
#include "components/webapps/browser/web_contents/web_app_url_loader.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/child_process_id.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "url/gurl.h"

namespace web_app {
namespace {

// Helper command that acquires `SharedWebContentsLock`, navigates the shared
// `WebContents` to `target_url`, and reports its committed `SiteInstance` and
// `ChildProcessId` before releasing the lock.
class NavigateSharedWebContentsCommand
    : public WebAppCommand<SharedWebContentsLock> {
 public:
  NavigateSharedWebContentsCommand(
      GURL target_url,
      base::OnceCallback<void(scoped_refptr<content::SiteInstance>,
                              content::ChildProcessId)> on_navigated)
      : WebAppCommand<SharedWebContentsLock>("NavigateSharedWebContentsCommand",
                                             SharedWebContentsLockDescription(),
                                             base::DoNothing()),
        target_url_(std::move(target_url)),
        on_navigated_(std::move(on_navigated)) {}

 protected:
  void StartWithLock(std::unique_ptr<SharedWebContentsLock> lock) override {
    lock_ = std::move(lock);
    url_loader_ = lock_->web_contents_manager().CreateUrlLoader();
    url_loader_->LoadUrl(
        target_url_, &lock_->shared_web_contents(),
        webapps::WebAppUrlLoader::UrlComparison::kExact,
        base::BindOnce(&NavigateSharedWebContentsCommand::OnLoaded,
                       weak_factory_.GetWeakPtr()));
  }

 private:
  void OnLoaded(webapps::WebAppUrlLoaderResult result) {
    CHECK_EQ(result, webapps::WebAppUrlLoaderResult::kUrlLoaded);
    content::RenderFrameHost* rfh =
        lock_->shared_web_contents().GetPrimaryMainFrame();
    std::move(on_navigated_)
        .Run(rfh->GetSiteInstance(), rfh->GetProcess()->GetID());
    CompleteAndSelfDestruct(CommandResult::kSuccess);
  }

  GURL target_url_;
  base::OnceCallback<void(scoped_refptr<content::SiteInstance>,
                          content::ChildProcessId)>
      on_navigated_;
  std::unique_ptr<SharedWebContentsLock> lock_;
  std::unique_ptr<webapps::WebAppUrlLoader> url_loader_;
  base::WeakPtrFactory<NavigateSharedWebContentsCommand> weak_factory_{this};
};

class ParseManifestFromManifestUrlCommandTest : public WebAppBrowserTestBase {
 public:
  ParseManifestResult ParseAndAwaitResult(
      const GURL& manifest_url,
      const std::string& manifest_contents) {
    base::test::TestFuture<ParseManifestResult> future;
    provider().command_manager().ScheduleCommand(
        std::make_unique<ParseManifestFromManifestUrlCommand>(
            manifest_url, manifest_contents, future.GetCallback()));
    return future.Take();
  }
};

// A valid manifest with a relative start_url resolves against manifest_url.
IN_PROC_BROWSER_TEST_F(ParseManifestFromManifestUrlCommandTest,
                       RelativeStartUrlResolvesAgainstManifestUrl) {
  const GURL kManifestUrl("https://example.com/app/manifest.json");
  const std::string kManifest = R"json({
    "name": "Test App",
    "start_url": "/start"
  })json";

  ParseManifestResult result = ParseAndAwaitResult(kManifestUrl, kManifest);

  ASSERT_TRUE(result.has_value());
  const blink::Manifest& manifest = result.value();
  EXPECT_EQ(manifest.start_url(), GURL("https://example.com/start"));
  EXPECT_EQ(manifest.name(), u"Test App");
}

// A relative start_url without leading slash resolves relative to the manifest
// file's directory.
IN_PROC_BROWSER_TEST_F(ParseManifestFromManifestUrlCommandTest,
                       RelativeStartUrlResolvesFromManifestDirectory) {
  const GURL kManifestUrl("https://example.com/app/manifest.json");
  const std::string kManifest = R"json({
    "name": "Test App",
    "start_url": "index.html"
  })json";

  ParseManifestResult result = ParseAndAwaitResult(kManifestUrl, kManifest);

  ASSERT_TRUE(result.has_value());
  const blink::Manifest& manifest = result.value();
  EXPECT_EQ(manifest.start_url(), GURL("https://example.com/app/index.html"));
}

// A missing start_url fails parsing requirements.
IN_PROC_BROWSER_TEST_F(ParseManifestFromManifestUrlCommandTest,
                       MissingStartUrlFails) {
  const GURL kManifestUrl("https://example.com/app/manifest.json");
  const std::string kManifest = R"json({
    "name": "Test App"
  })json";

  ParseManifestResult result = ParseAndAwaitResult(kManifestUrl, kManifest);

  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), ParseManifestError::kStartUrlInvalid);
}

// A cross-origin absolute start_url is rejected by ManifestParser. The
// resulting manifest has no valid start_url, causing the command to fail.
IN_PROC_BROWSER_TEST_F(ParseManifestFromManifestUrlCommandTest,
                       CrossOriginStartUrlFails) {
  const GURL kManifestUrl("https://example.com/app/manifest.json");
  const std::string kManifest = R"json({
    "name": "Test App",
    "start_url": "https://evil.com/start"
  })json";

  ParseManifestResult result = ParseAndAwaitResult(kManifestUrl, kManifest);

  // ManifestParser rejects cross-origin start_url, leaving
  // has_valid_specified_start_url false.
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), ParseManifestError::kStartUrlInvalid);
}

// Invalid JSON fails parsing requirements.
IN_PROC_BROWSER_TEST_F(ParseManifestFromManifestUrlCommandTest,
                       InvalidJsonFails) {
  const GURL kManifestUrl("https://example.com/manifest.json");
  const std::string kManifest = "this is not json {{{";

  ParseManifestResult result = ParseAndAwaitResult(kManifestUrl, kManifest);

  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), ParseManifestError::kEmptyOrInvalidManifest);
}

// A manifest with no name or short_name fails the required fields check.
IN_PROC_BROWSER_TEST_F(ParseManifestFromManifestUrlCommandTest,
                       MissingNameFails) {
  const GURL kManifestUrl("https://example.com/manifest.json");
  const std::string kManifest = R"json({
    "start_url": "/"
  })json";

  ParseManifestResult result = ParseAndAwaitResult(kManifestUrl, kManifest);

  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(),
            ParseManifestError::kManifestMissingNameOrShortName);
}

// A valid manifest with an explicit id field preserves has_custom_id.
IN_PROC_BROWSER_TEST_F(ParseManifestFromManifestUrlCommandTest,
                       ManifestWithCustomId) {
  const GURL kManifestUrl("https://example.com/app/manifest.json");
  const std::string kManifest = R"json({
    "name": "Test App",
    "start_url": "/",
    "id": "/my-app-id"
  })json";

  ParseManifestResult result = ParseAndAwaitResult(kManifestUrl, kManifest);

  ASSERT_TRUE(result.has_value());
  const blink::Manifest& manifest = result.value();
  EXPECT_TRUE(manifest.has_custom_id());
  EXPECT_EQ(manifest.id(), GURL("https://example.com/my-app-id"));
}

// A valid manifest without an explicit id field has has_custom_id = false.
IN_PROC_BROWSER_TEST_F(ParseManifestFromManifestUrlCommandTest,
                       ManifestWithoutCustomId) {
  const GURL kManifestUrl("https://example.com/app/manifest.json");
  const std::string kManifest = R"json({
    "name": "Test App",
    "start_url": "/"
  })json";

  ParseManifestResult result = ParseAndAwaitResult(kManifestUrl, kManifest);

  ASSERT_TRUE(result.has_value());
  const blink::Manifest& manifest = result.value();
  EXPECT_FALSE(manifest.has_custom_id());
}

// Verifies that when a prior command leaves `shared_web_contents_` committed to
// a cross-origin site (`evil.test`), `ParseManifestFromManifestUrlCommand`
// swaps `shared_web_contents_` into a fresh, unrelated `SiteInstance` and a
// different renderer process before parsing the manifest.
IN_PROC_BROWSER_TEST_F(ParseManifestFromManifestUrlCommandTest,
                       IsolatesSharedWebContentsFromPriorOriginSiteInstance) {
  const GURL prior_origin_url =
      embedded_https_test_server().GetURL("evil.test", "/title1.html");
  scoped_refptr<content::SiteInstance> prior_site_instance;
  content::ChildProcessId prior_process_id;

  provider().command_manager().ScheduleCommand(
      std::make_unique<NavigateSharedWebContentsCommand>(
          prior_origin_url,
          base::BindLambdaForTesting(
              [&](scoped_refptr<content::SiteInstance> site_instance,
                  content::ChildProcessId process_id) {
                prior_site_instance = std::move(site_instance);
                prior_process_id = process_id;
              })));

  const GURL kManifestUrl("https://example.com/app/manifest.json");
  const std::string kManifest = R"json({
    "name": "Test App",
    "start_url": "/app/start",
    "id": "/app/my-app"
  })json";

  scoped_refptr<content::SiteInstance> parse_site_instance;
  content::ChildProcessId parse_process_id;
  base::test::TestFuture<ParseManifestResult> future;
  provider().command_manager().ScheduleCommand(
      std::make_unique<ParseManifestFromManifestUrlCommand>(
          kManifestUrl, kManifest,
          base::BindLambdaForTesting([&](ParseManifestResult result) {
            // This callback runs synchronously inside OnCommandComplete before
            // the posted ClearSharedWebContentsIfUnused task executes.
            content::WebContents* shared_wc =
                provider().command_manager().web_contents_for_testing();
            if (shared_wc) {
              content::RenderFrameHost* rfh = shared_wc->GetPrimaryMainFrame();
              parse_site_instance = rfh->GetSiteInstance();
              parse_process_id = rfh->GetProcess()->GetID();
            }
            future.SetValue(std::move(result));
          })));

  ParseManifestResult result = future.Take();
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(prior_site_instance);
  ASSERT_TRUE(parse_site_instance);
  EXPECT_NE(prior_site_instance.get(), parse_site_instance.get());
  EXPECT_FALSE(
      prior_site_instance->IsRelatedSiteInstance(parse_site_instance.get()));
  EXPECT_NE(prior_process_id, parse_process_id);
}

}  // namespace
}  // namespace web_app
