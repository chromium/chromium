// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/composebox/composebox_handler.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/test/bind.h"
#include "base/unguessable_token.h"
#include "base/version_info/channel.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/searchbox/searchbox_test_utils.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/omnibox/composebox/composebox_query.mojom.h"
#include "components/omnibox/composebox/test_composebox_query_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/webui/resources/cr_components/composebox/composebox.mojom.h"

namespace {
class MockPage : public composebox::mojom::Page {
 public:
  MockPage() = default;
  ~MockPage() override = default;

  mojo::PendingRemote<composebox::mojom::Page> BindAndGetRemote() {
    DCHECK(!receiver_.is_bound());
    return receiver_.BindNewPipeAndPassRemote();
  }

  MOCK_METHOD(void,
              OnContextualInputStatusChanged,
              (const base::UnguessableToken&,
               composebox_query::mojom::FileUploadStatus,
               std::optional<composebox_query::mojom::FileUploadErrorType>));

  mojo::Receiver<composebox::mojom::Page> receiver_{this};
};
}  // namespace

class ComposeboxHandlerBrowserTest : public InProcessBrowserTest {
 public:
  ComposeboxHandlerBrowserTest() {
    set_open_about_blank_on_browser_launch(false);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    shared_url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_factory_);
    fake_variations_client_ = std::make_unique<FakeVariationsClient>();

    auto test_query_controller =
        std::make_unique<TestComposeboxQueryController>(
            /*identity_manager=*/nullptr, shared_url_loader_factory_,
            version_info::Channel::UNKNOWN, "en-US",
            /*template_url_service=*/nullptr, fake_variations_client_.get(),
            /*send_lns_surface=*/false);
    handler_ = std::make_unique<ComposeboxHandler>(
        mojo::PendingReceiver<composebox::mojom::PageHandler>(),
        mock_page_.BindAndGetRemote(),
        mojo::PendingReceiver<searchbox::mojom::PageHandler>(),
        std::move(test_query_controller),
        /*metrics_recorder=*/nullptr, profile(), web_contents(),
        /*metrics_reporter=*/nullptr);
  }

  void TearDownOnMainThread() override {
    handler_.reset();
    fake_variations_client_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  Profile* profile() { return browser()->profile(); }
  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  ComposeboxHandler& handler() { return *handler_; }

 private:
  network::TestURLLoaderFactory test_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  std::unique_ptr<FakeVariationsClient> fake_variations_client_;
  std::unique_ptr<ComposeboxHandler> handler_;
  testing::NiceMock<MockPage> mock_page_;
};

IN_PROC_BROWSER_TEST_F(ComposeboxHandlerBrowserTest, GetTabs) {
  EXPECT_TRUE(AddTabAtIndex(0, GURL("about:blank"), ui::PAGE_TRANSITION_LINK));
  EXPECT_TRUE(
      AddTabAtIndex(1, GURL("chrome://flags"), ui::PAGE_TRANSITION_LINK));

  auto callback = base::BindLambdaForTesting(
      [](std::vector<composebox::mojom::TabInfoPtr> tabs) {
        ASSERT_EQ(tabs.size(), 1u);
        EXPECT_EQ(tabs[0]->title, "about:blank");
      });
  handler().GetTabs(std::move(callback));
}