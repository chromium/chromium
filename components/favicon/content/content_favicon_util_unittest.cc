// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon/content/content_favicon_util.h"

#include <memory>

#include "components/favicon/content/content_favicon_driver.h"
#include "components/favicon/core/test/mock_favicon_service.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "net/base/net_errors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace favicon {
namespace {

gfx::Image MakeImage(SkColor color) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(16, 16);
  bitmap.eraseColor(color);
  return gfx::Image::CreateFrom1xBitmap(bitmap);
}

class GetTabFaviconMaybeDesaturatedOnErrorTest
    : public content::RenderViewHostTestHarness {
 protected:
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    ContentFaviconDriver::CreateForWebContents(web_contents(),
                                               &favicon_service_);
  }

  // Sets `image` as the favicon of the last committed navigation entry.
  void SetCommittedFavicon(const gfx::Image& image) {
    content::NavigationEntry* entry =
        web_contents()->GetController().GetLastCommittedEntry();
    ASSERT_TRUE(entry);
    entry->GetFavicon().valid = true;
    entry->GetFavicon().image = image;
  }

  testing::NiceMock<MockFaviconService> favicon_service_;
};

TEST_F(GetTabFaviconMaybeDesaturatedOnErrorTest, ReturnsFaviconForNormalPage) {
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL("https://example.com/"));
  SetCommittedFavicon(MakeImage(SK_ColorRED));

  gfx::Image result = GetTabFaviconMaybeDesaturatedOnError(web_contents());

  ASSERT_FALSE(result.IsEmpty());
  // A non-error page returns the favicon unchanged.
  EXPECT_EQ(SK_ColorRED, result.AsBitmap().getColor(0, 0));
}

TEST_F(GetTabFaviconMaybeDesaturatedOnErrorTest,
       DesaturatesFaviconOnErrorPage) {
  std::unique_ptr<content::NavigationSimulator> navigation =
      content::NavigationSimulator::CreateBrowserInitiated(
          GURL("https://example.com/"), web_contents());
  navigation->Fail(net::ERR_TIMED_OUT);
  navigation->CommitErrorPage();
  ASSERT_EQ(
      content::PAGE_TYPE_ERROR,
      web_contents()->GetController().GetLastCommittedEntry()->GetPageType());
  SetCommittedFavicon(MakeImage(SK_ColorRED));

  gfx::Image result = GetTabFaviconMaybeDesaturatedOnError(web_contents());

  ASSERT_FALSE(result.IsEmpty());
  // An error page's favicon is desaturated, so it is no longer pure red.
  EXPECT_NE(SK_ColorRED, result.AsBitmap().getColor(0, 0));
}

TEST_F(GetTabFaviconMaybeDesaturatedOnErrorTest,
       ReturnsEmptyWithoutFaviconDriver) {
  std::unique_ptr<content::WebContents> contents =
      content::WebContentsTester::CreateTestWebContents(browser_context(),
                                                        nullptr);
  EXPECT_TRUE(GetTabFaviconMaybeDesaturatedOnError(contents.get()).IsEmpty());
}

}  // namespace
}  // namespace favicon
