// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/data_sharing/data_sharing_page_handler.h"

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/ui/webui/data_sharing/data_sharing_ui.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/data_sharing/public/features.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

class MockPage : public data_sharing::mojom::Page {
 public:
  MockPage() = default;
  ~MockPage() override = default;

  mojo::PendingRemote<data_sharing::mojom::Page> BindAndGetRemote() {
    DCHECK(!receiver_.is_bound());
    return receiver_.BindNewPipeAndPassRemote();
  }

  MOCK_METHOD(void, OnAccessTokenFetched, (const std::string& access_token));
  MOCK_METHOD(void,
              ReadGroups,
              (const std::vector<std::string>& group_ids,
               ReadGroupsCallback callback));
  MOCK_METHOD(void,
              DeleteGroup,
              (const std::string& group_id, DeleteGroupCallback callback));

  mojo::Receiver<data_sharing::mojom::Page> receiver_{this};
};

class TestDataSharingPageHandler : public DataSharingPageHandler {
 public:
  TestDataSharingPageHandler(
      DataSharingUI* webui_controller,
      mojo::PendingRemote<data_sharing::mojom::Page> page)
      : DataSharingPageHandler(
            webui_controller,
            mojo::PendingReceiver<data_sharing::mojom::PageHandler>(),
            std::move(page)) {}
};

}  // namespace

class DataSharingPageHandlerUnitTest : public BrowserWithTestWindowTest {
 public:
  DataSharingPageHandlerUnitTest()
      : BrowserWithTestWindowTest(
            base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME) {}
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {
            data_sharing::features::kDataSharingFeature,
        },
        /*disabled_features=*/{});
    BrowserWithTestWindowTest::SetUp();
    web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(profile()));
    web_ui_.set_web_contents(web_contents_.get());
    webui_controller_ = std::make_unique<DataSharingUI>(&web_ui_);
    handler_ = std::make_unique<TestDataSharingPageHandler>(
        webui_controller_.get(), page_.BindAndGetRemote());
  }

  void TearDown() override {
    web_contents_.reset();
    handler_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

  TestDataSharingPageHandler* handler() { return handler_.get(); }

 protected:
  testing::StrictMock<MockPage> page_;

 private:
  std::unique_ptr<content::WebContents> web_contents_;
  content::TestWebUI web_ui_;
  std::unique_ptr<TestDataSharingPageHandler> handler_;
  std::unique_ptr<DataSharingUI> webui_controller_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(DataSharingPageHandlerUnitTest, GetShareLink) {
  data_sharing::mojom::PageHandler::GetShareLinkCallback callback =
      base::BindLambdaForTesting(
          [&](const GURL& url) { ASSERT_TRUE(url.is_valid()); });
  handler()->GetShareLink("GROUP_ID", "ACCESS_TOKEN", std::move(callback));
}

TEST_F(DataSharingPageHandlerUnitTest, OnAccessTokenFetched) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  GTEST_SKIP() << "N/A for Google Chrome Branding Build";
#else
  // For non-branded build, the access token is set to expire 1 minute later.
  // (See kDummyTokenExpirationDuration in data_sharing_page_handler.cc)
  // Fast forward 1 minute to make sure the access token is refetched once.
  EXPECT_CALL(page_, OnAccessTokenFetched(testing::_)).Times(2);
  task_environment()->FastForwardBy(base::Minutes(1));
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}
