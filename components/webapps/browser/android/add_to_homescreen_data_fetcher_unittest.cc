// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/android/add_to_homescreen_data_fetcher.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/android/device_info.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/process/kill.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/favicon/content/large_icon_service_getter.h"
#include "components/favicon/core/large_icon_service.h"
#include "components/favicon_base/favicon_types.h"
#include "components/webapps/browser/features.h"
#include "components/webapps/browser/installable/installable_data.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "components/webapps/browser/installable/installable_manager.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/common/web_page_metadata.mojom.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"
#include "third_party/blink/public/mojom/favicon/favicon_url.mojom.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "url/gurl.h"

namespace webapps {

namespace {

const std::u16string kWebAppInstallInfoTitle = u"Meta Title";
const std::u16string kDefaultManifestName = u"Default Name";
const std::u16string kDefaultManifestShortName = u"Default Short Name";
const char* kDefaultManifestUrl = "https://www.example.com/manifest.json";
const char* kDefaultIconUrl = "https://www.example.com/icon.png";
const char* kDefaultStartUrl = "https://www.example.com/index.html";
const blink::mojom::DisplayMode kDefaultManifestDisplayMode =
    blink::mojom::DisplayMode::kStandalone;
const int kIconSizePx = 144;
const char kDecisionHistogram[] = "Webapp.AddToHomescreen.AnyPage.Decision";
const char kIconSourceHistogram[] =
    "Webapp.AddToHomescreen.AnyPage.PrimaryIconSource";
using AnyPageDecision = AddToHomescreenDataFetcher::AnyPageDecision;
using AnyPagePrimaryIconSource =
    AddToHomescreenDataFetcher::AnyPagePrimaryIconSource;

// Tracks which of the AddToHomescreenDataFetcher::Observer methods have been
// called.
class ObserverWaiter : public AddToHomescreenDataFetcher::Observer {
 public:
  ObserverWaiter() = default;

  ObserverWaiter(const ObserverWaiter&) = delete;
  ObserverWaiter& operator=(const ObserverWaiter&) = delete;

  ~ObserverWaiter() override = default;

  // Waits till the OnDataAvailable() callback is called.
  void WaitForDataAvailable() {
    if (data_available_)
      return;

    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void OnUserTitleAvailable(const std::u16string& title,
                            const GURL& url,
                            AddToHomescreenParams::AppType app_type) override {
    // This should only be called once.
    EXPECT_FALSE(title_available_);
    EXPECT_FALSE(data_available_);
    title_available_ = true;
    title_ = title;
    app_type_ = app_type;
  }

  void OnDataAvailable(
      const ShortcutInfo& info,
      const SkBitmap& primary_icon,
      AddToHomescreenParams::AppType app_type,
      const InstallableStatusCode installable_status) override {
    // This should only be called once.
    EXPECT_FALSE(data_available_);
    EXPECT_TRUE(title_available_);
    data_available_ = true;
    installable_status_ = installable_status;
    app_type_ = app_type;
    if (quit_closure_)
      quit_closure_.Run();
  }

  std::u16string title() const { return title_; }
  bool title_available() const { return title_available_; }
  bool data_available() const { return data_available_; }
  AddToHomescreenParams::AppType app_type() const { return app_type_; }
  InstallableStatusCode installable_status() const {
    return installable_status_;
  }

 private:
  std::u16string title_;
  bool title_available_ = false;
  bool data_available_ = false;
  AddToHomescreenParams::AppType app_type_;
  InstallableStatusCode installable_status_;
  base::RepeatingClosure quit_closure_;
};

// Owns the fetcher and deletes it from inside OnDataAvailable(), as the
// universal install sheet's AppDataFetcher does.
class FetcherDeletingObserver : public AddToHomescreenDataFetcher::Observer {
 public:
  FetcherDeletingObserver() = default;
  ~FetcherDeletingObserver() override = default;

  void set_fetcher(std::unique_ptr<AddToHomescreenDataFetcher> fetcher) {
    fetcher_ = std::move(fetcher);
  }

  void WaitForDataAvailable() { run_loop_.Run(); }

  void OnUserTitleAvailable(const std::u16string& title,
                            const GURL& url,
                            AddToHomescreenParams::AppType app_type) override {}

  void OnDataAvailable(const ShortcutInfo& info,
                       const SkBitmap& primary_icon,
                       AddToHomescreenParams::AppType app_type,
                       InstallableStatusCode installable_status) override {
    EXPECT_FALSE(data_available_);
    data_available_ = true;
    app_type_ = app_type;
    fetcher_.reset();
    run_loop_.Quit();
  }

  bool data_available() const { return data_available_; }
  AddToHomescreenParams::AppType app_type() const { return app_type_; }

 private:
  std::unique_ptr<AddToHomescreenDataFetcher> fetcher_;
  base::RunLoop run_loop_;
  bool data_available_ = false;
  AddToHomescreenParams::AppType app_type_ =
      AddToHomescreenParams::AppType::SHORTCUT;
};

// Builds blink::WebPageMetadata.
mojom::WebPageMetadataPtr BuildDefaultMetadata() {
  auto metadata = mojom::WebPageMetadata::New();
  metadata->application_name = kWebAppInstallInfoTitle;
  return metadata;
}

// Builds WebAPK compatible blink::Manifest.
blink::mojom::ManifestPtr BuildWebAPKManifest() {
  GURL start_url = GURL(kDefaultStartUrl);
  auto manifest = blink::mojom::Manifest::New();
  manifest->name = kDefaultManifestName;
  manifest->short_name = kDefaultManifestShortName;
  manifest->start_url = start_url;
  manifest->scope = start_url.GetWithoutFilename();
  manifest->has_valid_specified_start_url = true;
  manifest->id = start_url.GetWithoutRef();
  manifest->display = kDefaultManifestDisplayMode;

  blink::Manifest::ImageResource primary_icon;
  primary_icon.type = u"image/png";
  primary_icon.sizes.push_back(gfx::Size(144, 144));
  primary_icon.purpose.push_back(
      blink::mojom::ManifestImageResource_Purpose::ANY);
  primary_icon.src = GURL(kDefaultIconUrl);
  manifest->icons.push_back(primary_icon);

  return manifest;
}

}  // anonymous namespace

class TestInstallableManager : public InstallableManager {
 public:
  explicit TestInstallableManager(content::WebContents* web_contents)
      : InstallableManager(web_contents) {}

  // Mock out the GetData API so we can control exactly what is returned to the
  // data fetcher. The order of errors matches | InstallableManager::GetErrors|.
  void GetData(const InstallableParams& params,
               InstallableCallback callback) override {
    get_data_call_count_++;
    if (should_manifest_time_out_) {
      return;
    }

    if (should_hold_requests_) {
      // Queue a real task without starting it, so that the request stays
      // pending until InstallableManager resets (navigation, manifest URL
      // change, WebContents destruction) and delivers the reset code to it.
      task_queue_.Add(std::make_unique<InstallableTask>(
          web_contents(), weak_factory_.GetWeakPtr(), params,
          std::move(callback), *page_data_));
      return;
    }

    InitPageData();

    InstallableParams test_params = params;
    if (!should_check_eligibility_) {
      // Do not check if in secure content in unittest.
      test_params.check_eligibility = false;
    }

    InstallableManager::GetData(test_params, std::move(callback));
  }

  int get_data_call_count() const { return get_data_call_count_; }

  void SetWebPageMetadata(mojom::WebPageMetadataPtr metadata) {
    page_data_->OnPageMetadataFetched(std::move(metadata));
  }

  // Builds and sets the default manifest for the given document url.
  void SetManifestAsDefault(const GURL& document_url) {
    auto manifest = blink::mojom::Manifest::New();
    manifest->start_url = document_url;
    manifest->scope = document_url.GetWithoutFilename();
    manifest->id = document_url.GetWithoutRef();
    page_data_->OnManifestFetched(std::move(manifest), /*manifest_url=*/GURL());
  }

  void SetManifestWithError(blink::mojom::ManifestPtr manifest,
                            const GURL& manifest_url,
                            InstallableStatusCode error) {
    if (!manifest->icons.empty()) {
      SetPrimaryIcon(manifest->icons[0].src);
    }
    page_data_->OnManifestFetched(std::move(manifest), manifest_url, error);
  }

  void SetManifest(blink::mojom::ManifestPtr manifest) {
    if (!manifest->icons.empty()) {
      SetPrimaryIcon(manifest->icons[0].src);
    }

    page_data_->OnManifestFetched(std::move(manifest),
                                  GURL(kDefaultManifestUrl));
  }

  // Like SetManifest(), but does not fake a successful fetch of the manifest's
  // first icon, so the icon round can be made to fail (InitPageData defaults
  // it to NO_ACCEPTABLE_ICON) while the manifest itself stays promotable.
  void SetManifestWithoutIcon(blink::mojom::ManifestPtr manifest) {
    page_data_->OnManifestFetched(std::move(manifest),
                                  GURL(kDefaultManifestUrl));
  }

