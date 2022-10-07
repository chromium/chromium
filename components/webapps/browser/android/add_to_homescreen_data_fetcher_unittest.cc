// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/android/add_to_homescreen_data_fetcher.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/favicon/content/large_favicon_provider_getter.h"
#include "components/favicon/core/large_favicon_provider.h"
#include "components/favicon_base/favicon_types.h"
#include "components/webapps/browser/features.h"
#include "components/webapps/browser/installable/installable_data.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "components/webapps/browser/installable/installable_manager.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/common/web_page_metadata.mojom.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "url/gurl.h"

namespace webapps {

namespace {

const char* kWebAppInstallInfoTitle = "Meta Title";
const char* kDefaultManifestUrl = "https://www.example.com/manifest.json";
const char* kDefaultIconUrl = "https://www.example.com/icon.png";
const char* kDefaultManifestName = "Default Name";
const char* kDefaultManifestShortName = "Default Short Name";
const char* kDefaultStartUrl = "https://www.example.com/index.html";
const blink::mojom::DisplayMode kDefaultManifestDisplayMode =
    blink::mojom::DisplayMode::kStandalone;
const int kIconSizePx = 144;

// Tracks which of the AddToHomescreenDataFetcher::Observer methods have been
// called.
class ObserverWaiter : public AddToHomescreenDataFetcher::Observer {
 public:
  ObserverWaiter()
      : is_webapk_compatible_(false),
        title_available_(false),
        data_available_(false) {}

  ObserverWaiter(const ObserverWaiter&) = delete;
  ObserverWaiter& operator=(const ObserverWaiter&) = delete;

  ~ObserverWaiter() override {}

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
                            bool is_webapk_compatible) override {
    // This should only be called once.
    EXPECT_FALSE(title_available_);
    EXPECT_FALSE(data_available_);
    title_available_ = true;
    title_ = title;
    is_webapk_compatible_ = is_webapk_compatible;
  }

  void OnDataAvailable(
      const ShortcutInfo& info,
      const SkBitmap& primary_icon,
      const InstallableStatusCode installable_status) override {
    // This should only be called once.
    EXPECT_FALSE(data_available_);
    EXPECT_TRUE(title_available_);
    data_available_ = true;
    installable_status_ = installable_status;
    if (quit_closure_)
      quit_closure_.Run();
  }

  std::u16string title() const { return title_; }
  bool is_webapk_compatible() const { return is_webapk_compatible_; }
  bool title_available() const { return title_available_; }
  InstallableStatusCode installable_status() const {
    return installable_status_;
  }

