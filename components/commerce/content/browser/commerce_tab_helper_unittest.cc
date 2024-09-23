// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/content/browser/commerce_tab_helper.h"

#include "base/memory/ptr_util.h"
#include "components/commerce/content/browser/web_contents_wrapper.h"
#include "components/commerce/core/mock_shopping_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace commerce {

const char kLastMainFrameUrl[] = "https://foo.com";
const char kNewMainFrameUrl[] = "https://foobar.com";

class CommerceTabHelperTest : public content::RenderViewHostTestHarness {
 public:
  CommerceTabHelperTest() = default;
  CommerceTabHelperTest(const CommerceTabHelperTest&) = delete;
  CommerceTabHelperTest& operator=(const CommerceTabHelperTest&) = delete;
  ~CommerceTabHelperTest() override = default;

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    tab_helper_ = base::WrapUnique(new CommerceTabHelper(
        web_contents(), browser_context()->IsOffTheRecord(), &shopping_service_,
        0));
    NavigateAndCommit(GURL(kLastMainFrameUrl));
  }

  void TearDown() override { content::RenderViewHostTestHarness::TearDown(); }

 protected:
  std::unique_ptr<CommerceTabHelper> tab_helper_;
  MockShoppingService shopping_service_;
};

TEST_F(CommerceTabHelperTest, FocusedMainFrameNavigation) {
  FocusWebContentsOnMainFrame();
  NavigateAndCommit(GURL(kNewMainFrameUrl));

  EXPECT_CALL(shopping_service_, GetUrlInfosForRecentlyViewedWebWrappers)
      .WillOnce([this]() {
        return shopping_service_
            .ShoppingService::GetUrlInfosForRecentlyViewedWebWrappers();
      });
  std::vector<UrlInfo> infos =
      shopping_service_.GetUrlInfosForRecentlyViewedWebWrappers();
  EXPECT_EQ(infos.size(), 1u);
  EXPECT_EQ(infos[0].url, GURL(kNewMainFrameUrl));
}

TEST_F(CommerceTabHelperTest, NotFocusedMainFrameNavigation) {
  NavigateAndCommit(GURL(kNewMainFrameUrl));

  EXPECT_CALL(shopping_service_, GetUrlInfosForRecentlyViewedWebWrappers)
      .WillOnce([this]() {
        return shopping_service_
            .ShoppingService::GetUrlInfosForRecentlyViewedWebWrappers();
      });
  std::vector<UrlInfo> infos =
      shopping_service_.GetUrlInfosForRecentlyViewedWebWrappers();
  EXPECT_EQ(infos.size(), 0u);
}

TEST_F(CommerceTabHelperTest, SubFrameNavigation) {
  FocusWebContentsOnMainFrame();
  content::MockNavigationHandle navigation_handle(GURL(kNewMainFrameUrl),
                                                  main_rfh());
  navigation_handle.set_is_in_primary_main_frame(false);
  navigation_handle.set_has_committed(true);
  tab_helper_->DidFinishNavigation(&navigation_handle);

  EXPECT_CALL(shopping_service_, GetUrlInfosForRecentlyViewedWebWrappers)
      .WillOnce([this]() {
        return shopping_service_
            .ShoppingService::GetUrlInfosForRecentlyViewedWebWrappers();
      });
  std::vector<UrlInfo> infos =
      shopping_service_.GetUrlInfosForRecentlyViewedWebWrappers();
  EXPECT_EQ(infos.size(), 0u);
}

}  // namespace commerce
