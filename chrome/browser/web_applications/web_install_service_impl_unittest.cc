// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_install_service_impl.h"

#include <array>
#include <optional>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/fake_web_contents_manager.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_contents/web_app_data_retriever.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/installable/ml_install_operation_tracker.h"
#include "components/webapps/browser/installable/ml_installability_promoter.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "third_party/blink/public/mojom/web_install/web_install.mojom.h"
#include "url/gurl.h"

namespace web_app {
namespace {

constexpr char kInstallApiResultUma[] = "WebApp.WebInstallApi.Result";
constexpr char kInstallApiTypeUma[] = "WebApp.WebInstallApi.InstallType";
constexpr char kVariantedInstallResultUma[] =
    "WebApp.WebInstallService.Api.Result";
constexpr char kVariantedInstallTypeUma[] =
    "WebApp.WebInstallService.Api.InstallType";
constexpr char kInstallElementResultUma[] = "WebApp.WebInstallElement.Result";
constexpr char kInstallElementTypeUma[] =
    "WebApp.WebInstallElement.InstallType";
constexpr char kVariantedElementResultUma[] =
    "WebApp.WebInstallService.Element.Result";
constexpr char kVariantedElementTypeUma[] =
    "WebApp.WebInstallService.Element.InstallType";

constexpr char kDocumentUrl[] = "https://requesting-app.com/index.html";
constexpr char kManifestUrl[] = "https://example.com/app/manifest.json";
constexpr char kIconUrl[] = "https://example.com/app/icon.png";

// Unit tests for WebInstallServiceImpl shared logic. These tests cover
// code paths common to both `navigator.install()` (JS API) and the
// `<install>` element, without going through either Blink entry point.
class WebInstallServiceImplTest : public WebAppTest {
 public:
  WebInstallServiceImplTest() {
    scoped_feature_list_.InitWithFeatures({blink::features::kWebAppInstallation,
                                           blink::features::kInstallElement},
                                          {});
  }
  WebInstallServiceImplTest(const WebInstallServiceImplTest&) = delete;
  WebInstallServiceImplTest& operator=(const WebInstallServiceImplTest&) =
      delete;
  ~WebInstallServiceImplTest() override = default;

  void SetUp() override {
    WebAppTest::SetUp();
    test::AwaitStartWebAppProviderAndSubsystems(profile());

    // Navigate to an HTTPS page so CreateIfAllowed succeeds.
    NavigateAndCommit(GURL(kDocumentUrl));

    webapps::MLInstallabilityPromoter::CreateForWebContents(web_contents());

    // Mark the URL as loaded in the FakeWebContentsManager so data
    // retrievers know which page state to use.
    fake_web_contents_manager().SetUrlLoaded(web_contents(),
                                             GURL(kDocumentUrl));
  }

  // Creates a WebInstallServiceImpl bound to `service_remote_` via the
  // primary main frame's RenderFrameHost.
  void BindService() {
    WebInstallServiceImpl::CreateIfAllowed(
        web_contents()->GetPrimaryMainFrame(),
        service_remote_.BindNewPipeAndPassReceiver());
  }

  // Calls Install() with no options (current document install) and waits
  // for the mojo callback.
  std::pair<blink::mojom::WebInstallServiceResult, GURL>
  InstallFromApiCurrentDocument() {
    base::test::TestFuture<blink::mojom::WebInstallServiceResult, const GURL&>
        future;
    service_remote_->Install(/*options=*/nullptr, future.GetCallback());
    EXPECT_TRUE(future.Wait());
    return {future.Get<blink::mojom::WebInstallServiceResult>(),
            future.Get<GURL>()};
  }

  // Calls InstallFromElement() with no options (current document) and waits
  // for the mojo callback.
  std::pair<blink::mojom::WebInstallServiceResult, GURL>
  InstallFromElementCurrentDocument() {
    base::test::TestFuture<blink::mojom::WebInstallServiceResult, const GURL&>
        future;
    service_remote_->InstallFromElement(/*options=*/nullptr,
                                        future.GetCallback());
    EXPECT_TRUE(future.Wait());
    return {future.Get<blink::mojom::WebInstallServiceResult>(),
            future.Get<GURL>()};
  }

