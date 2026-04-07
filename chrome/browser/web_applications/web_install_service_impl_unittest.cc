// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_install_service_impl.h"

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
  // Create a manifest WITHOUT setting `id` — the fake will default it to
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

}  // namespace
}  // namespace web_app
