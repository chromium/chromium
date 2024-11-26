// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chrome_urls/chrome_urls_handler.h"

#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/common/chrome_features.h"
#include "components/chrome_urls_ui/mojom/chrome_urls.mojom.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace chrome_urls {

namespace {

class MockPage : public chrome_urls::mojom::Page {
 public:
  MockPage() = default;
  ~MockPage() override = default;

  mojo::PendingRemote<chrome_urls::mojom::Page> BindAndGetRemote() {
    DCHECK(!receiver_.is_bound());
    return receiver_.BindNewPipeAndPassRemote();
  }

  void FlushForTesting() { receiver_.FlushForTesting(); }

  mojo::Receiver<chrome_urls::mojom::Page> receiver_{this};
};

}  // namespace

class ChromeUrlsHandlerTest : public testing::Test {
 public:
  void SetUp() override {
    handler_ = std::make_unique<chrome_urls::ChromeUrlsHandler>(
        mojo::PendingReceiver<chrome_urls::mojom::PageHandler>(),
        mock_page_.BindAndGetRemote());
    mock_page_.FlushForTesting();
    testing::Mock::VerifyAndClearExpectations(&mock_page_);
  }

 protected:
  base::test::ScopedFeatureList feature_list_{features::kInternalOnlyUisPref};
  content::BrowserTaskEnvironment task_environment_;
  testing::NiceMock<MockPage> mock_page_;
  std::unique_ptr<chrome_urls::ChromeUrlsHandler> handler_;
};

TEST_F(ChromeUrlsHandlerTest, GetUrls) {
  base::MockCallback<chrome_urls::ChromeUrlsHandler::GetUrlsCallback> callback;
  chrome_urls::mojom::ChromeUrlsDataPtr url_data;
  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke(
          [&url_data](chrome_urls::mojom::ChromeUrlsDataPtr arg) {
            url_data = std::move(arg);
          }));
  handler_->GetUrls(callback.Get());

  // Validate WebUI URL data.
  EXPECT_EQ(3u, url_data->webui_urls.size());
  EXPECT_EQ("chrome://settings/", url_data->webui_urls[0]->url.spec());
  EXPECT_TRUE(url_data->webui_urls[0]->enabled);
  EXPECT_EQ("chrome://bookmarks/", url_data->webui_urls[1]->url.spec());
  EXPECT_TRUE(url_data->webui_urls[1]->enabled);
  EXPECT_EQ("chrome://downloads/", url_data->webui_urls[2]->url.spec());
  EXPECT_TRUE(url_data->webui_urls[2]->enabled);

  // Validate command URL data.
  EXPECT_EQ(3u, url_data->command_urls.size());
  EXPECT_EQ("chrome://crash/", url_data->command_urls[0].spec());
  EXPECT_EQ("chrome://kill/", url_data->command_urls[1].spec());
  EXPECT_EQ("chrome://hang/", url_data->command_urls[2].spec());
}

}  // namespace chrome_urls
