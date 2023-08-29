// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/android/add_to_homescreen_data_fetcher.h"

#include <memory>
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
#include "content/public/test/web_contents_tester.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
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

// Builds blink::WebPageMetadata.
mojom::WebPageMetadataPtr BuildDefaultMetadata() {
  auto metadata = mojom::WebPageMetadata::New();
  metadata->application_name = kWebAppInstallInfoTitle;
  return metadata;
}

// Builds WebAPK compatible blink::Manifest.
blink::mojom::ManifestPtr BuildDefaultManifest() {
  auto manifest = blink::mojom::Manifest::New();
  manifest->name = kDefaultManifestName;
  manifest->short_name = kDefaultManifestShortName;
  manifest->start_url = GURL(kDefaultStartUrl);
  manifest->id = GURL(kDefaultStartUrl);
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
    std::vector<InstallableStatusCode> errors;
    bool is_installable = true;

    if (blink::IsEmptyManifest(*manifest_)) {
      errors.push_back(MANIFEST_EMPTY);
      is_installable = false;
    }

    if (params.valid_manifest) {
      auto manifest_errors = InstallableEvaluator::IsManifestValidForWebApp(
          *manifest_, true /* check_webapp_manifest_display */);
      if (!manifest_errors.empty()) {
        errors.insert(errors.end(), manifest_errors.begin(),
                      manifest_errors.end());
        is_installable = false;
      }
    }

    if (params.valid_primary_icon && !primary_icon_) {
      errors.push_back(NO_ACCEPTABLE_ICON);
      is_installable = false;
    }

    if (should_manifest_time_out_) {
      return;
    }

    std::move(callback).Run(
        {std::move(errors), GURL(kDefaultManifestUrl), *manifest_, *metadata_,
         params.valid_primary_icon ? primary_icon_url_ : GURL(),
         params.valid_primary_icon ? primary_icon_.get() : nullptr,
         params.prefer_maskable_icon,
         std::vector<webapps::Screenshot>() /* screenshots */,
         params.valid_manifest ? is_installable : false});
  }

  void SetWebPageMetadata(mojom::WebPageMetadataPtr metadata) {
    DCHECK(metadata);
    metadata_ = std::move(metadata);
  }

  void SetManifest(blink::mojom::ManifestPtr manifest) {
    DCHECK(manifest);
    manifest_ = std::move(manifest);

    if (!manifest_->icons.empty()) {
      SetPrimaryIcon(manifest_->icons[0].src);
    }
  }

  void SetPrimaryIcon(const GURL& icon_url) {
    primary_icon_url_ = icon_url;
    primary_icon_ = std::make_unique<SkBitmap>(
        gfx::test::CreateBitmap(kIconSizePx, kIconSizePx));
  }

  void SetShouldManifestTimeOut(bool should_time_out) {
    should_manifest_time_out_ = should_time_out;
  }

 private:
  blink::mojom::ManifestPtr manifest_ = blink::mojom::Manifest::New();
  mojom::WebPageMetadataPtr metadata_ = BuildDefaultMetadata();
  GURL primary_icon_url_;
  std::unique_ptr<SkBitmap> primary_icon_;

  bool should_manifest_time_out_ = false;
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
                  bool is_webapk_compatible,
                  InstallableStatusCode status_code) {
    waiter.WaitForDataAvailable();

    EXPECT_EQ(is_webapk_compatible, waiter.is_webapk_compatible());
    EXPECT_TRUE(waiter.title_available());
    if (is_webapk_compatible)
      EXPECT_EQ(waiter.title(), expected_name);
    else
      EXPECT_EQ(waiter.title(), expected_user_title);

    EXPECT_EQ(fetcher->shortcut_info().user_title, expected_user_title);
    EXPECT_EQ(display_mode, fetcher->shortcut_info().display);
    EXPECT_EQ(status_code, waiter.installable_status());
  }

  void RunFetcher(AddToHomescreenDataFetcher* fetcher,
                  ObserverWaiter& waiter,
                  const std::u16string& expected_title,
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

  void SetWebPageMetadata(mojom::WebPageMetadataPtr metadata) {
    installable_manager_->SetWebPageMetadata(std::move(metadata));
  }

  void SetPrimaryIcon(const GURL& icon_url) {
    installable_manager_->SetPrimaryIcon(icon_url);
  }

  void SetShouldManifestTimeOut(bool should_time_out) {
    installable_manager_->SetShouldManifestTimeOut(should_time_out);
  }

  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  class NullLargeFaviconProvider : public favicon::LargeFaviconProvider {
   public:
    NullLargeFaviconProvider() = default;
    virtual ~NullLargeFaviconProvider() = default;

    MOCK_METHOD5(GetLargeIconRawBitmapOrFallbackStyleForPageUrl,
                 base::CancelableTaskTracker::TaskId(
                     const GURL& page_url,
                     int min_source_size_in_pixel,
                     int desired_size_in_pixel,
                     favicon_base::LargeIconCallback callback,
                     base::CancelableTaskTracker* tracker));
    MOCK_METHOD5(GetLargeIconImageOrFallbackStyleForPageUrl,
                 base::CancelableTaskTracker::TaskId(
                     const GURL& page_url,
                     int min_source_size_in_pixel,
                     int desired_size_in_pixel,
                     favicon_base::LargeIconImageCallback callback,
                     base::CancelableTaskTracker* tracker));
    base::CancelableTaskTracker::TaskId GetLargeIconRawBitmapForPageUrl(
        const GURL& page_url,
        int min_source_size_in_pixel,
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
             InstallableStatusCode::MANIFEST_EMPTY);
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
  RunFetcher(fetcher.get(), waiter, web_contents()->GetTitle(),
             blink::mojom::DisplayMode::kBrowser, false,
             InstallableStatusCode::DATA_TIMED_OUT);
  CheckHistograms(histograms);

  EXPECT_FALSE(fetcher->primary_icon().drawsNothing());
  EXPECT_TRUE(fetcher->shortcut_info().best_primary_icon_url.is_empty());
}

TEST_F(AddToHomescreenDataFetcherTest, ManifestFetchTimesOutNonPwa) {
  SetShouldManifestTimeOut(true);
  SetManifest(BuildDefaultManifest());

  // Check where InstallableManager finishes working after the time out and
  // determines non-PWA-ness.
  base::HistogramTester histograms;
  ObserverWaiter waiter;
  std::unique_ptr<AddToHomescreenDataFetcher> fetcher = BuildFetcher(&waiter);
  RunFetcher(fetcher.get(), waiter, web_contents()->GetTitle(),
             blink::mojom::DisplayMode::kBrowser, false,
             InstallableStatusCode::DATA_TIMED_OUT);
  CheckHistograms(histograms);

  EXPECT_FALSE(fetcher->primary_icon().drawsNothing());
  EXPECT_TRUE(fetcher->shortcut_info().best_primary_icon_url.is_empty());
}

TEST_F(AddToHomescreenDataFetcherTest, ManifestFetchTimesOutUnknown) {
  SetShouldManifestTimeOut(true);
  SetManifest(BuildDefaultManifest());

  // Check where InstallableManager doesn't finish working after the time out.
  base::HistogramTester histograms;
  ObserverWaiter waiter;
  std::unique_ptr<AddToHomescreenDataFetcher> fetcher = BuildFetcher(&waiter);
  RunFetcher(fetcher.get(), waiter, web_contents()->GetTitle(),
             blink::mojom::DisplayMode::kBrowser, false,
             InstallableStatusCode::DATA_TIMED_OUT);
  NavigateAndCommit(GURL("about:blank"));
  CheckHistograms(histograms);

  EXPECT_FALSE(fetcher->primary_icon().drawsNothing());
  EXPECT_TRUE(fetcher->shortcut_info().best_primary_icon_url.is_empty());
}

TEST_F(AddToHomescreenDataFetcherTest, InstallableManifest) {
  // Test a site that has valid manifest.
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

TEST_F(AddToHomescreenDataFetcherTest, ManifestNoNameNoShortName) {
  scoped_feature_list_.InitAndDisableFeature(
      features::kUniversalInstallManifest);
  // Test that when the manifest does not provide either Manifest::short_name
  // nor Manifest::name that:
  //  - The page is not WebAPK compatible.
  //  - WebAppInstallInfo::title is used as the "name".
  //  - We still use the icons from the manifest.
  blink::mojom::ManifestPtr manifest = BuildDefaultManifest();
  manifest->name = absl::nullopt;
  manifest->short_name = absl::nullopt;

  SetManifest(std::move(manifest));
  ObserverWaiter waiter;
  std::unique_ptr<AddToHomescreenDataFetcher> fetcher = BuildFetcher(&waiter);
  RunFetcher(fetcher.get(), waiter, kWebAppInstallInfoTitle,
             blink::mojom::DisplayMode::kStandalone, false,
             InstallableStatusCode::MANIFEST_MISSING_NAME_OR_SHORT_NAME);

  EXPECT_EQ(fetcher->shortcut_info().name, kWebAppInstallInfoTitle);
  EXPECT_EQ(fetcher->shortcut_info().short_name, kWebAppInstallInfoTitle);
  EXPECT_FALSE(fetcher->primary_icon().drawsNothing());
  EXPECT_EQ(fetcher->shortcut_info().best_primary_icon_url,
            GURL(kDefaultIconUrl));
}

}  // namespace webapps