 private:
  std::u16string title_;
  bool is_webapk_compatible_;
  bool title_available_;
  bool data_available_;
  InstallableStatusCode installable_status_;
  base::RepeatingClosure quit_closure_;
};

// Builds WebAPK compatible blink::Manifest.
blink::mojom::ManifestPtr BuildDefaultManifest() {
  auto manifest = blink::mojom::Manifest::New();
  manifest->name = base::ASCIIToUTF16(kDefaultManifestName);
  manifest->short_name = base::ASCIIToUTF16(kDefaultManifestShortName);
  manifest->start_url = GURL(kDefaultStartUrl);
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
  // data fetcher.
  void GetData(const InstallableParams& params,
               InstallableCallback callback) override {
    InstallableStatusCode code = NO_ERROR_DETECTED;
    bool is_installable = true;
    if (params.valid_manifest &&
        !IsManifestValidForWebApp(*manifest_,
                                  true /* check_webapp_manifest_display */)) {
      code = valid_manifest_->errors.at(0);
      is_installable = false;
    } else if (params.valid_primary_icon && !primary_icon_) {
      code = NO_ACCEPTABLE_ICON;
      is_installable = false;
    } else if (params.has_worker && !has_worker_) {
      code = NOT_OFFLINE_CAPABLE;
      is_installable = false;
    }

    if (should_manifest_time_out_ ||
        (params.valid_manifest && params.has_worker &&
         should_service_worker_time_out_)) {
      return;
    }

    std::vector<InstallableStatusCode> errors;
    if (code != NO_ERROR_DETECTED)
      errors.push_back(code);
    std::move(callback).Run(
        {std::move(errors), GURL(kDefaultManifestUrl), *manifest_,
         params.valid_primary_icon ? primary_icon_url_ : GURL(),
         params.valid_primary_icon ? primary_icon_.get() : nullptr,
         params.prefer_maskable_icon, GURL() /* splash_icon_url */,
         nullptr /* splash_icon */, params.prefer_maskable_icon,
         std::vector<webapps::Screenshot>() /* screenshots */,
         params.valid_manifest ? is_installable : false,
         params.has_worker ? is_installable : true});
  }

  void SetHasServiceWorker(bool worker) { has_worker_ = worker; }

  void SetManifest(blink::mojom::ManifestPtr manifest) {
    DCHECK(manifest);
    manifest_ = std::move(manifest);

    if (!manifest_->icons.empty()) {
      primary_icon_url_ = manifest_->icons[0].src;
      primary_icon_ = std::make_unique<SkBitmap>(
          gfx::test::CreateBitmap(kIconSizePx, kIconSizePx));
    }
  }

  void SetShouldManifestTimeOut(bool should_time_out) {
    should_manifest_time_out_ = should_time_out;
  }

  void SetShouldServiceWorkerTimeOut(bool should_time_out) {
    should_service_worker_time_out_ = should_time_out;
  }

 private:
  blink::mojom::ManifestPtr manifest_ = blink::mojom::Manifest::New();
  GURL primary_icon_url_;
  std::unique_ptr<SkBitmap> primary_icon_;

  bool has_worker_ = true;

  bool should_manifest_time_out_ = false;
  bool should_service_worker_time_out_ = false;
};

// Tests AddToHomescreenDataFetcher. These tests should be browser tests but
// Android does not support browser tests yet (crbug.com/611756).
class AddToHomescreenDataFetcherTest
    : public content::RenderViewHostTestHarness {
 public:
  AddToHomescreenDataFetcherTest() {}

  AddToHomescreenDataFetcherTest(const AddToHomescreenDataFetcherTest&) =
      delete;
  AddToHomescreenDataFetcherTest& operator=(
      const AddToHomescreenDataFetcherTest&) = delete;

  ~AddToHomescreenDataFetcherTest() override {}

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

    favicon::SetLargeFaviconProviderGetter(base::BindRepeating(
        [](favicon::LargeFaviconProvider* provider,
           content::BrowserContext* context) { return provider; },
        &null_large_favicon_provider_));

    NavigateAndCommit(GURL(kDefaultStartUrl));
  }

  std::unique_ptr<AddToHomescreenDataFetcher> BuildFetcher(
      AddToHomescreenDataFetcher::Observer* observer) {
    return std::make_unique<AddToHomescreenDataFetcher>(web_contents(), 500,
                                                        observer);
  }

  void RunFetcher(AddToHomescreenDataFetcher* fetcher,
                  ObserverWaiter& waiter,
                  const char* expected_user_title,
                  const char* expected_name,
                  blink::mojom::DisplayMode display_mode,
                  bool is_webapk_compatible,
                  InstallableStatusCode status_code) {
    webapps::mojom::WebPageMetadataPtr web_page_metadata(
        webapps::mojom::WebPageMetadata::New());
    web_page_metadata->application_name =
        base::ASCIIToUTF16(kWebAppInstallInfoTitle);

    fetcher->OnDidGetWebPageMetadata(
        mojo::AssociatedRemote<webapps::mojom::WebPageMetadataAgent>(),
        std::move(web_page_metadata));
    waiter.WaitForDataAvailable();

    EXPECT_EQ(is_webapk_compatible, waiter.is_webapk_compatible());
    EXPECT_TRUE(waiter.title_available());
    if (is_webapk_compatible)
      EXPECT_TRUE(base::EqualsASCII(waiter.title(), expected_name));
    else
      EXPECT_TRUE(base::EqualsASCII(waiter.title(), expected_user_title));

    EXPECT_TRUE(base::EqualsASCII(fetcher->shortcut_info().user_title,
                                  expected_user_title));
    EXPECT_EQ(display_mode, fetcher->shortcut_info().display);
    EXPECT_EQ(status_code, waiter.installable_status());
  }

  void RunFetcher(AddToHomescreenDataFetcher* fetcher,
                  ObserverWaiter& waiter,
                  const char* expected_title,
                  blink::mojom::DisplayMode display_mode,
                  bool is_webapk_compatible,
                  InstallableStatusCode status_code) {
    RunFetcher(fetcher, waiter, expected_title, expected_title, display_mode,
               is_webapk_compatible, status_code);
  }