  void SetPrimaryIcon(const GURL& icon_url) {
    page_data_->OnPrimaryIconFetched(
        icon_url, blink::mojom::ManifestImageResource_Purpose::ANY,
        gfx::test::CreateBitmap(kIconSizePx, kIconSizePx));
  }

  void SetPrimaryIconError(InstallableStatusCode error) {
    page_data_->OnPrimaryIconFetchedError(error);
  }

  void SetShouldManifestTimeOut(bool should_time_out) {
    should_manifest_time_out_ = should_time_out;
  }

  // Queue GetData requests without running them (see GetData()).
  void SetShouldHoldRequests(bool should_hold) {
    should_hold_requests_ = should_hold;
  }

  // Keep `check_eligibility` on, so IN_INCOGNITO / NOT_FROM_SECURE_ORIGIN are
  // evaluated. Note that without a WebappsClient, IsContentSecure() is false
  // for every non-localhost origin, so only use this for pages expected to be
  // ineligible.
  void SetShouldCheckEligibility(bool should_check) {
    should_check_eligibility_ = should_check;
  }

  // Synchronously resets the manager with MANIFEST_URL_CHANGED, as a
  // <link rel=manifest> change in the page would.
  void TriggerManifestUrlChanged(const GURL& manifest_url) {
    DidUpdateWebManifestURL(web_contents()->GetPrimaryMainFrame(),
                            manifest_url);
  }

  // Clears the cached page data so a test can run several fetches with
  // different manifests (InstallablePageData CHECKs against double fetches).
  void ResetPageData() { page_data_->Reset(); }

 private:
  int get_data_call_count_ = 0;
  void InitPageData() {
    // Initialize all default values and set "fetched" to be true so the
    // installable fetcher won't try to fetch the real data.
    if (!page_data_->manifest_fetched()) {
      page_data_->OnManifestFetched(blink::mojom::Manifest::New(), GURL(),
                                    InstallableStatusCode::NO_MANIFEST);
    }
    if (!page_data_->web_page_metadata_fetched()) {
      page_data_->OnPageMetadataFetched(BuildDefaultMetadata());
    }
    if (!page_data_->primary_icon_fetched()) {
      page_data_->OnPrimaryIconFetchedError(
          InstallableStatusCode::NO_ACCEPTABLE_ICON);
    }
    if (!page_data_->is_screenshots_fetch_complete()) {
      page_data_->OnScreenshotsDownloaded(std::vector<Screenshot>());
    }
  }

  bool should_manifest_time_out_ = false;
  bool should_hold_requests_ = false;
  bool should_check_eligibility_ = false;
};

// Tests AddToHomescreenDataFetcher. These tests should be browser tests but
// Android does not support browser tests yet (crbug.com/611756).
class AddToHomescreenDataFetcherTest
    : public content::RenderViewHostTestHarness {
 public:
  AddToHomescreenDataFetcherTest() = default;

  AddToHomescreenDataFetcherTest(const AddToHomescreenDataFetcherTest&) =
      delete;
  AddToHomescreenDataFetcherTest& operator=(
      const AddToHomescreenDataFetcherTest&) = delete;

  ~AddToHomescreenDataFetcherTest() override = default;

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();

    // Manually inject the TestInstallableManager as a "InstallableManager"
    // WebContentsUserData. We can't directly call ::CreateForWebContents due to
    // typing issues since TestInstallableManager doesn't directly inherit from
    // WebContentsUserData.
    web_contents()->SetUserData(
        TestInstallableManager::UserDataKey(),
        base::WrapUnique(new TestInstallableManager(web_contents())));
    installable_manager_ = static_cast<TestInstallableManager*>(
        web_contents()->GetUserData(TestInstallableManager::UserDataKey()));

    favicon::SetLargeIconServiceGetter(base::BindRepeating(
        [](favicon::LargeIconService* service,
           content::BrowserContext* context) { return service; },
        &null_large_icon_service_));

    NavigateAndCommit(GURL(kDefaultStartUrl));
  }

 protected:
  content::WebContentsTester* web_contents_tester() {
    return content::WebContentsTester::For(web_contents());
  }

  std::unique_ptr<AddToHomescreenDataFetcher> BuildFetcher(
      AddToHomescreenDataFetcher::Observer* observer) {
    return std::make_unique<AddToHomescreenDataFetcher>(web_contents(), 500,
                                                        observer);
  }

  void RunFetcher(AddToHomescreenDataFetcher* fetcher,
                  ObserverWaiter& waiter,
                  const std::u16string& expected_user_title,
                  const std::u16string& expected_name,
                  blink::mojom::DisplayMode display_mode,
                  AddToHomescreenParams::AppType expected_app_type,
                  InstallableStatusCode status_code) {
    waiter.WaitForDataAvailable();

    EXPECT_TRUE(waiter.title_available());
    EXPECT_EQ(waiter.app_type(), expected_app_type);

    if (expected_app_type == AddToHomescreenParams::AppType::WEBAPK) {
      EXPECT_EQ(waiter.title(), expected_name);
    } else {
      EXPECT_EQ(waiter.title(), expected_user_title);
    }

    EXPECT_EQ(fetcher->shortcut_info().user_title, expected_user_title);
    EXPECT_EQ(display_mode, fetcher->shortcut_info().display);
    EXPECT_EQ(status_code, waiter.installable_status());
  }

  void RunFetcher(AddToHomescreenDataFetcher* fetcher,
                  ObserverWaiter& waiter,
                  const std::u16string& expected_title,
                  blink::mojom::DisplayMode display_mode,
                  AddToHomescreenParams::AppType expected_app_type,
                  InstallableStatusCode status_code) {
    RunFetcher(fetcher, waiter, expected_title, expected_title, display_mode,
               expected_app_type, status_code);
  }

  void CheckHistograms(base::HistogramTester& histograms) {
    histograms.ExpectTotalCount("Webapp.AddToHomescreenDialog.Timeout", 1);
  }

  void SetManifest(blink::mojom::ManifestPtr manifest) {
    installable_manager_->SetManifest(std::move(manifest));
  }

  void SetManifestWithError(blink::mojom::ManifestPtr manifest,
                            const GURL& manifest_url,
                            InstallableStatusCode error) {
    installable_manager_->SetManifestWithError(std::move(manifest),
                                               manifest_url, error);
  }

  void SetManifestWithoutIcon(blink::mojom::ManifestPtr manifest) {
    installable_manager_->SetManifestWithoutIcon(std::move(manifest));
  }

  void SetManifestAsDefault(const GURL& document_url) {
    installable_manager_->SetManifestAsDefault(document_url);
  }

  void SetWebPageMetadata(mojom::WebPageMetadataPtr metadata) {
    installable_manager_->SetWebPageMetadata(std::move(metadata));
  }

  void SetPrimaryIcon(const GURL& icon_url) {
    installable_manager_->SetPrimaryIcon(icon_url);
  }

  void SetPrimaryIconError(InstallableStatusCode error) {
    installable_manager_->SetPrimaryIconError(error);
  }

  void SetShouldManifestTimeOut(bool should_time_out) {
    installable_manager_->SetShouldManifestTimeOut(should_time_out);
  }

  void SetShouldHoldRequests(bool should_hold) {
    installable_manager_->SetShouldHoldRequests(should_hold);
  }

  void SetShouldCheckEligibility(bool should_check) {
    installable_manager_->SetShouldCheckEligibility(should_check);
  }

  void TriggerManifestUrlChanged(const GURL& manifest_url) {
    installable_manager_->TriggerManifestUrlChanged(manifest_url);
  }

  // InstallablePageData CHECKs that each field is fetched at most once, so
  // tests that run several fetches against one WebContents must reset the
  // cached data in between.
  void ResetPageData() { installable_manager_->ResetPageData(); }

