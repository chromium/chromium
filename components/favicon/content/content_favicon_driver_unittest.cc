// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon/content/content_favicon_driver.h"

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/favicon/core/favicon_client.h"
#include "components/favicon/core/favicon_handler.h"
#include "components/favicon/core/test/favicon_driver_impl_test_helper.h"
#include "components/favicon/core/test/mock_favicon_service.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/favicon/favicon_url.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/favicon_size.h"

namespace favicon {
namespace {

using testing::Return;
using testing::SizeIs;
using testing::_;

void TestFetchFaviconForPage(
    content::WebContents* web_contents,
    const GURL& page_url,
    const std::vector<blink::mojom::FaviconURLPtr>& candidates) {
  ContentFaviconDriver* favicon_driver =
      ContentFaviconDriver::FromWebContents(web_contents);
  content::WebContentsTester::For(web_contents)->NavigateAndCommit(page_url);
  static_cast<content::WebContentsObserver*>(favicon_driver)
      ->DidUpdateFaviconURL(web_contents->GetMainFrame(), candidates);
  base::RunLoop().RunUntilIdle();
}

class ContentFaviconDriverTest : public content::RenderViewHostTestHarness {
 protected:
  const std::vector<gfx::Size> kEmptyIconSizes;
  const std::vector<SkBitmap> kEmptyIcons;
  const GURL kPageURL = GURL("http://www.google.com/");
  const GURL kIconURL = GURL("http://www.google.com/favicon.ico");

  ContentFaviconDriverTest() {
    ON_CALL(favicon_service_, UpdateFaviconMappingsAndFetch(_, _, _, _, _, _))
        .WillByDefault([](auto, auto, auto, auto,
                          favicon_base::FaviconResultsCallback callback,
                          base::CancelableTaskTracker* tracker) {
          return tracker->PostTask(
              base::ThreadTaskRunnerHandle::Get().get(), FROM_HERE,
              base::BindOnce(
                  std::move(callback),
                  std::vector<favicon_base::FaviconRawBitmapResult>()));
        });
    ON_CALL(favicon_service_, GetFaviconForPageURL(_, _, _, _, _))
        .WillByDefault([](auto, auto, auto,
                          favicon_base::FaviconResultsCallback callback,
                          base::CancelableTaskTracker* tracker) {
          return tracker->PostTask(
              base::ThreadTaskRunnerHandle::Get().get(), FROM_HERE,
              base::BindOnce(
                  std::move(callback),
                  std::vector<favicon_base::FaviconRawBitmapResult>()));
        });
  }

  ~ContentFaviconDriverTest() override = default;

  // content::RenderViewHostTestHarness:
  void SetUp() override {
    RenderViewHostTestHarness::SetUp();

    ContentFaviconDriver::CreateForWebContents(web_contents(),
                                               &favicon_service_);
  }

  content::WebContentsTester* web_contents_tester() {
    return content::WebContentsTester::For(web_contents());
  }

  testing::NiceMock<MockFaviconService> favicon_service_;
};

// Test that a download is initiated when there isn't a favicon in the database
// for either the page URL or the icon URL.
TEST_F(ContentFaviconDriverTest, ShouldCauseImageDownload) {
  // Mimic a page load.
  std::vector<blink::mojom::FaviconURLPtr> favicon_urls;
  favicon_urls.push_back(blink::mojom::FaviconURL::New(
      kIconURL, blink::mojom::FaviconIconType::kFavicon, kEmptyIconSizes));
  TestFetchFaviconForPage(web_contents(), kPageURL, favicon_urls);
  EXPECT_TRUE(web_contents_tester()->TestDidDownloadImage(
      kIconURL, 200, kEmptyIcons, kEmptyIconSizes));
}

// Test that no download is initiated when DocumentOnLoadCompletedInMainFrame()
// is not triggered (e.g. user stopped an ongoing page load).
TEST_F(ContentFaviconDriverTest, ShouldNotCauseImageDownload) {
  ContentFaviconDriver* favicon_driver =
      ContentFaviconDriver::FromWebContents(web_contents());
  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      kPageURL, web_contents());
  navigation->SetKeepLoading(true);
  navigation->Commit();
  std::vector<blink::mojom::FaviconURLPtr> favicon_urls;
  favicon_urls.push_back(blink::mojom::FaviconURL::New(
      kIconURL, blink::mojom::FaviconIconType::kFavicon, kEmptyIconSizes));
  static_cast<content::WebContentsObserver*>(favicon_driver)
      ->DidUpdateFaviconURL(web_contents()->GetMainFrame(), favicon_urls);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(web_contents_tester()->HasPendingDownloadImage(kIconURL));