  // Calls Install() with an install_url (background document install) and
  // waits for the mojo callback.
  std::pair<blink::mojom::WebInstallServiceResult, GURL> InstallFromUrl(
      const GURL& install_url,
      const std::optional<GURL>& manifest_id = std::nullopt) {
    auto options = blink::mojom::InstallOptions::New();
    options->install_url = install_url;
    options->manifest_id = manifest_id;

    base::test::TestFuture<blink::mojom::WebInstallServiceResult, const GURL&>
        future;
    service_remote_->Install(std::move(options), future.GetCallback());
    EXPECT_TRUE(future.Wait());
    return {future.Get<blink::mojom::WebInstallServiceResult>(),
            future.Get<GURL>()};
  }

  // Creates a manifest with the given properties. If `custom_id` is provided,
  // sets the id field explicitly. Otherwise, the FakeWebContentsManager will
  // default it to start_url (and set has_custom_id = false).
  blink::mojom::ManifestPtr CreateManifest(
      const GURL& start_url,
      const std::optional<GURL>& custom_id = std::nullopt) {
    auto manifest = blink::mojom::Manifest::New();
    manifest->name = u"Test App";
    manifest->short_name = u"Test";
    manifest->start_url = start_url;
    manifest->display = blink::mojom::DisplayMode::kStandalone;

    if (custom_id) {
      manifest->id = custom_id.value();
    }

    blink::Manifest::ImageResource icon;
    icon.src = GURL(kIconUrl);
    icon.sizes = {{144, 144}};
    icon.purpose = {blink::mojom::ManifestImageResource_Purpose::ANY};
    manifest->icons = {icon};

    return manifest;
  }

  // Populates the FakePageState for |url| with the given manifest.
  void SetupPageWithManifest(const GURL& url,
                             blink::mojom::ManifestPtr manifest) {
    auto& page_state = fake_web_contents_manager().GetOrCreatePageState(url);
    page_state.has_service_worker = true;
    page_state.manifest_before_default_processing = std::move(manifest);
    page_state.valid_manifest_for_web_app = true;
    page_state.error_code = webapps::InstallableStatusCode::NO_ERROR_DETECTED;
    page_state.manifest_url = GURL(kManifestUrl);
  }