  void CheckHistograms(base::HistogramTester& histograms) {
    histograms.ExpectTotalCount("Webapp.AddToHomescreenDialog.Timeout", 1);
  }

  void SetManifest(blink::mojom::ManifestPtr manifest) {
    installable_manager_->SetManifest(std::move(manifest));
  }

  void SetHasServiceWorker(bool worker) {
    installable_manager_->SetHasServiceWorker(worker);
  }

  void SetShouldManifestTimeOut(bool should_time_out) {
    installable_manager_->SetShouldManifestTimeOut(should_time_out);
  }

  void SetShouldServiceWorkerTimeOut(bool should_time_out) {
    installable_manager_->SetShouldServiceWorkerTimeOut(should_time_out);
  }

 private:
  class NullLargeFaviconProvider : public favicon::LargeFaviconProvider {
   public:
    NullLargeFaviconProvider() = default;
    virtual ~NullLargeFaviconProvider() = default;

    base::CancelableTaskTracker::TaskId GetLargestRawFaviconForPageURL(
        const GURL& page_url,
        const std::vector<favicon_base::IconTypeSet>& icon_types,
        int minimum_size_in_pixels,
        favicon_base::FaviconRawBitmapCallback callback,
        base::CancelableTaskTracker* tracker) override {
      content::GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback),
                                    favicon_base::FaviconRawBitmapResult()));
      return base::CancelableTaskTracker::kBadTaskId;
    }
  };

  raw_ptr<TestInstallableManager> installable_manager_;
  NullLargeFaviconProvider null_large_favicon_provider_;
};

TEST_F(AddToHomescreenDataFetcherTest, EmptyManifest) {
  // Check that an empty manifest has the appropriate methods run.
  base::HistogramTester histograms;
  ObserverWaiter waiter;
  std::unique_ptr<AddToHomescreenDataFetcher> fetcher = BuildFetcher(&waiter);
  RunFetcher(fetcher.get(), waiter, kWebAppInstallInfoTitle,
             blink::mojom::DisplayMode::kBrowser, false,
             InstallableStatusCode::NO_ACCEPTABLE_ICON);
  CheckHistograms(histograms);
}

