// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_copy_links_helper.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/browser/ui/toasts/toast_features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/clipboard_url_info.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/clipboard/test/clipboard_test_util.h"
#include "ui/base/clipboard/test/test_clipboard.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"
#include "url/gurl.h"

namespace tabs {
namespace {

class TestToastController : public ToastController {
 public:
  explicit TestToastController(BrowserWindowInterface* browser)
      : ToastController(browser, /*toast_registry=*/nullptr) {}
  MOCK_METHOD(bool, MaybeShowToast, (ToastParams params), (override));
};

class TabCopyLinksHelperTest : public ChromeRenderViewHostTestHarness {
 public:
  TabCopyLinksHelperTest() = default;
  ~TabCopyLinksHelperTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    ui::TestClipboard::CreateForCurrentThread();
  }

  void TearDown() override {
    ui::Clipboard::DestroyClipboardForCurrentThread();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  std::unique_ptr<content::WebContents> CreateTestContentsWithUrl(
      const GURL& url,
      const std::u16string& title = u"") {
    std::unique_ptr<content::WebContents> contents = CreateTestWebContents();
    content::WebContentsTester::For(contents.get())->NavigateAndCommit(url);
    if (!title.empty()) {
      content::WebContentsTester::For(contents.get())->SetTitle(title);
    }
    return contents;
  }

  std::vector<ui::ClipboardUrlInfo> ReadClipboardURLs() {
    return ui::clipboard_test_util::ReadURLs(
        ui::Clipboard::GetForCurrentThread(), /*data_dst=*/nullptr);
  }

  std::string GetClipboardText() {
    return ui::clipboard_test_util::ReadAsciiText(
        ui::Clipboard::GetForCurrentThread(), ui::ClipboardBuffer::kCopyPaste,
        /*data_dst=*/nullptr);
  }
};

TEST_F(TabCopyLinksHelperTest, GetURLForTabCopy) {
  EXPECT_EQ(GetURLForTabCopy(nullptr), GURL());

  std::unique_ptr<content::WebContents> fresh_tab = CreateTestWebContents();
  EXPECT_EQ(GetURLForTabCopy(fresh_tab.get()), GURL());

  std::unique_ptr<content::WebContents> internal_tab =
      CreateTestContentsWithUrl(GURL("chrome://version"));
  EXPECT_EQ(GetURLForTabCopy(internal_tab.get()), GURL());

  std::unique_ptr<content::WebContents> valid_tab =
      CreateTestContentsWithUrl(GURL("https://example.com/valid"));
  EXPECT_EQ(GetURLForTabCopy(valid_tab.get()),
            GURL("https://example.com/valid"));
}

TEST_F(TabCopyLinksHelperTest, CopyLinks_AllValidWebURLs) {
  auto tab1 =
      CreateTestContentsWithUrl(GURL("https://example.com/one"), u"One");
  auto tab2 =
      CreateTestContentsWithUrl(GURL("https://example.com/two"), u"Two");
  auto tab3 = CreateTestContentsWithUrl(GURL("http://localhost:8080/path"),
                                        u"Localhost");

  std::vector<content::WebContents*> web_contents = {tab1.get(), tab2.get(),
                                                     tab3.get()};
  size_t count = CopyLinks(/*browser=*/nullptr, web_contents);

  EXPECT_EQ(count, 3u);
  const std::vector<ui::ClipboardUrlInfo> urls = ReadClipboardURLs();
  ASSERT_EQ(urls.size(), 3u);
  EXPECT_EQ(urls[0].url, GURL("https://example.com/one"));
  EXPECT_EQ(urls[0].title, u"One");
  EXPECT_EQ(urls[1].url, GURL("https://example.com/two"));
  EXPECT_EQ(urls[1].title, u"Two");
  EXPECT_EQ(urls[2].url, GURL("http://localhost:8080/path"));
  EXPECT_EQ(urls[2].title, u"Localhost");
}

TEST_F(TabCopyLinksHelperTest, CopyLinks_SkipsNonWebSchemes) {
  auto tab1 =
      CreateTestContentsWithUrl(GURL("https://example.com/page"), u"Page");
  auto tab2 = CreateTestContentsWithUrl(GURL("chrome://version"));
  auto tab3 = CreateTestContentsWithUrl(GURL("data:text/plain,hello"));
  auto tab4 = CreateTestContentsWithUrl(GURL("blob:https://example.com/id"));
  auto tab5 = CreateTestContentsWithUrl(GURL("file:///tmp/doc.txt"));

  std::vector<content::WebContents*> web_contents = {
      tab1.get(), tab2.get(), tab3.get(), tab4.get(), tab5.get()};
  size_t count = CopyLinks(/*browser=*/nullptr, web_contents);

  EXPECT_EQ(count, 1u);
  const std::vector<ui::ClipboardUrlInfo> urls = ReadClipboardURLs();
  ASSERT_EQ(urls.size(), 1u);
  EXPECT_EQ(urls[0].url, GURL("https://example.com/page"));
  EXPECT_EQ(urls[0].title, u"Page");
}

TEST_F(TabCopyLinksHelperTest, CopyLinks_UsesLastCommittedUrl) {
  auto tab = CreateTestContentsWithUrl(GURL("https://example.com/committed"),
                                       u"Committed");
  auto sim = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://example.com/pending"), tab.get());
  sim->Start();
  EXPECT_EQ(tab->GetVisibleURL(), GURL("https://example.com/pending"));
  EXPECT_EQ(tab->GetLastCommittedURL(), GURL("https://example.com/committed"));

  std::vector<content::WebContents*> web_contents = {tab.get()};
  EXPECT_EQ(CopyLinks(/*browser=*/nullptr, web_contents), 1u);
  const std::vector<ui::ClipboardUrlInfo> urls = ReadClipboardURLs();
  ASSERT_EQ(urls.size(), 1u);
  EXPECT_EQ(urls[0].url, GURL("https://example.com/committed"));
  EXPECT_EQ(urls[0].title, u"Committed");
}

TEST_F(TabCopyLinksHelperTest, CopyLinks_SingleLinkAlsoWritesText) {
  auto tab =
      CreateTestContentsWithUrl(GURL("https://example.com/only"), u"Only");

  std::vector<content::WebContents*> web_contents = {tab.get()};
  size_t count = CopyLinks(/*browser=*/nullptr, web_contents);

  EXPECT_EQ(count, 1u);
  const std::vector<ui::ClipboardUrlInfo> urls = ReadClipboardURLs();
  ASSERT_EQ(urls.size(), 1u);
  EXPECT_EQ(urls[0].url, GURL("https://example.com/only"));
  EXPECT_EQ(urls[0].title, u"Only");

  EXPECT_EQ(GetClipboardText(), "https://example.com/only");
}

TEST_F(TabCopyLinksHelperTest, CopyLinks_NoValidURLs_DoesNotTouchClipboard) {
  {
    ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
    scw.WriteText(u"sentinel");
  }
  EXPECT_EQ(GetClipboardText(), "sentinel");

  auto tab1 = CreateTestContentsWithUrl(GURL("chrome://version"));
  auto tab2 = CreateTestContentsWithUrl(GURL("chrome://newtab"));

  std::vector<content::WebContents*> web_contents = {tab1.get(), tab2.get()};
  size_t count = CopyLinks(/*browser=*/nullptr, web_contents);

  EXPECT_EQ(count, 0u);
  EXPECT_EQ(GetClipboardText(), "sentinel");
  EXPECT_TRUE(ReadClipboardURLs().empty());
}

TEST_F(TabCopyLinksHelperTest, CopyLinks_ToastNotifications) {
  ui::UnownedUserDataHost user_data_host;
  testing::NiceMock<MockBrowserWindowInterface> mock_browser;
  ON_CALL(std::as_const(mock_browser), GetUnownedUserDataHost())
      .WillByDefault(testing::ReturnRef(user_data_host));
  ON_CALL(mock_browser, GetProfile()).WillByDefault(testing::Return(profile()));
  testing::StrictMock<TestToastController> toast_controller(&mock_browser);

  auto tab1 =
      CreateTestContentsWithUrl(GURL("https://example.com/one"), u"One");
  auto tab2 =
      CreateTestContentsWithUrl(GURL("https://example.com/two"), u"Two");
  auto invalid_tab = CreateTestContentsWithUrl(GURL("chrome://version"));

  {
    base::test::ScopedFeatureList enabled_feature(
        toast_features::kLinkCopiedToast);

    // 1 link copied -> ToastId::kLinkCopied with cardinality 1.
    EXPECT_CALL(toast_controller, MaybeShowToast(testing::_))
        .WillOnce([](ToastParams params) {
          EXPECT_EQ(params.toast_id, ToastId::kLinkCopied);
          EXPECT_EQ(params.body_string_cardinality_param, 1);
          return true;
        });
    std::vector<content::WebContents*> one_tab = {tab1.get()};
    EXPECT_EQ(CopyLinks(&mock_browser, one_tab), 1u);
    testing::Mock::VerifyAndClearExpectations(&toast_controller);

    // 2 links copied -> ToastId::kLinkCopied with cardinality 2.
    EXPECT_CALL(toast_controller, MaybeShowToast(testing::_))
        .WillOnce([](ToastParams params) {
          EXPECT_EQ(params.toast_id, ToastId::kLinkCopied);
          EXPECT_EQ(params.body_string_cardinality_param, 2);
          return true;
        });
    std::vector<content::WebContents*> two_tabs = {tab1.get(), tab2.get()};
    EXPECT_EQ(CopyLinks(&mock_browser, two_tabs), 2u);
    testing::Mock::VerifyAndClearExpectations(&toast_controller);

    // 0 copyable links -> no toast.
    EXPECT_CALL(toast_controller, MaybeShowToast(testing::_)).Times(0);
    std::vector<content::WebContents*> zero_tabs = {invalid_tab.get()};
    EXPECT_EQ(CopyLinks(&mock_browser, zero_tabs), 0u);
    testing::Mock::VerifyAndClearExpectations(&toast_controller);
  }

  // Feature disabled -> no toast.
  {
    base::test::ScopedFeatureList disabled_feature;
    disabled_feature.InitAndDisableFeature(toast_features::kLinkCopiedToast);
    EXPECT_CALL(toast_controller, MaybeShowToast(testing::_)).Times(0);
    std::vector<content::WebContents*> one_tab = {tab1.get()};
    EXPECT_EQ(CopyLinks(&mock_browser, one_tab), 1u);
  }
}

}  // namespace
}  // namespace tabs