 private:
  mojo::Remote<blink::mojom::WebInstallService> service_remote_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

///////////////////////////////////////////////////////////////////////////////
// Current document manifest validation tests.
// These test the shared manifest validation in
// OnGotManifestForCurrentDocumentInstall.
//
// These tests use Install() (API entry point) but only verify the shared
// validation logic. The UMA routing to element-specific histograms is
// tested separately in InstallFromElement_ElementUmaHistograms.
///////////////////////////////////////////////////////////////////////////////

// No manifest on the current document. Expect a DataError.
TEST_F(WebInstallServiceImplTest, CurrentDocument_NoManifest) {
  base::HistogramTester histograms;
  // Don't create page state - the fake will return "manifest not found" error.

  BindService();
  auto [result, manifest_id] = InstallFromApiCurrentDocument();

  EXPECT_EQ(result, blink::mojom::WebInstallServiceResult::kDataError);
  EXPECT_TRUE(manifest_id.is_empty());

  histograms.ExpectBucketCount(
      kInstallApiResultUma, WebInstallServiceResult::kInstallCommandFailed, 1);
  histograms.ExpectBucketCount(kInstallApiTypeUma,
                               WebInstallServiceType::kCurrentDocument, 1);
  histograms.ExpectBucketCount(kVariantedInstallResultUma,
                               WebInstallServiceResult::kInstallCommandFailed,
                               1);
  histograms.ExpectBucketCount(kVariantedInstallTypeUma,
                               WebInstallServiceType::kCurrentDocument, 1);
}

// Manifest exists but has no custom id. Expect a DataError with
// kNoCustomManifestId.
TEST_F(WebInstallServiceImplTest, CurrentDocument_NoCustomManifestId) {
  base::HistogramTester histograms;
  // Create a manifest WITHOUT setting `id` -- the fake will default it to
  // start_url and set has_custom_id = false.
  auto manifest = CreateManifest(GURL(kDocumentUrl));
  SetupPageWithManifest(GURL(kDocumentUrl), std::move(manifest));

  BindService();
  auto [result, manifest_id] = InstallFromApiCurrentDocument();

  EXPECT_EQ(result, blink::mojom::WebInstallServiceResult::kDataError);
  EXPECT_TRUE(manifest_id.is_empty());

  histograms.ExpectBucketCount(kInstallApiResultUma,
                               WebInstallServiceResult::kNoCustomManifestId, 1);
  histograms.ExpectBucketCount(kInstallApiTypeUma,
                               WebInstallServiceType::kCurrentDocument, 1);
  histograms.ExpectBucketCount(kVariantedInstallResultUma,
                               WebInstallServiceResult::kNoCustomManifestId, 1);
  histograms.ExpectBucketCount(kVariantedInstallTypeUma,
                               WebInstallServiceType::kCurrentDocument, 1);
}

// Manifest has a custom id from a different origin than the document.
// Expect a DataError.
TEST_F(WebInstallServiceImplTest, CurrentDocument_CrossOriginManifestId) {
  base::HistogramTester histograms;
  // Set up a manifest whose id is from a different origin.
  GURL cross_origin_id("https://evil.com/some_id");
  auto manifest = CreateManifest(GURL(kDocumentUrl), cross_origin_id);
  SetupPageWithManifest(GURL(kDocumentUrl), std::move(manifest));

  BindService();
  auto [result, manifest_id] = InstallFromApiCurrentDocument();

  EXPECT_EQ(result, blink::mojom::WebInstallServiceResult::kDataError);
  EXPECT_TRUE(manifest_id.is_empty());

  histograms.ExpectBucketCount(
      kInstallApiResultUma, WebInstallServiceResult::kInstallCommandFailed, 1);
  histograms.ExpectBucketCount(kInstallApiTypeUma,
                               WebInstallServiceType::kCurrentDocument, 1);
}

///////////////////////////////////////////////////////////////////////////////
// Install URL scheme validation tests.
// These test the shared validation in Install() that rejects non-HTTPS URLs.
///////////////////////////////////////////////////////////////////////////////

// Install target with file:// scheme should fail.
TEST_F(WebInstallServiceImplTest, FileSchemeRejected) {
  base::HistogramTester histograms;

  BindService();
  auto [result, manifest_id] =
      InstallFromUrl(GURL("file:///tmp/app/index.html"));

  EXPECT_EQ(result, blink::mojom::WebInstallServiceResult::kAbortError);
  EXPECT_TRUE(manifest_id.is_empty());

  histograms.ExpectBucketCount(kInstallApiResultUma,
                               WebInstallServiceResult::kUnexpectedFailure, 1);
  histograms.ExpectBucketCount(kInstallApiTypeUma,
                               WebInstallServiceType::kBackgroundDocument, 1);
  histograms.ExpectBucketCount(kVariantedInstallResultUma,
                               WebInstallServiceResult::kUnexpectedFailure, 1);
  histograms.ExpectBucketCount(kVariantedInstallTypeUma,
                               WebInstallServiceType::kBackgroundDocument, 1);
}

///////////////////////////////////////////////////////////////////////////////
// UMA histogram tests.
// These verify that the correct install type and result are recorded for
// current vs background document installs, and that API vs element entry
// points route to their respective histogram variants.
///////////////////////////////////////////////////////////////////////////////

// Current document install (no options) records kCurrentDocument type and
// exercises the OnAppInstalled result code mapping.
TEST_F(WebInstallServiceImplTest, UmaInstallType_CurrentDocument) {
  base::HistogramTester histograms;

  // Set up a valid manifest with custom id.
  GURL custom_id("https://requesting-app.com/my_app_id");
  auto manifest = CreateManifest(GURL(kDocumentUrl), custom_id);
  SetupPageWithManifest(GURL(kDocumentUrl), std::move(manifest));

  BindService();
  // The FakeWebAppUiManager::TriggerInstallDialog calls the callback with
  // kWebAppProviderNotReady, which exercises OnAppInstalled.
  auto [result, manifest_id] = InstallFromApiCurrentDocument();

  // FakeWebAppUiManager::TriggerInstallDialog returns kWebAppProviderNotReady,
  // which maps to kAbortError (default case).
  EXPECT_EQ(result, blink::mojom::WebInstallServiceResult::kAbortError);
  EXPECT_TRUE(manifest_id.is_empty());

  // Verify install type UMA for API entry point.
  histograms.ExpectBucketCount(kInstallApiTypeUma,
                               WebInstallServiceType::kCurrentDocument, 1);
  histograms.ExpectBucketCount(kVariantedInstallTypeUma,
                               WebInstallServiceType::kCurrentDocument, 1);
  // Verify result code mapping.
  histograms.ExpectBucketCount(
      kInstallApiResultUma, WebInstallServiceResult::kInstallCommandFailed, 1);
}

// InstallFromElement records UMA to element-specific histograms.
TEST_F(WebInstallServiceImplTest, InstallFromElement_ElementUmaHistograms) {
  base::HistogramTester histograms;
  base::AutoReset<int> manifest_wait_timeout =
      WebAppDataRetriever::SetManifestWaitTimeoutForTesting(0);

  BindService();
  auto [result, manifest_id] = InstallFromElementCurrentDocument();

  // Should record to element UMA, not API UMA.
  histograms.ExpectBucketCount(kInstallElementTypeUma,
                               WebInstallServiceType::kCurrentDocument, 1);
  histograms.ExpectBucketCount(kVariantedElementTypeUma,
                               WebInstallServiceType::kCurrentDocument, 1);
  histograms.ExpectTotalCount(kInstallElementResultUma, 1);
  histograms.ExpectTotalCount(kVariantedElementResultUma, 1);
  // API histograms should be empty.
  histograms.ExpectTotalCount(kInstallApiTypeUma, 0);
  histograms.ExpectTotalCount(kInstallApiResultUma, 0);
}

///////////////////////////////////////////////////////////////////////////////
// CreateIfAllowed validation tests.
// These test the guard checks in the static factory method.
///////////////////////////////////////////////////////////////////////////////

// CreateIfAllowed from a child iframe resets the receiver.
TEST_F(WebInstallServiceImplTest, CreateIfAllowed_ChildFrame) {
  content::RenderFrameHost* child_rfh =
      content::RenderFrameHostTester::For(web_contents()->GetPrimaryMainFrame())
          ->AppendChild("child_frame");
  // Navigate to HTTPS so only the IsInPrimaryMainFrame() check rejects.
  child_rfh = content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("https://child.example.com"), child_rfh);