  // Runs the UI sequence until a task posted now has run, i.e. until every
  // task the preceding synchronous step may have posted has been processed.
  // Used by the tests that assert nothing reaches the observer.
  void RunPostedTasks() {
    base::RunLoop run_loop;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  // Deletes the WebContents (and with it the TestInstallableManager).
  void DestroyWebContents() {
    installable_manager_ = nullptr;
    DeleteContents();
  }

  TestInstallableManager* test_installable_manager() {
    return installable_manager_;
  }

 private:
  class NullLargeIconService : public favicon::LargeIconService {
   public:
    NullLargeIconService() = default;
    ~NullLargeIconService() override = default;

    MOCK_METHOD(base::CancelableTaskTracker::TaskId,
                GetLargeIconRawBitmapOrFallbackStyleForPageUrl,
                (const GURL& page_url,
                 int min_source_size_in_pixel,
                 int desired_size_in_pixel,
                 favicon_base::LargeIconCallback callback,
                 base::CancelableTaskTracker* tracker),
                (override));
    MOCK_METHOD(base::CancelableTaskTracker::TaskId,
                GetLargeIconImageOrFallbackStyleForPageUrl,
                (const GURL& page_url,
                 int min_source_size_in_pixel,
                 int desired_size_in_pixel,
                 favicon_base::LargeIconImageCallback callback,
                 base::CancelableTaskTracker* tracker),
                (override));
    MOCK_METHOD(base::CancelableTaskTracker::TaskId,
                GetLargeIconRawBitmapOrFallbackStyleForIconUrl,
                (const GURL& icon_url,
                 int min_source_size_in_pixel,
                 int desired_size_in_pixel,
                 favicon_base::LargeIconCallback callback,
                 base::CancelableTaskTracker* tracker),
                (override));
    MOCK_METHOD(base::CancelableTaskTracker::TaskId,
                GetIconRawBitmapOrFallbackStyleForPageUrl,
                (const GURL& page_url,
                 int desired_size_in_pixel,
                 favicon_base::LargeIconCallback callback,
                 base::CancelableTaskTracker* tracker),
                (override));
    MOCK_METHOD(void,
                GetLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache,
                (const GURL& page_url,
                 bool should_trim_page_url_path,
                 const net::NetworkTrafficAnnotationTag& traffic_annotation,
                 favicon_base::GoogleFaviconServerCallback callback),
                (override));
    MOCK_METHOD(void,
                GetLargeIconFromCacheFallbackToGoogleServer,
                (const GURL& page_url,
                 StandardIconSize min_source_size_in_pixel,
                 std::optional<StandardIconSize> size_in_pixel_to_resize_to,
                 NoBigEnoughIconBehavior no_big_enough_icon_behavior,
                 bool should_trim_page_url_path,
                 const net::NetworkTrafficAnnotationTag& traffic_annotation,
                 favicon_base::LargeIconCallback callback,
                 base::CancelableTaskTracker* tracker),
                (override));
    MOCK_METHOD(void,
                TouchIconFromGoogleServer,
                (const GURL& icon_url),
                (override));
    base::CancelableTaskTracker::TaskId GetLargeIconRawBitmapForPageUrl(
        const GURL& page_url,
        int min_source_size_in_pixel,
        std::optional<int> size_in_pixel_to_resize_to,
        NoBigEnoughIconBehavior no_big_enough_icon_behavior,
        favicon_base::LargeIconCallback callback,
        base::CancelableTaskTracker* tracker) override {
      content::GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(callback),
                         favicon_base::LargeIconResult(
                             favicon_base::FaviconRawBitmapResult())));
      return base::CancelableTaskTracker::kBadTaskId;
    }
  };

  raw_ptr<TestInstallableManager> installable_manager_;
  NullLargeIconService null_large_icon_service_;
};

TEST_F(AddToHomescreenDataFetcherTest, NoManifest) {
  // Check that an empty manifest has the appropriate methods run.
  base::HistogramTester histograms;
  ObserverWaiter waiter;
  std::unique_ptr<AddToHomescreenDataFetcher> fetcher = BuildFetcher(&waiter);
  RunFetcher(fetcher.get(), waiter, kWebAppInstallInfoTitle,
             blink::mojom::DisplayMode::kBrowser,
             AddToHomescreenParams::AppType::SHORTCUT,
             InstallableStatusCode::NO_MANIFEST);
  CheckHistograms(histograms);
  // The harness leaves the manifest empty, which is the opaque-origin case.
  histograms.ExpectUniqueSample(kDecisionHistogram,
                                AnyPageDecision::kShortcutIneligible, 1);
}

TEST_F(AddToHomescreenDataFetcherTest, NoIconManifest) {
  // Legacy (flag-off) behavior for a manifest with no icons: falls back to
  // SHORTCUT with a generated icon (empty icon url). The flag-on behavior is
  // covered by AnyPage_ManifestMissingSuitableIcon.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kAndroidInstallAnyPageAsDiyAppStopgap);

  blink::mojom::ManifestPtr manifest = BuildWebAPKManifest();
  manifest->icons.clear();
  SetManifest(std::move(manifest));

  base::HistogramTester histograms;
  ObserverWaiter waiter;
  std::unique_ptr<AddToHomescreenDataFetcher> fetcher = BuildFetcher(&waiter);
  RunFetcher(fetcher.get(), waiter, kDefaultManifestShortName,
             blink::mojom::DisplayMode::kStandalone,
             AddToHomescreenParams::AppType::SHORTCUT,
             InstallableStatusCode::NO_ACCEPTABLE_ICON);
  CheckHistograms(histograms);

  EXPECT_TRUE(fetcher->shortcut_info().best_primary_icon_url.is_empty());
  EXPECT_TRUE(fetcher->shortcut_info().splash_image_url.is_empty());
}

// Check that the AddToHomescreenDataFetcher::Observer methods are called
// if the first call to InstallableManager::GetData() times out. This should
// fall back to the metadata title and have a non-empty icon (taken from the
// favicon).
TEST_F(AddToHomescreenDataFetcherTest, ManifestFetchTimesOutPwa) {
  SetShouldManifestTimeOut(true);
  SetManifest(BuildWebAPKManifest());

  // Check a site where InstallableManager finishes working after the time out
  // and determines PWA-ness. This is only relevant when checking WebAPK
  // compatibility.
  base::HistogramTester histograms;
  ObserverWaiter waiter;
  std::unique_ptr<AddToHomescreenDataFetcher> fetcher = BuildFetcher(&waiter);
  RunFetcher(fetcher.get(), waiter, web_contents()->GetTitle(),
             blink::mojom::DisplayMode::kBrowser,
             AddToHomescreenParams::AppType::SHORTCUT,
             InstallableStatusCode::DATA_TIMED_OUT);
  CheckHistograms(histograms);

  EXPECT_FALSE(fetcher->primary_icon().drawsNothing());
  EXPECT_TRUE(fetcher->shortcut_info().best_primary_icon_url.is_empty());
}

TEST_F(AddToHomescreenDataFetcherTest, ManifestFetchTimesOutNonPwa) {
  SetShouldManifestTimeOut(true);
  SetManifest(BuildWebAPKManifest());

  // Check where InstallableManager finishes working after the time out and
  // determines non-PWA-ness.
  base::HistogramTester histograms;
  ObserverWaiter waiter;
  std::unique_ptr<AddToHomescreenDataFetcher> fetcher = BuildFetcher(&waiter);
  RunFetcher(fetcher.get(), waiter, web_contents()->GetTitle(),
             blink::mojom::DisplayMode::kBrowser,
             AddToHomescreenParams::AppType::SHORTCUT,
             InstallableStatusCode::DATA_TIMED_OUT);
  CheckHistograms(histograms);

  EXPECT_FALSE(fetcher->primary_icon().drawsNothing());
  EXPECT_TRUE(fetcher->shortcut_info().best_primary_icon_url.is_empty());
}

TEST_F(AddToHomescreenDataFetcherTest, ManifestFetchTimesOutUnknown) {
  SetShouldManifestTimeOut(true);
  SetManifest(BuildWebAPKManifest());

  // Check where InstallableManager doesn't finish working after the time out.
  base::HistogramTester histograms;
  ObserverWaiter waiter;
  std::unique_ptr<AddToHomescreenDataFetcher> fetcher = BuildFetcher(&waiter);
  RunFetcher(fetcher.get(), waiter, web_contents()->GetTitle(),
             blink::mojom::DisplayMode::kBrowser,
             AddToHomescreenParams::AppType::SHORTCUT,
             InstallableStatusCode::DATA_TIMED_OUT);
  NavigateAndCommit(GURL("about:blank"));
  CheckHistograms(histograms);
  histograms.ExpectUniqueSample(kDecisionHistogram,
                                AnyPageDecision::kShortcutTimeout, 1);

  EXPECT_FALSE(fetcher->primary_icon().drawsNothing());
  EXPECT_TRUE(fetcher->shortcut_info().best_primary_icon_url.is_empty());
}

TEST_F(AddToHomescreenDataFetcherTest, InstallableManifest) {
  // Test a site that has valid manifest.
  SetManifest(BuildWebAPKManifest());

  base::HistogramTester histograms;
  ObserverWaiter waiter;
  std::unique_ptr<AddToHomescreenDataFetcher> fetcher = BuildFetcher(&waiter);
  RunFetcher(fetcher.get(), waiter, kDefaultManifestShortName,
             kDefaultManifestName, blink::mojom::DisplayMode::kStandalone,
             AddToHomescreenParams::AppType::WEBAPK,
             InstallableStatusCode::NO_ERROR_DETECTED);

  // There should always be a primary icon.
  EXPECT_FALSE(fetcher->primary_icon().drawsNothing());
  EXPECT_EQ(fetcher->shortcut_info().best_primary_icon_url,
            GURL(kDefaultIconUrl));

  // Check that splash icon url has been selected.
  EXPECT_EQ(fetcher->shortcut_info().splash_image_url, GURL(kDefaultIconUrl));
  CheckHistograms(histograms);
}