TEST_F(AddToHomescreenDataFetcherTest, NoIconManifest) {
  // Test a manifest with no icons. This should use the short name and have
  // a generated icon (empty icon url).
  blink::mojom::ManifestPtr manifest = BuildDefaultManifest();
  manifest->icons.clear();
  SetManifest(std::move(manifest));

  base::HistogramTester histograms;
  ObserverWaiter waiter;
  std::unique_ptr<AddToHomescreenDataFetcher> fetcher = BuildFetcher(&waiter);
  RunFetcher(fetcher.get(), waiter, kDefaultManifestShortName,
             blink::mojom::DisplayMode::kStandalone, false,
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
  SetManifest(BuildDefaultManifest());

  // Check a site where InstallableManager finishes working after the time out
  // and determines PWA-ness. This is only relevant when checking WebAPK
  // compatibility.
  base::HistogramTester histograms;
  ObserverWaiter waiter;
  std::unique_ptr<AddToHomescreenDataFetcher> fetcher = BuildFetcher(&waiter);
  RunFetcher(fetcher.get(), waiter, kWebAppInstallInfoTitle,
             blink::mojom::DisplayMode::kBrowser, false,
             InstallableStatusCode::DATA_TIMED_OUT);
  CheckHistograms(histograms);

  EXPECT_FALSE(fetcher->primary_icon().drawsNothing());
  EXPECT_TRUE(fetcher->shortcut_info().best_primary_icon_url.is_empty());
}

TEST_F(AddToHomescreenDataFetcherTest, ManifestFetchTimesOutNonPwa) {
  SetShouldManifestTimeOut(true);
  SetManifest(BuildDefaultManifest());
  SetHasServiceWorker(false);

  // Check where InstallableManager finishes working after the time out and
  // determines non-PWA-ness.
  base::HistogramTester histograms;
  ObserverWaiter waiter;
  std::unique_ptr<AddToHomescreenDataFetcher> fetcher = BuildFetcher(&waiter);
  RunFetcher(fetcher.get(), waiter, kWebAppInstallInfoTitle,
             blink::mojom::DisplayMode::kBrowser, false,
             InstallableStatusCode::DATA_TIMED_OUT);
  CheckHistograms(histograms);

  EXPECT_FALSE(fetcher->primary_icon().drawsNothing());
  EXPECT_TRUE(fetcher->shortcut_info().best_primary_icon_url.is_empty());
}

TEST_F(AddToHomescreenDataFetcherTest, ManifestFetchTimesOutUnknown) {
  SetShouldManifestTimeOut(true);
  SetShouldServiceWorkerTimeOut(true);
  SetManifest(BuildDefaultManifest());

  // Check where InstallableManager doesn't finish working after the time out.
  base::HistogramTester histograms;
  ObserverWaiter waiter;
  std::unique_ptr<AddToHomescreenDataFetcher> fetcher = BuildFetcher(&waiter);
  RunFetcher(fetcher.get(), waiter, kWebAppInstallInfoTitle,
             blink::mojom::DisplayMode::kBrowser, false,
             InstallableStatusCode::DATA_TIMED_OUT);
  NavigateAndCommit(GURL("about:blank"));
  CheckHistograms(histograms);

  EXPECT_FALSE(fetcher->primary_icon().drawsNothing());
  EXPECT_TRUE(fetcher->shortcut_info().best_primary_icon_url.is_empty());
}

// Check that the AddToHomescreenDataFetcher::Observer methods are called if
// the service worker check times out on a page that is installable (i.e. it's
// taken too long). This should use the short_name and icon from the manifest,
// but not be WebAPK-compatible. Only relevant when checking WebAPK
// compatibility.
TEST_F(AddToHomescreenDataFetcherTest, ServiceWorkerCheckTimesOutPwa) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kSkipServiceWorkerCheckInstallOnly);

  SetManifest(BuildDefaultManifest());
  SetShouldServiceWorkerTimeOut(true);

  // Check where InstallableManager finishes working after the timeout and
  // determines PWA-ness.
  base::HistogramTester histograms;
  ObserverWaiter waiter;
  std::unique_ptr<AddToHomescreenDataFetcher> fetcher = BuildFetcher(&waiter);
  RunFetcher(fetcher.get(), waiter, kDefaultManifestShortName,
             blink::mojom::DisplayMode::kStandalone, false,
             InstallableStatusCode::DATA_TIMED_OUT);
  CheckHistograms(histograms);

  EXPECT_FALSE(fetcher->primary_icon().drawsNothing());
  EXPECT_EQ(fetcher->shortcut_info().best_primary_icon_url,
            GURL(kDefaultIconUrl));
}

TEST_F(AddToHomescreenDataFetcherTest, ServiceWorkerCheckTimesOutNonPwa) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kSkipServiceWorkerCheckInstallOnly);

  SetManifest(BuildDefaultManifest());
  SetShouldServiceWorkerTimeOut(true);
  SetHasServiceWorker(false);

  // Check where InstallableManager finishes working after the timeout and
  // determines non-PWA-ness.
  base::HistogramTester histograms;
  ObserverWaiter waiter;
  std::unique_ptr<AddToHomescreenDataFetcher> fetcher = BuildFetcher(&waiter);
  RunFetcher(fetcher.get(), waiter, kDefaultManifestShortName,
             blink::mojom::DisplayMode::kStandalone, false,
             InstallableStatusCode::DATA_TIMED_OUT);
  CheckHistograms(histograms);

  EXPECT_FALSE(fetcher->primary_icon().drawsNothing());
  EXPECT_EQ(fetcher->shortcut_info().best_primary_icon_url,
            GURL(kDefaultIconUrl));
}

TEST_F(AddToHomescreenDataFetcherTest, ServiceWorkerCheckTimesOutUnknown) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kSkipServiceWorkerCheckInstallOnly);

  SetManifest(BuildDefaultManifest());
  SetShouldServiceWorkerTimeOut(true);
  SetHasServiceWorker(false);

  // Check where InstallableManager doesn't finish working after the timeout.
  // This is akin to waiting for a service worker forever.
  base::HistogramTester histograms;
  ObserverWaiter waiter;
  std::unique_ptr<AddToHomescreenDataFetcher> fetcher = BuildFetcher(&waiter);
  RunFetcher(fetcher.get(), waiter, kDefaultManifestShortName,
             blink::mojom::DisplayMode::kStandalone, false,
             InstallableStatusCode::DATA_TIMED_OUT);

  // Navigate to ensure the histograms are written.
  NavigateAndCommit(GURL("about:blank"));
  CheckHistograms(histograms);

  EXPECT_FALSE(fetcher->primary_icon().drawsNothing());
  EXPECT_EQ(fetcher->shortcut_info().best_primary_icon_url,
            GURL(kDefaultIconUrl));
}