  mojo::Remote<blink::mojom::WebInstallService> remote;
  WebInstallServiceImpl::CreateIfAllowed(child_rfh,
                                         remote.BindNewPipeAndPassReceiver());

  // The receiver should have been reset because the frame is not the primary
  // main frame.
  base::test::TestFuture<blink::mojom::WebInstallServiceResult, const GURL&>
      future;
  remote->Install(/*options=*/nullptr, future.GetCallback());
  remote.FlushForTesting();
  EXPECT_FALSE(remote.is_connected());
}

// CreateIfAllowed with a non-HTTP(S) scheme resets the receiver.
TEST_F(WebInstallServiceImplTest, CreateIfAllowed_NonHttpsScheme) {
  // Navigate to a non-HTTPS URL.
  NavigateAndCommit(GURL("about:blank"));

  mojo::Remote<blink::mojom::WebInstallService> remote;
  WebInstallServiceImpl::CreateIfAllowed(web_contents()->GetPrimaryMainFrame(),
                                         remote.BindNewPipeAndPassReceiver());

  // The receiver should have been reset. Verify by trying to call a method
  // and confirming it disconnects.
  base::test::TestFuture<blink::mojom::WebInstallServiceResult, const GURL&>
      future;
  remote->Install(/*options=*/nullptr, future.GetCallback());
  // The pipe is reset, so flushing should cause a disconnect.
  remote.FlushForTesting();
  EXPECT_FALSE(remote.is_connected());
}

// If MLInstallabilityPromoter already has an install in progress, reject new
// installs via the current document flow.
TEST_F(WebInstallServiceImplTest, CurrentDocument_InstallAlreadyInProgress) {
  base::HistogramTester histograms;

  // Set up a valid manifest so we get past manifest validation.
  GURL custom_id("https://requesting-app.com/my_app_id");
  auto manifest = CreateManifest(GURL(kDocumentUrl), custom_id);
  SetupPageWithManifest(GURL(kDocumentUrl), std::move(manifest));

  // Register an install in progress before calling Install().
  webapps::MLInstallabilityPromoter* promoter =
      webapps::MLInstallabilityPromoter::FromWebContents(web_contents());
  ASSERT_TRUE(promoter);
  std::unique_ptr<webapps::MlInstallOperationTracker> tracker =
      promoter->RegisterCurrentInstallForWebContents(
          webapps::WebappInstallSource::WEB_INSTALL);
  ASSERT_TRUE(tracker);

  BindService();
  auto [result, manifest_id] = InstallFromApiCurrentDocument();

  EXPECT_EQ(result, blink::mojom::WebInstallServiceResult::kAbortError);
  EXPECT_TRUE(manifest_id.is_empty());

  histograms.ExpectBucketCount(kInstallApiResultUma,
                               WebInstallServiceResult::kUnexpectedFailure, 1);
  histograms.ExpectBucketCount(kInstallApiTypeUma,
                               WebInstallServiceType::kCurrentDocument, 1);
  histograms.ExpectBucketCount(kVariantedInstallResultUma,
                               WebInstallServiceResult::kUnexpectedFailure, 1);
  histograms.ExpectBucketCount(kVariantedInstallTypeUma,
                               WebInstallServiceType::kCurrentDocument, 1);
}

///////////////////////////////////////////////////////////////////////////////
// IsInstalled rate limiting tests.
// These verify that cross-origin IsInstalled queries are rate limited by both
// total count per document and minimum time interval between queries.
///////////////////////////////////////////////////////////////////////////////

constexpr char kCrossOriginUrl[] = "https://example.com/app";

// Small cap used in place of the production 100-query limit for cross-origin
// IsInstalled rate limiting. Tests reference this directly in loop bounds.
constexpr size_t kMaxQueries = 3;

// Minimum interval between consecutive cross-origin queries -- matches the
// production default of `g_min_cross_origin_query_interval`. Tests advance
// the mock clock by this amount to satisfy the interval throttle.
constexpr base::TimeDelta kMinCrossOriginQueryInterval = base::Seconds(1);

class WebInstallServiceImplRateLimitTest : public WebAppTest {
 public:
  WebInstallServiceImplRateLimitTest()
      : WebAppTest(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    scoped_feature_list_.InitWithFeatures({blink::features::kWebAppInstallation,
                                           blink::features::kInstallElement},
                                          {});
  }