TEST_F(AddToHomescreenDataFetcherTest, ManifestNoNameNoShortName) {
  // Test that when the manifest does not provide either Manifest::short_name
  // nor Manifest::name but web page metadata provides a application-name.
  blink::mojom::ManifestPtr manifest = BuildWebAPKManifest();
  manifest->name = std::nullopt;
  manifest->short_name = std::nullopt;
  SetManifest(std::move(manifest));
  mojom::WebPageMetadataPtr metadata = BuildDefaultMetadata();
  SetWebPageMetadata(std::move(metadata));

  ObserverWaiter waiter;
  std::unique_ptr<AddToHomescreenDataFetcher> fetcher = BuildFetcher(&waiter);
  RunFetcher(fetcher.get(), waiter, kWebAppInstallInfoTitle,
             blink::mojom::DisplayMode::kStandalone,
             AddToHomescreenParams::AppType::WEBAPK,
             InstallableStatusCode::NO_ERROR_DETECTED);

  EXPECT_EQ(fetcher->shortcut_info().name, kWebAppInstallInfoTitle);
  EXPECT_EQ(fetcher->shortcut_info().short_name, kWebAppInstallInfoTitle);
  EXPECT_FALSE(fetcher->primary_icon().drawsNothing());
  EXPECT_EQ(fetcher->shortcut_info().best_primary_icon_url,
            GURL(kDefaultIconUrl));
}

TEST_F(AddToHomescreenDataFetcherTest, NoManifestIcons) {
  // Test that when the manifest does not provide any icon, we fallback to use
  // favicon.
  blink::mojom::ManifestPtr manifest = BuildWebAPKManifest();
  manifest->icons.clear();
  SetManifest(std::move(manifest));

  std::vector<blink::mojom::FaviconURLPtr> favicon_urls;
  favicon_urls.push_back(blink::mojom::FaviconURL::New(
      GURL{"http://www.google.com/favicon.ico"},
      blink::mojom::FaviconIconType::kFavicon, std::vector<gfx::Size>(),
      /*is_default_icon=*/false));
  web_contents_tester()->TestSetFaviconURL(mojo::Clone(favicon_urls));

  // Fake that |InstallableIconFetcher| fetched the icon correctly.
  SetPrimaryIcon(GURL(kDefaultIconUrl));

  ObserverWaiter waiter;
  std::unique_ptr<AddToHomescreenDataFetcher> fetcher = BuildFetcher(&waiter);
  RunFetcher(fetcher.get(), waiter, kDefaultManifestShortName,
             kDefaultManifestName, blink::mojom::DisplayMode::kStandalone,
             AddToHomescreenParams::AppType::WEBAPK,
             InstallableStatusCode::NO_ERROR_DETECTED);

  EXPECT_EQ(fetcher->shortcut_info().name, kDefaultManifestName);
  EXPECT_EQ(fetcher->shortcut_info().short_name, kDefaultManifestShortName);
  EXPECT_FALSE(fetcher->primary_icon().drawsNothing());
  EXPECT_EQ(fetcher->shortcut_info().best_primary_icon_url,
            GURL(kDefaultIconUrl));
}

TEST_F(AddToHomescreenDataFetcherTest, ManifestDisplayMode) {
  // Test that when the manifest does not provide display mode, we fallback to
  // install with DisplayMode::kMinimalUi.
  blink::mojom::ManifestPtr manifest = BuildWebAPKManifest();
  manifest->display = blink::mojom::DisplayMode::kUndefined;
  SetManifest(std::move(manifest));
  mojom::WebPageMetadataPtr metadata = BuildDefaultMetadata();
  SetWebPageMetadata(std::move(metadata));

  ObserverWaiter waiter;
  std::unique_ptr<AddToHomescreenDataFetcher> fetcher = BuildFetcher(&waiter);
  RunFetcher(fetcher.get(), waiter, kDefaultManifestShortName,
             kDefaultManifestName, blink::mojom::DisplayMode::kMinimalUi,
             AddToHomescreenParams::AppType::WEBAPK,
             InstallableStatusCode::NO_ERROR_DETECTED);

  EXPECT_EQ(fetcher->shortcut_info().name, kDefaultManifestName);
  EXPECT_EQ(fetcher->shortcut_info().short_name, kDefaultManifestShortName);
  EXPECT_FALSE(fetcher->primary_icon().drawsNothing());
  EXPECT_EQ(fetcher->shortcut_info().best_primary_icon_url,
            GURL(kDefaultIconUrl));
}

TEST_F(AddToHomescreenDataFetcherTest,
       UniversalInstallEmptyManifestAtRootScope) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kAndroidInstallAnyPageAsDiyAppStopgap);

  GURL document_url = GURL("https://www.example.com/index.html");
  NavigateAndCommit(document_url);

  SetManifestAsDefault(document_url);
  SetWebPageMetadata(BuildDefaultMetadata());
  std::vector<blink::mojom::FaviconURLPtr> favicon_urls;
  favicon_urls.push_back(blink::mojom::FaviconURL::New(
      GURL{"http://www.google.com/favicon.ico"},
      blink::mojom::FaviconIconType::kFavicon, std::vector<gfx::Size>(),
      /*is_default_icon=*/false));
  web_contents_tester()->TestSetFaviconURL(mojo::Clone(favicon_urls));
  // Fake that |InstallableIconFetcher| fetched the icon correctly.
  SetPrimaryIcon(GURL(kDefaultIconUrl));

  base::HistogramTester histograms;
  ObserverWaiter waiter;
  std::unique_ptr<AddToHomescreenDataFetcher> fetcher = BuildFetcher(&waiter);
  RunFetcher(fetcher.get(), waiter, kWebAppInstallInfoTitle,
             blink::mojom::DisplayMode::kMinimalUi,
             AddToHomescreenParams::AppType::WEBAPK_DIY,
             InstallableStatusCode::NO_ERROR_DETECTED);

  EXPECT_EQ(fetcher->shortcut_info().name, kWebAppInstallInfoTitle);
  EXPECT_EQ(fetcher->shortcut_info().short_name, kWebAppInstallInfoTitle);
  EXPECT_FALSE(fetcher->primary_icon().drawsNothing());
  EXPECT_EQ(fetcher->shortcut_info().best_primary_icon_url,
            GURL(kDefaultIconUrl));
  // The flag-off arm records DIY for a root-scope no-manifest page too.
  histograms.ExpectUniqueSample(kDecisionHistogram,
                                AnyPageDecision::kDiyNoManifest, 1);
  histograms.ExpectUniqueSample(kIconSourceHistogram,
                                AnyPagePrimaryIconSource::kDownloaded, 1);
}

TEST_F(AddToHomescreenDataFetcherTest,
       UniversalInstallEmptyManifestNotRootScope) {
  GURL document_url = GURL("https://www.example.com/scope/index.html");
  NavigateAndCommit(document_url);

  SetManifestAsDefault(document_url);
  SetWebPageMetadata(BuildDefaultMetadata());
  std::vector<blink::mojom::FaviconURLPtr> favicon_urls;
  favicon_urls.push_back(blink::mojom::FaviconURL::New(
      GURL{"http://www.google.com/favicon.ico"},
      blink::mojom::FaviconIconType::kFavicon, std::vector<gfx::Size>(),
      /*is_default_icon=*/false));
  web_contents_tester()->TestSetFaviconURL(mojo::Clone(favicon_urls));
  // Fake that |InstallableIconFetcher| fetched the icon correctly.
  SetPrimaryIcon(GURL(kDefaultIconUrl));

  ObserverWaiter waiter;
  std::unique_ptr<AddToHomescreenDataFetcher> fetcher = BuildFetcher(&waiter);
  if (AddToHomescreenDataFetcher::IsStopgapEnabled()) {
    // When stopgap is enabled, any eligible page (including sub-path) becomes
    // WEBAPK_DIY.
    RunFetcher(fetcher.get(), waiter, kWebAppInstallInfoTitle,
               base::android::device_info::is_desktop()
                   ? blink::mojom::DisplayMode::kMinimalUi
                   : blink::mojom::DisplayMode::kStandalone,
               AddToHomescreenParams::AppType::WEBAPK_DIY,
               InstallableStatusCode::NO_MANIFEST);
  } else if (base::android::device_info::is_desktop()) {
    // Desktop Android expects a standalone DIY WebAPK.
    RunFetcher(fetcher.get(), waiter, kWebAppInstallInfoTitle,
               blink::mojom::DisplayMode::kStandalone,
               AddToHomescreenParams::AppType::WEBAPK_DIY,
               InstallableStatusCode::NO_MANIFEST);
  } else {
    // Regular Android expects a shortcut.
    RunFetcher(fetcher.get(), waiter, kWebAppInstallInfoTitle,
               blink::mojom::DisplayMode::kBrowser,
               AddToHomescreenParams::AppType::SHORTCUT,
               InstallableStatusCode::NO_MANIFEST);
  }

  EXPECT_EQ(fetcher->shortcut_info().name, kWebAppInstallInfoTitle);
  EXPECT_EQ(fetcher->shortcut_info().short_name, kWebAppInstallInfoTitle);
  EXPECT_FALSE(fetcher->primary_icon().drawsNothing());
  EXPECT_EQ(fetcher->shortcut_info().best_primary_icon_url,
            GURL(kDefaultIconUrl));
}