TEST_F(AddToHomescreenDataFetcherTest, InstallableManifest) {
  // Test a site that has an offline-capable service worker.
  SetManifest(BuildDefaultManifest());

  base::HistogramTester histograms;
  ObserverWaiter waiter;
  std::unique_ptr<AddToHomescreenDataFetcher> fetcher = BuildFetcher(&waiter);
  RunFetcher(fetcher.get(), waiter, kDefaultManifestShortName,
             kDefaultManifestName, blink::mojom::DisplayMode::kStandalone, true,
             InstallableStatusCode::NO_ERROR_DETECTED);

  // There should always be a primary icon.
  EXPECT_FALSE(fetcher->primary_icon().drawsNothing());
  EXPECT_EQ(fetcher->shortcut_info().best_primary_icon_url,
            GURL(kDefaultIconUrl));

  // Check that splash icon url has been selected.
  EXPECT_EQ(fetcher->shortcut_info().splash_image_url, GURL(kDefaultIconUrl));
  CheckHistograms(histograms);
}

TEST_F(AddToHomescreenDataFetcherTest, ManifestNameClobbersWebApplicationName) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kSkipServiceWorkerCheckInstallOnly);

  // Test that when the manifest provides Manifest::name but not
  // Manifest::short_name that Manifest::name is used as the title.
  {
    // Check the case where we have no icons.
    blink::mojom::ManifestPtr manifest = BuildDefaultManifest();
    manifest->icons.clear();
    manifest->short_name = absl::nullopt;
    SetManifest(std::move(manifest));

    ObserverWaiter waiter;
    std::unique_ptr<AddToHomescreenDataFetcher> fetcher = BuildFetcher(&waiter);
    RunFetcher(fetcher.get(), waiter, kDefaultManifestName,
               blink::mojom::DisplayMode::kStandalone, false,
               InstallableStatusCode::NO_ACCEPTABLE_ICON);

    EXPECT_TRUE(fetcher->shortcut_info().best_primary_icon_url.is_empty());
    EXPECT_TRUE(base::EqualsASCII(fetcher->shortcut_info().short_name,
                                  kDefaultManifestName));
  }

  blink::mojom::ManifestPtr manifest = BuildDefaultManifest();
  manifest->short_name = absl::nullopt;
  SetManifest(std::move(manifest));

  {
    // Check a site with no offline-capable service worker.
    SetHasServiceWorker(false);
    ObserverWaiter waiter;
    std::unique_ptr<AddToHomescreenDataFetcher> fetcher = BuildFetcher(&waiter);
    RunFetcher(fetcher.get(), waiter, kDefaultManifestName,
               blink::mojom::DisplayMode::kStandalone, false,
               InstallableStatusCode::NOT_OFFLINE_CAPABLE);

    EXPECT_FALSE(fetcher->primary_icon().drawsNothing());
    EXPECT_EQ(fetcher->shortcut_info().best_primary_icon_url,
              GURL(kDefaultIconUrl));
    EXPECT_TRUE(base::EqualsASCII(fetcher->shortcut_info().short_name,
                                  kDefaultManifestName));
  }

  {
    // Check a site where we time out waiting for the service worker.
    SetShouldServiceWorkerTimeOut(true);
    ObserverWaiter waiter;
    std::unique_ptr<AddToHomescreenDataFetcher> fetcher = BuildFetcher(&waiter);
    RunFetcher(fetcher.get(), waiter, kDefaultManifestName,
               blink::mojom::DisplayMode::kStandalone, false,
               InstallableStatusCode::DATA_TIMED_OUT);

    EXPECT_FALSE(fetcher->primary_icon().drawsNothing());
    EXPECT_EQ(fetcher->shortcut_info().best_primary_icon_url,
              GURL(kDefaultIconUrl));
    EXPECT_TRUE(base::EqualsASCII(fetcher->shortcut_info().short_name,
                                  kDefaultManifestName));
  }

  {
    // Check a site with an offline-capable service worker.
    SetHasServiceWorker(true);
    SetShouldServiceWorkerTimeOut(false);
    ObserverWaiter waiter;
    std::unique_ptr<AddToHomescreenDataFetcher> fetcher = BuildFetcher(&waiter);
    RunFetcher(fetcher.get(), waiter, kDefaultManifestName,
               blink::mojom::DisplayMode::kStandalone, true,
               InstallableStatusCode::NO_ERROR_DETECTED);

    EXPECT_FALSE(fetcher->primary_icon().drawsNothing());
    EXPECT_EQ(fetcher->shortcut_info().best_primary_icon_url,
              GURL(kDefaultIconUrl));
    EXPECT_TRUE(base::EqualsASCII(fetcher->shortcut_info().short_name,
                                  kDefaultManifestName));
  }
}