  void SetUp() override {
    WebAppTest::SetUp();
    test::AwaitStartWebAppProviderAndSubsystems(profile());
    NavigateAndCommit(GURL(kDocumentUrl));
  }

  void BindService() {
    WebInstallServiceImpl::CreateIfAllowed(
        web_contents()->GetPrimaryMainFrame(),
        service_remote_.BindNewPipeAndPassReceiver());
  }

  bool IsInstalled(const GURL& install_url) {
    auto options = blink::mojom::InstallOptions::New();
    options->install_url = install_url;

    base::test::TestFuture<bool> future;
    service_remote_->IsInstalled(std::move(options), future.GetCallback());
    EXPECT_TRUE(future.Wait());

    return future.Get();
  }

  bool IsInstalledWithManifestId(const GURL& install_url,
                                 const GURL& manifest_id) {
    auto options = blink::mojom::InstallOptions::New();
    options->install_url = install_url;
    options->manifest_id = manifest_id;

    base::test::TestFuture<bool> future;
    service_remote_->IsInstalled(std::move(options), future.GetCallback());
    EXPECT_TRUE(future.Wait());

    return future.Get();
  }

  bool IsInstalledSameOrigin() {
    base::test::TestFuture<bool> future;
    service_remote_->IsInstalled(/*options=*/nullptr, future.GetCallback());
    EXPECT_TRUE(future.Wait());

    return future.Get();
  }