TEST_F(AddToHomescreenDataFetcherTest, PendingNavigation) {
  GURL committed_url("https://www.attacker.com/");
  NavigateAndCommit(committed_url);

  GURL pending_url("https://www.victim.com/");
  std::unique_ptr<content::NavigationSimulator> navigation =
      content::NavigationSimulator::CreateBrowserInitiated(pending_url,
                                                           web_contents());
  navigation->Start();

  EXPECT_EQ(web_contents()->GetVisibleURL(), pending_url);
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), committed_url);

  ObserverWaiter waiter;
  std::unique_ptr<AddToHomescreenDataFetcher> fetcher = BuildFetcher(&waiter);

  EXPECT_EQ(fetcher->shortcut_info().url, committed_url);
}

TEST_F(AddToHomescreenDataFetcherTest, AnyPage_NoManifestSubPath) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAndroidInstallAnyPageAsDiyAppStopgap);

  GURL document_url("https://www.example.com/subpath/page.html");
  NavigateAndCommit(document_url);

  SetManifestAsDefault(document_url);
  SetWebPageMetadata(BuildDefaultMetadata());
  SetPrimaryIcon(GURL(kDefaultIconUrl));

  base::HistogramTester histograms;
  ObserverWaiter waiter;
  std::unique_ptr<AddToHomescreenDataFetcher> fetcher = BuildFetcher(&waiter);
  RunFetcher(fetcher.get(), waiter, kWebAppInstallInfoTitle,
             base::android::device_info::is_desktop()
                 ? blink::mojom::DisplayMode::kMinimalUi
                 : blink::mojom::DisplayMode::kStandalone,
             AddToHomescreenParams::AppType::WEBAPK_DIY,
             InstallableStatusCode::NO_MANIFEST);

  EXPECT_EQ(fetcher->shortcut_info().scope,
            GURL("https://www.example.com/subpath/"));
  EXPECT_FALSE(fetcher->primary_icon().drawsNothing());
  EXPECT_EQ(fetcher->shortcut_info().best_primary_icon_url,
            GURL(kDefaultIconUrl));
  histograms.ExpectUniqueSample(kDecisionHistogram,
                                AnyPageDecision::kDiyNoManifest, 1);
  histograms.ExpectUniqueSample(kIconSourceHistogram,
                                AnyPagePrimaryIconSource::kDownloaded, 1);
}

TEST_F(AddToHomescreenDataFetcherTest, AnyPage_NoIcon_GeneratedMonogram) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAndroidInstallAnyPageAsDiyAppStopgap);

  GURL document_url("https://www.example.com/page.html");
  NavigateAndCommit(document_url);

  SetManifestAsDefault(document_url);
  SetWebPageMetadata(BuildDefaultMetadata());
  // TestInstallableManager defaults to NO_ACCEPTABLE_ICON if SetPrimaryIcon is
  // not called.

  ObserverWaiter waiter;
  std::unique_ptr<AddToHomescreenDataFetcher> fetcher = BuildFetcher(&waiter);
  RunFetcher(fetcher.get(), waiter, kWebAppInstallInfoTitle,
             base::android::device_info::is_desktop()
                 ? blink::mojom::DisplayMode::kMinimalUi
                 : blink::mojom::DisplayMode::kStandalone,
             AddToHomescreenParams::AppType::WEBAPK_DIY,
             InstallableStatusCode::NO_MANIFEST);

  EXPECT_FALSE(fetcher->primary_icon().drawsNothing());
  EXPECT_EQ(fetcher->shortcut_info().best_primary_icon_url, document_url);
  EXPECT_FALSE(fetcher->shortcut_info().is_primary_icon_maskable);
}

TEST_F(AddToHomescreenDataFetcherTest, AnyPage_ManifestDisplayBrowser) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAndroidInstallAnyPageAsDiyAppStopgap);

  blink::mojom::ManifestPtr manifest = BuildWebAPKManifest();
  manifest->display = blink::mojom::DisplayMode::kBrowser;
  SetManifest(std::move(manifest));

  base::HistogramTester histograms;
  ObserverWaiter waiter;
  std::unique_ptr<AddToHomescreenDataFetcher> fetcher = BuildFetcher(&waiter);
  RunFetcher(fetcher.get(), waiter, kDefaultManifestShortName,
             base::android::device_info::is_desktop()
                 ? blink::mojom::DisplayMode::kMinimalUi
                 : blink::mojom::DisplayMode::kStandalone,
             AddToHomescreenParams::AppType::WEBAPK_DIY,
             InstallableStatusCode::MANIFEST_DISPLAY_NOT_SUPPORTED);

  EXPECT_EQ(fetcher->shortcut_info().name, kDefaultManifestName);
  EXPECT_EQ(fetcher->shortcut_info().short_name, kDefaultManifestShortName);
  histograms.ExpectUniqueSample(kDecisionHistogram,
                                AnyPageDecision::kDiyNotPromotable, 1);
}

TEST_F(AddToHomescreenDataFetcherTest, AnyPage_Manifest404) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAndroidInstallAnyPageAsDiyAppStopgap);

  GURL document_url("https://www.example.com/sub/index.html");
  NavigateAndCommit(document_url);

  auto manifest = blink::mojom::Manifest::New();
  manifest->start_url = document_url;
  manifest->scope = document_url.GetWithoutFilename();
  manifest->id = document_url.GetWithoutRef();
  SetManifestWithError(
      std::move(manifest), GURL(kDefaultManifestUrl),
      InstallableStatusCode::MANIFEST_PARSING_OR_NETWORK_ERROR);

  SetWebPageMetadata(BuildDefaultMetadata());
  SetPrimaryIcon(GURL(kDefaultIconUrl));

  base::HistogramTester histograms;
  ObserverWaiter waiter;
  std::unique_ptr<AddToHomescreenDataFetcher> fetcher = BuildFetcher(&waiter);
  RunFetcher(fetcher.get(), waiter, kWebAppInstallInfoTitle,
             base::android::device_info::is_desktop()
                 ? blink::mojom::DisplayMode::kMinimalUi
                 : blink::mojom::DisplayMode::kStandalone,
             AddToHomescreenParams::AppType::WEBAPK_DIY,
             InstallableStatusCode::MANIFEST_PARSING_OR_NETWORK_ERROR);

  EXPECT_TRUE(fetcher->shortcut_info().manifest_url.is_empty());
  EXPECT_FALSE(fetcher->primary_icon().drawsNothing());
  EXPECT_EQ(fetcher->shortcut_info().best_primary_icon_url,
            GURL(kDefaultIconUrl));
  histograms.ExpectUniqueSample(kDecisionHistogram,
                                AnyPageDecision::kDiyManifestError, 1);
}

TEST_F(AddToHomescreenDataFetcherTest, AnyPage_Promotable) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAndroidInstallAnyPageAsDiyAppStopgap);

  SetManifest(BuildWebAPKManifest());

  ObserverWaiter waiter;
  std::unique_ptr<AddToHomescreenDataFetcher> fetcher = BuildFetcher(&waiter);
  RunFetcher(fetcher.get(), waiter, kDefaultManifestShortName,
             kDefaultManifestName, blink::mojom::DisplayMode::kStandalone,
             AddToHomescreenParams::AppType::WEBAPK,
             InstallableStatusCode::NO_ERROR_DETECTED);

  EXPECT_EQ(fetcher->shortcut_info().best_primary_icon_url,
            GURL(kDefaultIconUrl));
}

TEST_F(AddToHomescreenDataFetcherTest, AnyPage_PromotableIconDownloadFails) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAndroidInstallAnyPageAsDiyAppStopgap);

  GURL document_url(kDefaultStartUrl);

  // The manifest keeps its icons declared, so it stays promotable; only the
  // icon download (round 2) fails.
  SetManifestWithoutIcon(BuildWebAPKManifest());
  SetPrimaryIconError(InstallableStatusCode::NO_ACCEPTABLE_ICON);

  base::HistogramTester histograms;
  ObserverWaiter waiter;
  std::unique_ptr<AddToHomescreenDataFetcher> fetcher = BuildFetcher(&waiter);
  // crafted = errors.empty() is independent of the icon download, and round 3
  // reports its own (empty) error set.
  RunFetcher(fetcher.get(), waiter, kDefaultManifestShortName,
             kDefaultManifestName, blink::mojom::DisplayMode::kStandalone,
             AddToHomescreenParams::AppType::WEBAPK,
             InstallableStatusCode::NO_ERROR_DETECTED);

  // A monogram was generated and keyed by the page URL.
  EXPECT_FALSE(fetcher->primary_icon().drawsNothing());
  EXPECT_FALSE(fetcher->shortcut_info().is_primary_icon_maskable);
  EXPECT_EQ(fetcher->shortcut_info().best_primary_icon_url, document_url);
  EXPECT_EQ(test_installable_manager()->get_data_call_count(), 3);
  histograms.ExpectUniqueSample(kDecisionHistogram, AnyPageDecision::kCrafted,
                                1);
  histograms.ExpectUniqueSample(kIconSourceHistogram,
                                AnyPagePrimaryIconSource::kGenerated, 1);
}