TEST_F(AddToHomescreenDataFetcherTest, ManifestNoNameNoShortName) {
  // Test that when the manifest does not provide either Manifest::short_name
  // nor Manifest::name that:
  //  - The page is not WebAPK compatible.
  //  - WebAppInstallInfo::title is used as the "name".
  //  - We still use the icons from the manifest.
  blink::mojom::ManifestPtr manifest = BuildDefaultManifest();
  manifest->name = absl::nullopt;
  manifest->short_name = absl::nullopt;

  // Check the case where we don't time out waiting for the service worker.
  SetManifest(std::move(manifest));
  ObserverWaiter waiter;
  std::unique_ptr<AddToHomescreenDataFetcher> fetcher = BuildFetcher(&waiter);
  RunFetcher(fetcher.get(), waiter, kWebAppInstallInfoTitle,
             blink::mojom::DisplayMode::kStandalone, false,
             InstallableStatusCode::MANIFEST_MISSING_NAME_OR_SHORT_NAME);

  EXPECT_TRUE(base::EqualsASCII(fetcher->shortcut_info().name,
                                kWebAppInstallInfoTitle));
  EXPECT_TRUE(base::EqualsASCII(fetcher->shortcut_info().short_name,
                                kWebAppInstallInfoTitle));
  EXPECT_FALSE(fetcher->primary_icon().drawsNothing());
  EXPECT_EQ(fetcher->shortcut_info().best_primary_icon_url,
            GURL(kDefaultIconUrl));
}

TEST_F(AddToHomescreenDataFetcherTest, NoServiceWorkerInstallable) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kSkipServiceWorkerCheckInstallOnly);

  SetManifest(BuildDefaultManifest());
  SetHasServiceWorker(false);

  // Check where InstallableManager doesn't finish working after the timeout.
  // This is akin to waiting for a service worker forever.
  base::HistogramTester histograms;
  ObserverWaiter waiter;
  std::unique_ptr<AddToHomescreenDataFetcher> fetcher = BuildFetcher(&waiter);
  RunFetcher(fetcher.get(), waiter, kDefaultManifestShortName,
             kDefaultManifestName, blink::mojom::DisplayMode::kStandalone,
             true /*is_webapk_compatible*/,
             InstallableStatusCode::NO_ERROR_DETECTED);

  // Navigate to ensure the histograms are written.
  NavigateAndCommit(GURL("about:blank"));
  CheckHistograms(histograms);

  EXPECT_FALSE(fetcher->primary_icon().drawsNothing());
  EXPECT_EQ(fetcher->shortcut_info().best_primary_icon_url,
            GURL(kDefaultIconUrl));
}

TEST_F(AddToHomescreenDataFetcherTest, ServiceWorkerTimeOutInstallable) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kSkipServiceWorkerCheckInstallOnly);

  SetManifest(BuildDefaultManifest());
  SetShouldServiceWorkerTimeOut(true);

  // Check where InstallableManager doesn't finish working after the timeout.
  // This is akin to waiting for a service worker forever.
  base::HistogramTester histograms;
  ObserverWaiter waiter;
  std::unique_ptr<AddToHomescreenDataFetcher> fetcher = BuildFetcher(&waiter);
  RunFetcher(fetcher.get(), waiter, kDefaultManifestShortName,
             kDefaultManifestName, blink::mojom::DisplayMode::kStandalone,
             true /*is_webapk_compatible*/,
             InstallableStatusCode::NO_ERROR_DETECTED);

  // Navigate to ensure the histograms are written.
  NavigateAndCommit(GURL("about:blank"));
  CheckHistograms(histograms);

  EXPECT_FALSE(fetcher->primary_icon().drawsNothing());
  EXPECT_EQ(fetcher->shortcut_info().best_primary_icon_url,
            GURL(kDefaultIconUrl));
}

}  // namespace webapps
