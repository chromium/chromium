// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/android/add_to_homescreen_data_fetcher.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/android_buildflags.h"
#include "components/favicon/content/large_icon_service_getter.h"
#include "components/favicon/core/large_icon_service.h"
#include "components/favicon_base/favicon_types.h"
#include "components/webapps/browser/installable/installable_data.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "components/webapps/browser/installable/installable_manager.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/common/web_page_metadata.mojom.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
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
    if (should_manifest_time_out_) {
      return;
    }

    InitPageData();

    // Do not check if in secure content in unittest.
    InstallableParams test_params = params;
    test_params.check_eligibility = false;

    InstallableManager::GetData(test_params, std::move(callback));
  }

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

  void SetManifest(blink::mojom::ManifestPtr manifest) {
    if (!manifest->icons.empty()) {
      SetPrimaryIcon(manifest->icons[0].src);
    }

    page_data_->OnManifestFetched(std::move(manifest),
                                  GURL(kDefaultManifestUrl));
  }

  void SetPrimaryIcon(const GURL& icon_url) {
    page_data_->OnPrimaryIconFetched(
        icon_url, blink::mojom::ManifestImageResource_Purpose::ANY,
        gfx::test::CreateBitmap(kIconSizePx, kIconSizePx));
  }

  void SetShouldManifestTimeOut(bool should_time_out) {
    should_manifest_time_out_ = should_time_out;
  }

 private:
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

  void SetManifestAsDefault(const GURL& document_url) {
    installable_manager_->SetManifestAsDefault(document_url);
  }

  void SetWebPageMetadata(mojom::WebPageMetadataPtr metadata) {
    installable_manager_->SetWebPageMetadata(std::move(metadata));
  }

  void SetPrimaryIcon(const GURL& icon_url) {
    installable_manager_->SetPrimaryIcon(icon_url);
  }

  void SetShouldManifestTimeOut(bool should_time_out) {
    installable_manager_->SetShouldManifestTimeOut(should_time_out);
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
}

#if BUILDFLAG(IS_DESKTOP_ANDROID)
TEST_F(AddToHomescreenDataFetcherTest, NoManifestDesktopAndroid) {
  // Fake that `InstallableIconFetcher` generated the icon, which is the
  // fallback behavior on desktop Android.
  SetPrimaryIcon(GURL(kDefaultIconUrl));

  ObserverWaiter waiter;
  std::unique_ptr<AddToHomescreenDataFetcher> fetcher = BuildFetcher(&waiter);
  RunFetcher(fetcher.get(), waiter, kWebAppInstallInfoTitle,
             blink::mojom::DisplayMode::kStandalone,
             AddToHomescreenParams::AppType::WEBAPK_DIY,
             InstallableStatusCode::NO_MANIFEST);
}
#endif  // BUILDFLAG(IS_DESKTOP_ANDROID)

TEST_F(AddToHomescreenDataFetcherTest, NoIconManifest) {
  // Test a manifest with no icons. This should use the short name and have
  // a generated icon (empty icon url).
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
#if BUILDFLAG(IS_DESKTOP_ANDROID)
  // Desktop Android expects a standalone DIY WebAPK.
  RunFetcher(fetcher.get(), waiter, kWebAppInstallInfoTitle,
             blink::mojom::DisplayMode::kStandalone,
             AddToHomescreenParams::AppType::WEBAPK_DIY,
             InstallableStatusCode::NO_MANIFEST);
#else
  // Regular Android expects a shortcut.
  RunFetcher(fetcher.get(), waiter, kWebAppInstallInfoTitle,
             blink::mojom::DisplayMode::kBrowser,
             AddToHomescreenParams::AppType::SHORTCUT,
             InstallableStatusCode::NO_MANIFEST);
#endif  // BUILDFLAG(IS_DESKTOP_ANDROID)

  EXPECT_EQ(fetcher->shortcut_info().name, kWebAppInstallInfoTitle);
  EXPECT_EQ(fetcher->shortcut_info().short_name, kWebAppInstallInfoTitle);
  EXPECT_FALSE(fetcher->primary_icon().drawsNothing());
  EXPECT_EQ(fetcher->shortcut_info().best_primary_icon_url,
            GURL(kDefaultIconUrl));
}

}  // namespace webapps