TEST_F(AddToHomescreenDataFetcherTest, AnyPage_ManifestMissingSuitableIcon) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAndroidInstallAnyPageAsDiyAppStopgap);

  GURL document_url("https://www.example.com/index.html");
  NavigateAndCommit(document_url);

  blink::mojom::ManifestPtr manifest = BuildWebAPKManifest();
  manifest->icons.clear();
  SetManifest(std::move(manifest));

  base::HistogramTester histograms;
  ObserverWaiter waiter;
  std::unique_ptr<AddToHomescreenDataFetcher> fetcher = BuildFetcher(&waiter);
  RunFetcher(fetcher.get(), waiter, kDefaultManifestShortName,
             kDefaultManifestName, blink::mojom::DisplayMode::kStandalone,
             AddToHomescreenParams::AppType::WEBAPK_DIY,
             InstallableStatusCode::MANIFEST_MISSING_SUITABLE_ICON);

  EXPECT_FALSE(fetcher->primary_icon().drawsNothing());
  EXPECT_EQ(fetcher->shortcut_info().best_primary_icon_url, document_url);
  histograms.ExpectUniqueSample(kIconSourceHistogram,
                                AnyPagePrimaryIconSource::kGenerated, 1);
}

TEST_F(AddToHomescreenDataFetcherTest,
       AnyPage_DesktopGeneratedIconWithDifferentManifestStartUrl) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAndroidInstallAnyPageAsDiyAppStopgap);

  // Navigate to a subpage while the manifest specifies a different start_url.
  // On BUILDFLAG(IS_DESKTOP_ANDROID), InstallableIconFetcher generates a
  // monogram in Round 2 using `web_contents()->GetLastCommittedURL()` as
  // `primary_icon_url`, whereas `shortcut_info_.url` is updated to
  // `manifest->start_url`. PrimaryIconSource must still record kGenerated.
  GURL document_url("https://www.example.com/subpage.html");
  NavigateAndCommit(document_url);

  blink::mojom::ManifestPtr manifest = BuildWebAPKManifest();
  manifest->icons.clear();
  SetManifest(std::move(manifest));
  SetPrimaryIcon(document_url);

  base::HistogramTester histograms;
  ObserverWaiter waiter;
  std::unique_ptr<AddToHomescreenDataFetcher> fetcher = BuildFetcher(&waiter);
  RunFetcher(fetcher.get(), waiter, kDefaultManifestShortName,
             kDefaultManifestName, blink::mojom::DisplayMode::kStandalone,
             AddToHomescreenParams::AppType::WEBAPK_DIY,
             InstallableStatusCode::MANIFEST_MISSING_SUITABLE_ICON);

  EXPECT_EQ(fetcher->shortcut_info().url, GURL(kDefaultStartUrl));
  EXPECT_EQ(fetcher->shortcut_info().best_primary_icon_url, document_url);
  histograms.ExpectUniqueSample(kIconSourceHistogram,
                                AnyPagePrimaryIconSource::kGenerated, 1);
}

TEST_F(AddToHomescreenDataFetcherTest, AnyPage_ResetDuringRound1) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAndroidInstallAnyPageAsDiyAppStopgap);

  auto manifest = blink::mojom::Manifest::New();
  SetManifestWithError(std::move(manifest), GURL(),
                       InstallableStatusCode::USER_NAVIGATED);

  ObserverWaiter waiter;
  std::unique_ptr<AddToHomescreenDataFetcher> fetcher = BuildFetcher(&waiter);
  // Round 1 completes synchronously inside the constructor here (all page data
  // is cached); the observer must not have been notified re-entrantly.
  EXPECT_FALSE(waiter.title_available());
  waiter.WaitForDataAvailable();

  EXPECT_EQ(waiter.app_type(), AddToHomescreenParams::AppType::SHORTCUT);
  EXPECT_EQ(waiter.installable_status(), InstallableStatusCode::USER_NAVIGATED);
  EXPECT_EQ(test_installable_manager()->get_data_call_count(), 1);
}

TEST_F(AddToHomescreenDataFetcherTest, AnyPage_FlagOff_NoManifestSubPath) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kAndroidInstallAnyPageAsDiyAppStopgap);

  // This pins today's phone behaviour; nothing in the flag-off path keys on
  // the form factor.
  base::android::device_info::set_is_desktop_for_testing(false);
  absl::Cleanup reset_is_desktop = [] {
    base::android::device_info::reset_is_desktop_for_testing();
  };

  GURL document_url("https://www.example.com/subpath/page.html");
  NavigateAndCommit(document_url);

  SetManifestAsDefault(document_url);
  SetWebPageMetadata(BuildDefaultMetadata());
  SetPrimaryIcon(GURL(kDefaultIconUrl));

  base::HistogramTester histograms;
  ObserverWaiter waiter;
  std::unique_ptr<AddToHomescreenDataFetcher> fetcher = BuildFetcher(&waiter);
  RunFetcher(fetcher.get(), waiter, kWebAppInstallInfoTitle,
             blink::mojom::DisplayMode::kBrowser,
             AddToHomescreenParams::AppType::SHORTCUT,
             InstallableStatusCode::NO_MANIFEST);
  histograms.ExpectUniqueSample(kDecisionHistogram,
                                AnyPageDecision::kShortcutNotRootScope, 1);
}

// A page below the origin root whose manifest 404s is a shortcut today.
TEST_F(AddToHomescreenDataFetcherTest, AnyPage_FlagOff_Manifest404) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kAndroidInstallAnyPageAsDiyAppStopgap);

  GURL document_url("https://www.example.com/sub/index.html");
  NavigateAndCommit(document_url);

  auto manifest = blink::mojom::Manifest::New();
  manifest->start_url = document_url;
  manifest->scope = document_url.GetWithoutFilename();
  manifest->id = document_url.GetWithoutRef();
  SetManifestWithError(
      std::move(manifest), GURL(kDefaultManifestUrl),
      InstallableStatusCode::MANIFEST_PARSING_OR_NETWORK_ERROR);
  SetWebPageMetadata(BuildDefaultMetadata());
  SetPrimaryIcon(GURL(kDefaultIconUrl));

  ObserverWaiter waiter;
  std::unique_ptr<AddToHomescreenDataFetcher> fetcher = BuildFetcher(&waiter);
  RunFetcher(fetcher.get(), waiter, kWebAppInstallInfoTitle,
             blink::mojom::DisplayMode::kBrowser,
             AddToHomescreenParams::AppType::SHORTCUT,
             InstallableStatusCode::MANIFEST_PARSING_OR_NETWORK_ERROR);
}

// An empty manifest (opaque origin / sandboxed frame) stays a shortcut even
// with the flag on; the fetched favicon is kept.
TEST_F(AddToHomescreenDataFetcherTest, EmptyManifestParseErrorIsShortcut) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAndroidInstallAnyPageAsDiyAppStopgap);

  SetManifestWithError(
      blink::mojom::Manifest::New(), GURL(),
      InstallableStatusCode::MANIFEST_PARSING_OR_NETWORK_ERROR);
  SetPrimaryIcon(GURL(kDefaultIconUrl));

  ObserverWaiter waiter;
  std::unique_ptr<AddToHomescreenDataFetcher> fetcher = BuildFetcher(&waiter);
  RunFetcher(fetcher.get(), waiter, kWebAppInstallInfoTitle,
             blink::mojom::DisplayMode::kBrowser,
             AddToHomescreenParams::AppType::SHORTCUT,
             InstallableStatusCode::MANIFEST_PARSING_OR_NETWORK_ERROR);
  EXPECT_FALSE(fetcher->primary_icon().drawsNothing());
  EXPECT_EQ(fetcher->shortcut_info().best_primary_icon_url,
            GURL(kDefaultIconUrl));
  EXPECT_EQ(test_installable_manager()->get_data_call_count(), 3);
}

// A plain http page fails eligibility (NOT_FROM_SECURE_ORIGIN) and stays a
// shortcut; the fetched favicon is kept.
TEST_F(AddToHomescreenDataFetcherTest, InsecureHttpIsShortcut) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAndroidInstallAnyPageAsDiyAppStopgap);

  GURL http_url("http://www.example.com/index.html");
  NavigateAndCommit(http_url);
  SetShouldCheckEligibility(true);
  SetManifestAsDefault(http_url);
  SetWebPageMetadata(BuildDefaultMetadata());
  SetPrimaryIcon(GURL(kDefaultIconUrl));

  base::HistogramTester histograms;
  ObserverWaiter waiter;
  std::unique_ptr<AddToHomescreenDataFetcher> fetcher = BuildFetcher(&waiter);
  RunFetcher(fetcher.get(), waiter, kWebAppInstallInfoTitle,
             blink::mojom::DisplayMode::kBrowser,
             AddToHomescreenParams::AppType::SHORTCUT,
             InstallableStatusCode::NOT_FROM_SECURE_ORIGIN);
  EXPECT_FALSE(fetcher->primary_icon().drawsNothing());
  EXPECT_EQ(fetcher->shortcut_info().best_primary_icon_url,
            GURL(kDefaultIconUrl));
  histograms.ExpectUniqueSample(kDecisionHistogram,
                                AnyPageDecision::kShortcutIneligible, 1);
  histograms.ExpectTotalCount(kIconSourceHistogram, 0);
}

