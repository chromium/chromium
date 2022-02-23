// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/download_shelf/download_shelf_ui.h"

#include "base/memory/raw_ptr.h"
#include "base/timer/mock_timer.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/download/public/common/mock_download_item.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_web_ui.h"

using download::DownloadItem;
using ::testing::_;
using ::testing::AtLeast;
using ::testing::Return;
using ::testing::ReturnRefOfCopy;
using url::Origin;

namespace {

class TestDownloadShelfHandler : public DownloadShelfHandler {
 public:
  MOCK_METHOD0(DoClose, void());
  MOCK_METHOD0(DoShowAll, void());
  MOCK_METHOD1(DiscardDownload, void(uint32_t));
  MOCK_METHOD1(KeepDownload, void(uint32_t));
  MOCK_METHOD1(GetDownloads,
               void(download_shelf::mojom::PageHandler::GetDownloadsCallback));
  MOCK_METHOD4(ShowContextMenu,
               void(uint32_t download_id,
                    int32_t client_x,
                    int32_t client_y,
                    double timestamp));
  MOCK_METHOD1(DoShowDownload, void(DownloadUIModel*));
  MOCK_METHOD1(OnDownloadOpened, void(uint32_t download_id));
  MOCK_METHOD1(OnDownloadUpdated, void(DownloadUIModel*));
  MOCK_METHOD1(OnDownloadErased, void(uint32_t download_id));
};

class TestDownloadShelfUI : public DownloadShelfUI {
 public:
  explicit TestDownloadShelfUI(
      content::WebUI* web_ui,
      std::unique_ptr<DownloadShelfHandler> page_handler,
      base::RetainingOneShotTimer* timer_)
      : DownloadShelfUI(web_ui) {
    SetPageHandlerForTesting(std::move(page_handler));
    SetProgressTimerForTesting(base::WrapUnique(timer_));
  }
};

class DownloadShelfUITest : public BrowserWithTestWindowTest {
 public:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(profile()));
    web_ui_.set_web_contents(web_contents_.get());
    std::unique_ptr<TestDownloadShelfHandler> page_handler =
        std::make_unique<testing::StrictMock<TestDownloadShelfHandler>>();
    handler_ = page_handler.get();
    mock_timer_ = new base::MockRetainingOneShotTimer();
    download_shelf_ui_ = std::make_unique<TestDownloadShelfUI>(
        &web_ui_, std::move(page_handler), mock_timer_);
  }

  void TearDown() override {
    web_contents_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

  DownloadShelfUITest() = default;

  TestDownloadShelfUI* download_shelf_ui() { return download_shelf_ui_.get(); }

 protected:
  raw_ptr<TestDownloadShelfHandler> handler_;
  raw_ptr<base::MockRetainingOneShotTimer> mock_timer_;

 private:
  std::unique_ptr<content::WebContents> web_contents_;
  content::TestWebUI web_ui_;
  std::unique_ptr<TestDownloadShelfUI> download_shelf_ui_;
};

std::unique_ptr<download::MockDownloadItem> CreateActiveDownloadItem(
    int32_t id) {
  std::unique_ptr<download::MockDownloadItem> item(
      new ::testing::NiceMock<download::MockDownloadItem>());
  ON_CALL(*item, GetURL()).WillByDefault(ReturnRefOfCopy(GURL()));
  ON_CALL(*item, GetTabUrl()).WillByDefault(ReturnRefOfCopy(GURL()));
  ON_CALL(*item, GetDangerType())
      .WillByDefault(Return(download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS));
  ON_CALL(*item, GetFullPath())
      .WillByDefault(ReturnRefOfCopy(base::FilePath()));
  ON_CALL(*item, GetHash()).WillByDefault(ReturnRefOfCopy(std::string()));
  ON_CALL(*item, GetId()).WillByDefault(Return(id));
  ON_CALL(*item, GetState()).WillByDefault(Return(DownloadItem::IN_PROGRESS));
  ON_CALL(*item, GetTargetFilePath())
      .WillByDefault(ReturnRefOfCopy(base::FilePath()));
  ON_CALL(*item, HasUserGesture()).WillByDefault(Return(false));
  ON_CALL(*item, IsDangerous()).WillByDefault(Return(false));
  ON_CALL(*item, IsPaused()).WillByDefault(Return(false));
  ON_CALL(*item, IsTemporary()).WillByDefault(Return(false));
  std::string guid = base::GenerateGUID();
  ON_CALL(*item, GetGuid()).WillByDefault(ReturnRefOfCopy(guid));
  ON_CALL(*item, PercentComplete()).WillByDefault(Return(1));

  return item;
}

}  // namespace