  mojo::Remote<blink::mojom::WebInstallService>& service_remote() {
    return service_remote_;
  }

 private:
  mojo::Remote<blink::mojom::WebInstallService> service_remote_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(WebInstallServiceImplRateLimitTest, SingleCrossOriginQuery_Allowed) {
  BindService();

  EXPECT_FALSE(IsInstalled(GURL(kCrossOriginUrl)));
  task_environment()->AdvanceClock(kMinCrossOriginQueryInterval);

  test::InstallDummyWebApp(profile(), "Cross Origin App",
                           GURL(kCrossOriginUrl));

  EXPECT_TRUE(IsInstalled(GURL(kCrossOriginUrl)));
}

// Same-origin queries should not be rate limited.
TEST_F(WebInstallServiceImplRateLimitTest, SameOriginQuery_NotRateLimited) {
  // Use a small limit to avoid looping 100 times.
  base::AutoReset<size_t> max_queries =
      WebInstallServiceImpl::SetMaxCrossOriginQueriesForTesting(kMaxQueries);

  test::InstallDummyWebApp(profile(), "Same Origin App", GURL(kDocumentUrl));
  BindService();

  // Issue well past the cross-origin cap; none should be blocked.
  for (size_t i = 0; i < kMaxQueries * 2; ++i) {
    EXPECT_TRUE(IsInstalledSameOrigin());
  }
}

// Basic count throttle: cross-origin queries are rejected once the
// per-document maximum has been reached.
TEST_F(WebInstallServiceImplRateLimitTest,
       CrossOriginQuery_RateLimitedAtCountCap) {
  // Use a small limit to avoid looping 100 times.
  base::AutoReset<size_t> max_queries =
      WebInstallServiceImpl::SetMaxCrossOriginQueriesForTesting(kMaxQueries);

  test::InstallDummyWebApp(profile(), "Cross Origin App",
                           GURL(kCrossOriginUrl));
  BindService();

  // Issue the maximum allowed queries, advancing time after each to avoid
  // the time-based rate limit on the next query.
  for (size_t i = 0; i < kMaxQueries; ++i) {
    // The cross origin app was installed, so these should succeed.
    EXPECT_TRUE(IsInstalled(GURL(kCrossOriginUrl)));
    task_environment()->AdvanceClock(kMinCrossOriginQueryInterval);
  }

  // The loop's final AdvanceClock cleared the time throttle, so this next
  // query is rejected purely on count.
  EXPECT_FALSE(IsInstalled(GURL(kCrossOriginUrl)));
}

// Basic interval throttle: a second cross-origin query issued before the
// minimum interval elapses is deferred (not rejected); it completes once the
// interval has passed.
TEST_F(WebInstallServiceImplRateLimitTest,
       CrossOriginQuery_DeferredWithinInterval) {
  test::InstallDummyWebApp(profile(), "Cross Origin App",
                           GURL(kCrossOriginUrl));
  BindService();

  // First query at t=0 runs immediately and is accepted.
  EXPECT_TRUE(IsInstalled(GURL(kCrossOriginUrl)));

  // Back-to-back second query is deferred -- not ready until the interval
  // elapses.
  auto options = blink::mojom::InstallOptions::New();
  options->install_url = GURL(kCrossOriginUrl);
  base::test::TestFuture<bool> deferred;
  service_remote()->IsInstalled(std::move(options), deferred.GetCallback());

  // Drain the mojo pipe so the IsInstalled call reaches the service and
  // posts the delayed task. The deferred reply should still be pending.
  service_remote().FlushForTesting();
  EXPECT_FALSE(deferred.IsReady());

  // Advance well past the minimum interval so the deferred lookup is
  // guaranteed to have run.
  task_environment()->FastForwardBy(kMinCrossOriginQueryInterval * 2);
  ASSERT_TRUE(deferred.IsReady());
  EXPECT_TRUE(deferred.Get());
}

// A cross-origin query issued after an idle period longer than the minimum
// interval dispatches immediately, not after the (now-stale) reserved slot.
TEST_F(WebInstallServiceImplRateLimitTest,
       CrossOriginQuery_AfterExtendedIdle_DispatchesImmediately) {
  test::InstallDummyWebApp(profile(), "Cross Origin App",
                           GURL(kCrossOriginUrl));
  BindService();

  // Prime the dispatch slot with a first query.
  EXPECT_TRUE(IsInstalled(GURL(kCrossOriginUrl)));

  // Idle for much longer than the minimum interval.
  task_environment()->AdvanceClock(kMinCrossOriginQueryInterval * 5);

  // The next query's reserved slot is in the past; it must dispatch now
  // without waiting.
  auto options = blink::mojom::InstallOptions::New();
  options->install_url = GURL(kCrossOriginUrl);
  base::test::TestFuture<bool> future;
  service_remote()->IsInstalled(std::move(options), future.GetCallback());
  service_remote().FlushForTesting();
  EXPECT_TRUE(future.IsReady());
  EXPECT_TRUE(future.Get());
}

// Deferred queries must still consume the per-document count budget. This
// pins the invariant that the count increment happens before scheduling -- if
// it were ever moved after, a compromised renderer could enqueue an
// unbounded number of pending lookups by spamming the API.
TEST_F(WebInstallServiceImplRateLimitTest, DeferredQuery_ConsumesCountBudget) {
  // Use a small limit to avoid looping 100 times.
  base::AutoReset<size_t> max_queries =
      WebInstallServiceImpl::SetMaxCrossOriginQueriesForTesting(kMaxQueries);

  test::InstallDummyWebApp(profile(), "Cross Origin App",
                           GURL(kCrossOriginUrl));
  BindService();

  // Issue kMaxQueries back-to-back. The first runs immediately; the rest are
  // deferred at the minimum interval. Each one consumes one count of budget.
  std::array<base::test::TestFuture<bool>, kMaxQueries> futures;
  for (auto& future : futures) {
    auto options = blink::mojom::InstallOptions::New();
    options->install_url = GURL(kCrossOriginUrl);
    service_remote()->IsInstalled(std::move(options), future.GetCallback());
  }
  // Flush so all IsInstalled calls reach the service and post their tasks.
  service_remote().FlushForTesting();

  // Drain all deferred lookups by advancing well past the total expected
  // span.
  task_environment()->FastForwardBy(kMinCrossOriginQueryInterval *
                                    (kMaxQueries + 1));
  for (auto& future : futures) {
    ASSERT_TRUE(future.IsReady());
    EXPECT_TRUE(future.Get());
  }

  // The budget is now exhausted. The next query must be rejected on count,
  // returning false immediately rather than being deferred.
  EXPECT_FALSE(IsInstalled(GURL(kCrossOriginUrl)));
}

// A burst of cross-origin queries must be paced at the minimum interval.
// The i-th deferred query should complete at t = i * interval, no sooner.
// This guards against a "collapse the queue when the clock is ahead" bug
// where a single FastForward would flush the whole burst at once.
TEST_F(WebInstallServiceImplRateLimitTest,
       BurstedDeferredQueries_PacedAtMinInterval) {
  base::AutoReset<base::TimeDelta> interval =
      WebInstallServiceImpl::SetMinCrossOriginQueryIntervalForTesting(
          base::Seconds(5));

  test::InstallDummyWebApp(profile(), "Cross Origin App",
                           GURL(kCrossOriginUrl));
  BindService();

  // Issue three back-to-back queries at t=0.
  std::array<base::test::TestFuture<bool>, 3> futures;
  for (auto& future : futures) {
    auto options = blink::mojom::InstallOptions::New();
    options->install_url = GURL(kCrossOriginUrl);
    service_remote()->IsInstalled(std::move(options), future.GetCallback());
  }
  service_remote().FlushForTesting();

  // t=0s: only futures[0] is ready (delay=0).
  ASSERT_TRUE(futures[0].IsReady());
  EXPECT_TRUE(futures[0].Get());
  EXPECT_FALSE(futures[1].IsReady());
  EXPECT_FALSE(futures[2].IsReady());

  // t=4s: futures[1] still pending (needs t=5s).
  task_environment()->FastForwardBy(base::Seconds(4));
  EXPECT_FALSE(futures[1].IsReady());
  EXPECT_FALSE(futures[2].IsReady());

  // Past t=5s: futures[1] fires; futures[2] still pending (needs t=10s).
  // Overshoot the boundary to avoid flakiness from exact-interval timing.
  task_environment()->FastForwardBy(base::Seconds(2));
  ASSERT_TRUE(futures[1].IsReady());
  EXPECT_TRUE(futures[1].Get());
  EXPECT_FALSE(futures[2].IsReady());

  // Past t=10s: futures[2] fires.
  task_environment()->FastForwardBy(base::Seconds(5));
  ASSERT_TRUE(futures[2].IsReady());
  EXPECT_TRUE(futures[2].Get());
}

// The cross-origin check follows the registrar lookup origin: when a
// same-origin manifest_id is provided, queries are not rate-limited even if
// the install_url is cross-origin.
TEST_F(WebInstallServiceImplRateLimitTest,
       CrossOriginInstallUrl_SameOriginManifestId_NotRateLimited) {
  // Use a small limit to avoid looping 100 times.
  base::AutoReset<size_t> max_queries =
      WebInstallServiceImpl::SetMaxCrossOriginQueriesForTesting(kMaxQueries);

  // Install an app whose manifest_id matches the document origin.
  test::InstallDummyWebApp(profile(), "Same Origin App", GURL(kDocumentUrl));
  BindService();

  // Issue well past the cross-origin cap. Each query pairs a cross-origin
  // install_url with a same-origin manifest_id, so the lookup is treated as
  // same-origin and none should be rate-limited.
  for (size_t i = 0; i < kMaxQueries * 2; ++i) {
    EXPECT_TRUE(
        IsInstalledWithManifestId(GURL(kCrossOriginUrl), GURL(kDocumentUrl)));
  }
}

// The cross-origin check follows the registrar lookup origin: when a
// cross-origin manifest_id is provided, queries are rate-limited even if the
// install_url is same-origin.
TEST_F(WebInstallServiceImplRateLimitTest,
       SameOriginInstallUrl_CrossOriginManifestId_RateLimited) {
  // Use a small limit to avoid looping 100 times.
  base::AutoReset<size_t> max_queries =
      WebInstallServiceImpl::SetMaxCrossOriginQueriesForTesting(kMaxQueries);

  // Install an app whose manifest_id is cross-origin to the document.
  test::InstallDummyWebApp(profile(), "Cross Origin App",
                           GURL(kCrossOriginUrl));
  BindService();

  // Issue the maximum allowed queries, advancing time after each to avoid
  // the time-based rate limit on the next query.
  for (size_t i = 0; i < kMaxQueries; ++i) {
    EXPECT_TRUE(
        IsInstalledWithManifestId(GURL(kDocumentUrl), GURL(kCrossOriginUrl)));
    task_environment()->AdvanceClock(kMinCrossOriginQueryInterval);
  }

  // The loop's final AdvanceClock cleared the time throttle, so this next
  // query is rejected purely on count.
  EXPECT_FALSE(
      IsInstalledWithManifestId(GURL(kDocumentUrl), GURL(kCrossOriginUrl)));
}

// A non-HTTP(S) manifest_id (e.g. chrome://) should return false immediately
// without consuming cross-origin query budget.
TEST_F(WebInstallServiceImplRateLimitTest,
       InvalidManifestIdScheme_ReturnsFalse) {
  BindService();

  EXPECT_FALSE(IsInstalledWithManifestId(GURL(kCrossOriginUrl),
                                         GURL("chrome://settings")));
}

}  // namespace
}  // namespace web_app