// Manifest URLs with embedded credentials are not WebAPK compatible.
TEST_F(AddToHomescreenDataFetcherTest, ManifestWithCredentialsIsShortcut) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAndroidInstallAnyPageAsDiyAppStopgap);

  blink::mojom::ManifestPtr manifest = BuildWebAPKManifest();
  manifest->start_url = GURL("https://user:pass@www.example.com/index.html");
  SetManifest(std::move(manifest));

  base::HistogramTester histograms;
  ObserverWaiter waiter;
  std::unique_ptr<AddToHomescreenDataFetcher> fetcher = BuildFetcher(&waiter);
  waiter.WaitForDataAvailable();
  EXPECT_EQ(waiter.app_type(), AddToHomescreenParams::AppType::SHORTCUT);
  histograms.ExpectUniqueSample(kDecisionHistogram,
                                AnyPageDecision::kShortcutIneligible, 1);
}

// Non-http(s) schemes stay shortcuts even with a usable manifest and icon.
TEST_F(AddToHomescreenDataFetcherTest, ChromeExtensionUrlIsShortcut) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAndroidInstallAnyPageAsDiyAppStopgap);

  GURL extension_url("chrome-extension://abcdefghijklmnop/index.html");
  NavigateAndCommit(extension_url);
  SetManifestAsDefault(extension_url);
  SetWebPageMetadata(BuildDefaultMetadata());
  SetPrimaryIcon(GURL(kDefaultIconUrl));

  ObserverWaiter waiter;
  std::unique_ptr<AddToHomescreenDataFetcher> fetcher = BuildFetcher(&waiter);
  waiter.WaitForDataAvailable();
  EXPECT_EQ(waiter.app_type(), AddToHomescreenParams::AppType::SHORTCUT);
}

// An error page, modelled the way InstallableManager sees one: blink does not
// synthesize the default manifest for it, so the cached manifest stays empty
// (InitPageData) and the empty-manifest gate keeps it a shortcut. The
// committed error page itself is not consulted by this harness.
TEST_F(AddToHomescreenDataFetcherTest, ErrorPageIsShortcut) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAndroidInstallAnyPageAsDiyAppStopgap);

  auto navigation = content::NavigationSimulator::CreateRendererInitiated(
      GURL("https://www.example.com/error"), main_rfh());
  navigation->Fail(net::ERR_FAILED);
  navigation->CommitErrorPage();
  SetPrimaryIcon(GURL(kDefaultIconUrl));

  ObserverWaiter waiter;
  std::unique_ptr<AddToHomescreenDataFetcher> fetcher = BuildFetcher(&waiter);
  waiter.WaitForDataAvailable();
  EXPECT_EQ(waiter.app_type(), AddToHomescreenParams::AppType::SHORTCUT);
  EXPECT_EQ(waiter.installable_status(), InstallableStatusCode::NO_MANIFEST);
}

// A crashed tab whose manifest and metadata were already cached in
// InstallablePageData before the renderer crashed (so InstallableDataFetcher
// returns cached data with NO_ERROR_DETECTED). Round 1 still detects the
// crashed tab via WebContents::IsCrashed(), stops the timer, and falls back to
// an asynchronous SHORTCUT with RENDERER_EXITING without issuing a second
// GetData call.
TEST_F(AddToHomescreenDataFetcherTest, CrashedTabIsShortcut) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAndroidInstallAnyPageAsDiyAppStopgap);

  SetManifest(BuildWebAPKManifest());
  web_contents_tester()->SetIsCrashed(base::TERMINATION_STATUS_PROCESS_CRASHED,
                                      1);

  base::HistogramTester histograms;
  ObserverWaiter waiter;
  std::unique_ptr<AddToHomescreenDataFetcher> fetcher = BuildFetcher(&waiter);
  EXPECT_FALSE(waiter.title_available());
  waiter.WaitForDataAvailable();
  EXPECT_EQ(waiter.app_type(), AddToHomescreenParams::AppType::SHORTCUT);
  EXPECT_EQ(waiter.installable_status(),
            InstallableStatusCode::RENDERER_EXITING);
  EXPECT_EQ(test_installable_manager()->get_data_call_count(), 1);
  histograms.ExpectUniqueSample(kDecisionHistogram,
                                AnyPageDecision::kShortcutReset, 1);
}

// Display mode resolution for DIY apps on phones vs Android Desktop (8.3).
TEST_F(AddToHomescreenDataFetcherTest, DisplayMatrixBothFormFactors) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAndroidInstallAnyPageAsDiyAppStopgap);

  GURL document_url("https://www.example.com/sub/index.html");
  NavigateAndCommit(document_url);

  absl::Cleanup reset_is_desktop = [] {
    base::android::device_info::reset_is_desktop_for_testing();
  };
  for (bool is_desktop : {false, true}) {
    SCOPED_TRACE(is_desktop ? "desktop" : "phone");
    base::android::device_info::set_is_desktop_for_testing(is_desktop);
    const blink::mojom::DisplayMode default_diy_display =
        is_desktop ? blink::mojom::DisplayMode::kMinimalUi
                   : blink::mojom::DisplayMode::kStandalone;

    // 1. No manifest: standalone on phones, minimal-ui on Android Desktop.
    {
      ResetPageData();
      SetPrimaryIcon(GURL(kDefaultIconUrl));
      SetManifestAsDefault(document_url);
      SetWebPageMetadata(BuildDefaultMetadata());
      ObserverWaiter waiter;
      std::unique_ptr<AddToHomescreenDataFetcher> fetcher =
          BuildFetcher(&waiter);
      RunFetcher(fetcher.get(), waiter, kWebAppInstallInfoTitle,
                 default_diy_display,
                 AddToHomescreenParams::AppType::WEBAPK_DIY,
                 InstallableStatusCode::NO_MANIFEST);
    }

    // 2. mobile-web-app-capable: standalone on both.
    {
      ResetPageData();
      SetPrimaryIcon(GURL(kDefaultIconUrl));
      SetManifestAsDefault(document_url);
      auto metadata = BuildDefaultMetadata();
      metadata->mobile_capable = mojom::WebPageMobileCapable::ENABLED;
      SetWebPageMetadata(std::move(metadata));
      ObserverWaiter waiter;
      std::unique_ptr<AddToHomescreenDataFetcher> fetcher =
          BuildFetcher(&waiter);
      RunFetcher(fetcher.get(), waiter, kWebAppInstallInfoTitle,
                 blink::mojom::DisplayMode::kStandalone,
                 AddToHomescreenParams::AppType::WEBAPK_DIY,
                 InstallableStatusCode::NO_MANIFEST);
    }

    // 3. Non-promotable manifest (no icons) with display: standalone: kept.
    {
      ResetPageData();
      SetPrimaryIcon(GURL(kDefaultIconUrl));
      blink::mojom::ManifestPtr manifest = BuildWebAPKManifest();
      manifest->icons.clear();
      manifest->display = blink::mojom::DisplayMode::kStandalone;
      SetManifest(std::move(manifest));
      SetWebPageMetadata(BuildDefaultMetadata());
      ObserverWaiter waiter;
      std::unique_ptr<AddToHomescreenDataFetcher> fetcher =
          BuildFetcher(&waiter);
      RunFetcher(fetcher.get(), waiter, kDefaultManifestShortName,
                 blink::mojom::DisplayMode::kStandalone,
                 AddToHomescreenParams::AppType::WEBAPK_DIY,
                 InstallableStatusCode::MANIFEST_MISSING_SUITABLE_ICON);
    }

    // 4. Manifest display: browser: standalone on phones, minimal-ui on
    // Android Desktop.
    {
      ResetPageData();
      SetPrimaryIcon(GURL(kDefaultIconUrl));
      blink::mojom::ManifestPtr manifest = BuildWebAPKManifest();
      manifest->display = blink::mojom::DisplayMode::kBrowser;
      SetManifest(std::move(manifest));
      SetWebPageMetadata(BuildDefaultMetadata());
      ObserverWaiter waiter;
      std::unique_ptr<AddToHomescreenDataFetcher> fetcher =
          BuildFetcher(&waiter);
      RunFetcher(fetcher.get(), waiter, kDefaultManifestShortName,
                 default_diy_display,
                 AddToHomescreenParams::AppType::WEBAPK_DIY,
                 InstallableStatusCode::MANIFEST_DISPLAY_NOT_SUPPORTED);
    }
  }
}

// Guard #1: the WebContents is destroyed while round 1 is pending.
// InstallableManager resets with RENDERER_EXITING from inside
// ~WebContentsImpl; the fetcher must neither crash nor notify.
TEST_F(AddToHomescreenDataFetcherTest, AbortOnWebContentsDestroyed) {
  SetShouldHoldRequests(true);
  ObserverWaiter waiter;
  std::unique_ptr<AddToHomescreenDataFetcher> fetcher = BuildFetcher(&waiter);

  DestroyWebContents();
  RunPostedTasks();

  EXPECT_FALSE(waiter.title_available());
  EXPECT_FALSE(waiter.data_available());
}