  // Nevertheless, we expect the list exposed via favicon_urls().
  EXPECT_THAT(favicon_driver->favicon_urls(), SizeIs(1));
}

// Test that Favicon is not requested repeatedly during the same session if
// the favicon is known to be unavailable (e.g. due to HTTP 404 status).
TEST_F(ContentFaviconDriverTest, ShouldNotRequestRepeatedlyIfUnavailable) {
  ON_CALL(favicon_service_, WasUnableToDownloadFavicon(kIconURL))
      .WillByDefault(Return(true));
  // Mimic a page load.
  std::vector<blink::mojom::FaviconURLPtr> favicon_urls;
  favicon_urls.push_back(blink::mojom::FaviconURL::New(
      kIconURL, blink::mojom::FaviconIconType::kFavicon, kEmptyIconSizes));
  TestFetchFaviconForPage(web_contents(), kPageURL, favicon_urls);
  // Verify that no download request is pending for the image.
  EXPECT_FALSE(web_contents_tester()->HasPendingDownloadImage(kIconURL));
}

TEST_F(ContentFaviconDriverTest, ShouldDownloadSecondIfFirstUnavailable) {
  const GURL kOtherIconURL = GURL("http://www.google.com/other-favicon.ico");
  ON_CALL(favicon_service_, WasUnableToDownloadFavicon(kIconURL))
      .WillByDefault(Return(true));
  // Mimic a page load.
  std::vector<blink::mojom::FaviconURLPtr> favicon_urls;
  favicon_urls.push_back(blink::mojom::FaviconURL::New(
      kIconURL, blink::mojom::FaviconIconType::kFavicon, kEmptyIconSizes));
  favicon_urls.push_back(blink::mojom::FaviconURL::New(
      kOtherIconURL, blink::mojom::FaviconIconType::kFavicon, kEmptyIconSizes));
  TestFetchFaviconForPage(web_contents(), kPageURL, favicon_urls);
  // Verify a  download request is pending for the second image.
  EXPECT_FALSE(web_contents_tester()->HasPendingDownloadImage(kIconURL));
  EXPECT_TRUE(web_contents_tester()->HasPendingDownloadImage(kOtherIconURL));
}

// Test that ContentFaviconDriver ignores updated favicon URLs if there is no
// last committed entry. This occurs when script is injected in about:blank.
// See crbug.com/520759 for more details
TEST_F(ContentFaviconDriverTest, FaviconUpdateNoLastCommittedEntry) {
  ASSERT_EQ(nullptr, web_contents()->GetController().GetLastCommittedEntry());

  std::vector<blink::mojom::FaviconURLPtr> favicon_urls;
  favicon_urls.push_back(blink::mojom::FaviconURL::New(
      GURL("http://www.google.ca/favicon.ico"),
      blink::mojom::FaviconIconType::kFavicon, kEmptyIconSizes));
  favicon::ContentFaviconDriver* driver =
      favicon::ContentFaviconDriver::FromWebContents(web_contents());
  static_cast<content::WebContentsObserver*>(driver)->DidUpdateFaviconURL(
      web_contents()->GetMainFrame(), favicon_urls);

  // Test that ContentFaviconDriver ignored the favicon url update.
  EXPECT_TRUE(driver->favicon_urls().empty());
}

using ContentFaviconDriverTestNoFaviconService =
    content::RenderViewHostTestHarness;

// This test verifies a crash doesn't happen during deletion of the
// WebContents. The crash occurred because ~WebContentsImpl would trigger
// running callbacks for manifests. This mean FaviconHandler would be called
// while ContentFaviconDriver::web_contents() was null, which is unexpected and
// crashed. See https://crbug.com/1114237 for more.
TEST_F(ContentFaviconDriverTestNoFaviconService,
       WebContentsDeletedWithInProgressManifestRequest) {
  ContentFaviconDriver::CreateForWebContents(web_contents(), nullptr);
  // Manifests are only downloaded with TOUCH_LARGEST. Force creating this
  // handler so code path is exercised on all platforms.
  favicon::ContentFaviconDriver* driver =
      favicon::ContentFaviconDriver::FromWebContents(web_contents());
  FaviconDriverImplTestHelper::RecreateHandlerForType(
      driver, FaviconDriverObserver::TOUCH_LARGEST);

  // Mimic a page load.
  std::vector<blink::mojom::FaviconURLPtr> favicon_urls;
  favicon_urls.push_back(blink::mojom::FaviconURL::New(
      GURL("http://www.google.com/favicon.ico"),
      blink::mojom::FaviconIconType::kTouchIcon, std::vector<gfx::Size>()));
  TestFetchFaviconForPage(web_contents(), GURL("http://www.google.com/"),
                          favicon_urls);

  // Trigger downloading a manifest.
  static_cast<content::WebContentsObserver*>(driver)->DidUpdateWebManifestURL(
      web_contents()->GetMainFrame(), GURL("http://bad.manifest.com"));

  // The request for the manifest is still pending, delete the WebContents,
  // which should trigger notifying the callback for the manifest and *not*
  // crash.
  DeleteContents();
}

}  // namespace
}  // namespace favicon