TEST_F(DownloadShelfUITest, DownloadLifecycle) {
  std::unique_ptr<download::MockDownloadItem> download_item =
      CreateActiveDownloadItem(1);
  EXPECT_CALL(*download_item, GetId()).Times(AtLeast(1));
  DownloadUIModel::DownloadUIModelPtr download_model =
      DownloadItemModel::Wrap(download_item.get());
  DownloadUIModel* download_model_ptr = download_model.get();
  EXPECT_CALL(*handler_, DoShowDownload(_)).Times(1);
  download_shelf_ui()->DoShowDownload(std::move(download_model),
                                      base::Time::Now());
  ASSERT_EQ(1u, download_shelf_ui()->GetDownloads().size());

  // Assert handler OnDownloadUpdated called on item progress and update
  // notifications.
  EXPECT_CALL(*handler_, OnDownloadUpdated(_)).Times(2);
  EXPECT_CALL(*download_item, IsPaused()).Times(2);
  EXPECT_CALL(*download_item, PercentComplete()).Times(2);
  mock_timer_->Fire();
  download_item->NotifyObserversDownloadUpdated();

  // Assert handler onDownloadErased called when setting download to not show
  // on shelf.
  EXPECT_CALL(*handler_, OnDownloadErased(_)).Times(1);
  download_model_ptr->SetShouldShowInShelf(false);
  download_item->NotifyObserversDownloadUpdated();

  // Assert handler onDownloadErased called on item removal notification.
  EXPECT_CALL(*handler_, OnDownloadErased(_)).Times(1);
  download_item->NotifyObserversDownloadRemoved();

  // Assert on item destroyed, the download has been removed.
  download_item.reset();
  ASSERT_EQ(0u, download_shelf_ui()->GetDownloads().size());
}

TEST_F(DownloadShelfUITest, DownloadProgress) {
  std::unique_ptr<download::MockDownloadItem> download_item =
      CreateActiveDownloadItem(1);
  EXPECT_CALL(*download_item, GetId()).Times(AtLeast(1));
  DownloadUIModel::DownloadUIModelPtr download_model =
      DownloadItemModel::Wrap(download_item.get());
  EXPECT_CALL(*handler_, DoShowDownload(_)).Times(1);
  download_shelf_ui()->DoShowDownload(std::move(download_model),
                                      base::Time::Now());
  ASSERT_EQ(1u, download_shelf_ui()->GetDownloads().size());
  ASSERT_TRUE(mock_timer_->IsRunning());

  // Pause the download, assert OnDownloadUpdated called only once.
  EXPECT_CALL(*handler_, OnDownloadUpdated(_)).Times(1);
  ON_CALL(*download_item, IsPaused()).WillByDefault(Return(true));
  download_item->NotifyObserversDownloadUpdated();
  mock_timer_->Fire();
  ASSERT_FALSE(mock_timer_->IsRunning());

  // Resume the download, assert OnDownloadUpdated called twice.
  EXPECT_CALL(*handler_, OnDownloadUpdated(_)).Times(2);
  ON_CALL(*download_item, IsPaused()).WillByDefault(Return(false));
  download_item->NotifyObserversDownloadUpdated();
  mock_timer_->Fire();
  ASSERT_TRUE(mock_timer_->IsRunning());

  // Download finished, assert OnDownloadUpdated called only once.
  EXPECT_CALL(*handler_, OnDownloadUpdated(_)).Times(1);
  ON_CALL(*download_item, IsPaused()).WillByDefault(Return(false));
  ON_CALL(*download_item, PercentComplete()).WillByDefault(Return(100));
  download_item->NotifyObserversDownloadUpdated();
  mock_timer_->Fire();
  ASSERT_FALSE(mock_timer_->IsRunning());

  // Assert on item destroyed, the download has been removed.
  download_item.reset();
  ASSERT_EQ(0u, download_shelf_ui()->GetDownloads().size());
}