// Guard #2: a navigation while round 1 is pending. The reset callback must
// not issue round 2; the shortcut path runs in a later task.
TEST_F(AddToHomescreenDataFetcherTest, AbortOnUserNavigated) {
  SetShouldHoldRequests(true);
  base::HistogramTester histograms;
  ObserverWaiter waiter;
  std::unique_ptr<AddToHomescreenDataFetcher> fetcher = BuildFetcher(&waiter);
  EXPECT_EQ(test_installable_manager()->get_data_call_count(), 1);

  NavigateAndCommit(GURL("https://www.other.com/"));
  waiter.WaitForDataAvailable();

  EXPECT_EQ(waiter.app_type(), AddToHomescreenParams::AppType::SHORTCUT);
  EXPECT_EQ(waiter.installable_status(), InstallableStatusCode::USER_NAVIGATED);
  EXPECT_EQ(test_installable_manager()->get_data_call_count(), 1);
  histograms.ExpectUniqueSample(kDecisionHistogram,
                                AnyPageDecision::kShortcutReset, 1);
}

// Guard #2: a manifest URL change while round 1 is pending. The reset is
// delivered synchronously here, so this also pins down that nothing reaches
// the observer re-entrantly.
TEST_F(AddToHomescreenDataFetcherTest, AbortOnManifestUrlChanged) {
  SetShouldHoldRequests(true);
  ObserverWaiter waiter;
  std::unique_ptr<AddToHomescreenDataFetcher> fetcher = BuildFetcher(&waiter);

  TriggerManifestUrlChanged(GURL("https://www.example.com/new_manifest.json"));
  EXPECT_FALSE(waiter.title_available());
  EXPECT_FALSE(waiter.data_available());

  waiter.WaitForDataAvailable();
  EXPECT_EQ(waiter.app_type(), AddToHomescreenParams::AppType::SHORTCUT);
  EXPECT_EQ(waiter.installable_status(),
            InstallableStatusCode::MANIFEST_URL_CHANGED);
  EXPECT_EQ(test_installable_manager()->get_data_call_count(), 1);
}

// A reset while round 2 is pending goes through today's "no icon" path:
// SHORTCUT, exactly two GetData calls, no round 3.
TEST_F(AddToHomescreenDataFetcherTest, AnyPage_ResetDuringRound2) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAndroidInstallAnyPageAsDiyAppStopgap);

  GURL document_url(kDefaultStartUrl);
  SetManifestAsDefault(document_url);
  SetWebPageMetadata(BuildDefaultMetadata());
  SetPrimaryIcon(GURL(kDefaultIconUrl));

  base::HistogramTester histograms;
  ObserverWaiter waiter;
  std::unique_ptr<AddToHomescreenDataFetcher> fetcher = BuildFetcher(&waiter);
  // Round 1 completed synchronously and queued round 2 behind it; the
  // manager has not started round 2 yet (that happens in a posted task).
  EXPECT_EQ(test_installable_manager()->get_data_call_count(), 2);
  EXPECT_FALSE(waiter.title_available());

  TriggerManifestUrlChanged(GURL("https://www.example.com/new_manifest.json"));
  waiter.WaitForDataAvailable();

  EXPECT_EQ(waiter.app_type(), AddToHomescreenParams::AppType::SHORTCUT);
  EXPECT_EQ(waiter.installable_status(),
            InstallableStatusCode::MANIFEST_URL_CHANGED);
  EXPECT_EQ(test_installable_manager()->get_data_call_count(), 2);
  histograms.ExpectUniqueSample(kDecisionHistogram,
                                AnyPageDecision::kShortcutReset, 1);
}

// Flag on: the data timeout still ends in SHORTCUT, records exactly one
// decision, and a round that completes after the timer does not reach the
// observer a second time.
TEST_F(AddToHomescreenDataFetcherTest, AnyPage_TimeoutStillShortcut) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAndroidInstallAnyPageAsDiyAppStopgap);

  // Round 1 stays pending until the manager resets it below.
  SetShouldHoldRequests(true);
  base::HistogramTester histograms;
  ObserverWaiter waiter;
  std::unique_ptr<AddToHomescreenDataFetcher> fetcher = BuildFetcher(&waiter);
  RunFetcher(fetcher.get(), waiter, web_contents()->GetTitle(),
             blink::mojom::DisplayMode::kBrowser,
             AddToHomescreenParams::AppType::SHORTCUT,
             InstallableStatusCode::DATA_TIMED_OUT);

  // The late round completes now; the fetcher's weak pointers were
  // invalidated by the timeout, so nothing else is delivered.
  TriggerManifestUrlChanged(GURL("https://www.example.com/new_manifest.json"));
  RunPostedTasks();
  EXPECT_TRUE(waiter.data_available());
  EXPECT_EQ(waiter.installable_status(), InstallableStatusCode::DATA_TIMED_OUT);
  EXPECT_EQ(test_installable_manager()->get_data_call_count(), 1);
  histograms.ExpectUniqueSample(kDecisionHistogram,
                                AnyPageDecision::kShortcutTimeout, 1);
  histograms.ExpectTotalCount(kIconSourceHistogram, 0);
}

// The universal install sheet's observer deletes the fetcher from inside
// OnDataAvailable(). The generated-icon path must not touch |this| after the
// observer call, with the flag off (shortcut) and on (DIY WebAPK).
TEST_F(AddToHomescreenDataFetcherTest, ObserverDeletesFetcher_FlagOff) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kAndroidInstallAnyPageAsDiyAppStopgap);

  GURL document_url("https://www.example.com/subpath/page.html");
  NavigateAndCommit(document_url);
  SetManifestAsDefault(document_url);
  SetWebPageMetadata(BuildDefaultMetadata());
  // No icon: the monogram is generated in OnIconCreated.

  base::HistogramTester histograms;
  FetcherDeletingObserver observer;
  observer.set_fetcher(BuildFetcher(&observer));
  observer.WaitForDataAvailable();

  EXPECT_EQ(observer.app_type(), AddToHomescreenParams::AppType::SHORTCUT);
  histograms.ExpectUniqueSample(kDecisionHistogram,
                                AnyPageDecision::kShortcutNoIcon, 1);
  histograms.ExpectTotalCount(kIconSourceHistogram, 0);
}

TEST_F(AddToHomescreenDataFetcherTest, ObserverDeletesFetcher_FlagOn) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAndroidInstallAnyPageAsDiyAppStopgap);

  GURL document_url("https://www.example.com/subpath/page.html");
  NavigateAndCommit(document_url);
  SetManifestAsDefault(document_url);
  SetWebPageMetadata(BuildDefaultMetadata());
  // No icon: the monogram is generated in OnIconCreated.

  base::HistogramTester histograms;
  FetcherDeletingObserver observer;
  observer.set_fetcher(BuildFetcher(&observer));
  observer.WaitForDataAvailable();

  EXPECT_EQ(observer.app_type(), AddToHomescreenParams::AppType::WEBAPK_DIY);
  histograms.ExpectUniqueSample(kDecisionHistogram,
                                AnyPageDecision::kDiyNoManifest, 1);
  // Recorded before the observer call, i.e. before the fetcher was deleted.
  histograms.ExpectUniqueSample(kIconSourceHistogram,
                                AnyPagePrimaryIconSource::kGenerated, 1);
}

// Off-the-record profiles fail eligibility with IN_INCOGNITO.
class AddToHomescreenDataFetcherIncognitoTest
    : public AddToHomescreenDataFetcherTest {
 protected:
  std::unique_ptr<content::BrowserContext> CreateBrowserContext() override {
    auto context = std::make_unique<content::TestBrowserContext>();
    context->set_is_off_the_record(true);
    return context;
  }
};

TEST_F(AddToHomescreenDataFetcherIncognitoTest, IncognitoIsShortcut) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAndroidInstallAnyPageAsDiyAppStopgap);

  GURL document_url(kDefaultStartUrl);
  SetShouldCheckEligibility(true);
  SetManifestAsDefault(document_url);
  SetWebPageMetadata(BuildDefaultMetadata());
  SetPrimaryIcon(GURL(kDefaultIconUrl));

  base::HistogramTester histograms;
  ObserverWaiter waiter;
  std::unique_ptr<AddToHomescreenDataFetcher> fetcher = BuildFetcher(&waiter);
  RunFetcher(fetcher.get(), waiter, kWebAppInstallInfoTitle,
             blink::mojom::DisplayMode::kBrowser,
             AddToHomescreenParams::AppType::SHORTCUT,
             InstallableStatusCode::IN_INCOGNITO);
  EXPECT_FALSE(fetcher->primary_icon().drawsNothing());
  histograms.ExpectUniqueSample(kDecisionHistogram,
                                AnyPageDecision::kShortcutIneligible, 1);
}

}  // namespace webapps
