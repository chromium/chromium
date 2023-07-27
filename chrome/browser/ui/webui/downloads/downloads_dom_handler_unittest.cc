// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/downloads/downloads_dom_handler.h"

#include <utility>
#include <vector>

#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_service.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/browser/ui/webui/downloads/downloads.mojom.h"
#include "chrome/browser/ui/webui/downloads/mock_downloads_page.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/download/public/common/mock_download_item.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_download_manager.h"
#include "content/public/test/test_web_ui.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kTestDangerousDownloadUrl[] = "http://evildownload.com";

class TestDownloadsDOMHandler : public DownloadsDOMHandler {
 public:
  TestDownloadsDOMHandler(mojo::PendingRemote<downloads::mojom::Page> page,
                          content::DownloadManager* download_manager,
                          content::WebUI* web_ui)
      : DownloadsDOMHandler(
            mojo::PendingReceiver<downloads::mojom::PageHandler>(),
            std::move(page),
            download_manager,
            web_ui) {}

  using DownloadsDOMHandler::FinalizeRemovals;
  using DownloadsDOMHandler::RemoveDownloads;
};

}  // namespace

// A fixture to test DownloadsDOMHandler.
class DownloadsDOMHandlerTest : public testing::Test {
 public:
  DownloadsDOMHandlerTest() = default;

  // testing::Test:
  void SetUp() override {
    ON_CALL(manager_, GetBrowserContext())
        .WillByDefault(testing::Return(&profile_));
  }

  TestingProfile* profile() { return &profile_; }
  content::MockDownloadManager* manager() { return &manager_; }
  content::TestWebUI* web_ui() { return &web_ui_; }

 protected:
  testing::StrictMock<MockPage> page_;

 private:
  // NOTE: The initialization order of these members matters.
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;

  testing::NiceMock<content::MockDownloadManager> manager_;
  content::TestWebUI web_ui_;
};

TEST_F(DownloadsDOMHandlerTest, ChecksForRemovedFiles) {
  EXPECT_CALL(*manager(), CheckForHistoryFilesRemoval());
  TestDownloadsDOMHandler handler(page_.BindAndGetRemote(), manager(),
                                  web_ui());

  testing::Mock::VerifyAndClear(manager());

  EXPECT_CALL(*manager(), CheckForHistoryFilesRemoval());
}

TEST_F(DownloadsDOMHandlerTest, HandleGetDownloads) {
  TestDownloadsDOMHandler handler(page_.BindAndGetRemote(), manager(),
                                  web_ui());

  handler.GetDownloads(std::vector<std::string>());

  EXPECT_CALL(page_, InsertItems(0, testing::_));
}

TEST_F(DownloadsDOMHandlerTest, ClearAll) {
  std::vector<download::DownloadItem*> downloads;

  // Safe, in-progress items should be passed over.
  testing::StrictMock<download::MockDownloadItem> in_progress;
  EXPECT_CALL(in_progress, IsDangerous()).WillOnce(testing::Return(false));
  EXPECT_CALL(in_progress, IsInsecure()).WillOnce(testing::Return(false));
  EXPECT_CALL(in_progress, IsTransient()).WillOnce(testing::Return(false));
  EXPECT_CALL(in_progress, GetState())
      .WillOnce(testing::Return(download::DownloadItem::IN_PROGRESS));
  downloads.push_back(&in_progress);

  // Dangerous items should be removed (regardless of state).
  testing::StrictMock<download::MockDownloadItem> dangerous;
  EXPECT_CALL(dangerous, IsDangerous()).WillOnce(testing::Return(true));
  EXPECT_CALL(dangerous, Remove());
  downloads.push_back(&dangerous);

  // Completed items should be marked as hidden from the shelf.
  testing::StrictMock<download::MockDownloadItem> completed;
  EXPECT_CALL(completed, IsDangerous()).WillOnce(testing::Return(false));
  EXPECT_CALL(completed, IsInsecure()).WillOnce(testing::Return(false));
  EXPECT_CALL(completed, IsTransient()).WillRepeatedly(testing::Return(false));
  EXPECT_CALL(completed, GetState())
      .WillOnce(testing::Return(download::DownloadItem::COMPLETE));
  EXPECT_CALL(completed, GetId()).WillOnce(testing::Return(1));
  EXPECT_CALL(completed, UpdateObservers());
  downloads.push_back(&completed);

  ASSERT_TRUE(DownloadItemModel(&completed).ShouldShowInShelf());

  TestDownloadsDOMHandler handler(page_.BindAndGetRemote(), manager(),
                                  web_ui());
  handler.RemoveDownloads(downloads);

  // Ensure |completed| has been "soft removed" (i.e. can be revived).
  EXPECT_FALSE(DownloadItemModel(&completed).ShouldShowInShelf());

  // Make sure |completed| actually get removed when removals are "finalized".
  EXPECT_CALL(*manager(), GetDownload(1)).WillOnce(testing::Return(&completed));
  EXPECT_CALL(completed, Remove());
  handler.FinalizeRemovals();
}

class DownloadsDOMHandlerWithFakeSafeBrowsingTest
    : public DownloadsDOMHandlerTest {
 public:
  DownloadsDOMHandlerWithFakeSafeBrowsingTest()
      : test_safe_browsing_factory_(
            new safe_browsing::TestSafeBrowsingServiceFactory()) {
    feature_list_.InitAndDisableFeature(
        safe_browsing::kSafeBrowsingCsbrrNewDownloadTrigger);
  }

  void SetUp() override {
    browser_process_ = TestingBrowserProcess::GetGlobal();
    safe_browsing::SafeBrowsingServiceInterface::RegisterFactory(
        test_safe_browsing_factory_.get());
    sb_service_ = static_cast<safe_browsing::SafeBrowsingService*>(
        safe_browsing::SafeBrowsingService::CreateSafeBrowsingService());
    browser_process_->SetSafeBrowsingService(sb_service_.get());
    sb_service_->Initialize();
    base::RunLoop().RunUntilIdle();

    DownloadsDOMHandlerTest::SetUp();
  }

  void TearDown() override {
    browser_process_->safe_browsing_service()->ShutDown();
    browser_process_->SetSafeBrowsingService(nullptr);
    safe_browsing::SafeBrowsingServiceInterface::RegisterFactory(nullptr);
    DownloadsDOMHandlerTest::TearDown();
    base::RunLoop().RunUntilIdle();
  }

 protected:
  void SetUpDangerousDownload() {
    EXPECT_CALL(dangerous_download_, IsDangerous())
        .WillRepeatedly(testing::Return(true));
    EXPECT_CALL(dangerous_download_, IsDone())
        .WillRepeatedly(testing::Return(false));
    EXPECT_CALL(dangerous_download_, GetURL())
        .WillRepeatedly(testing::ReturnRef(download_url_));
    EXPECT_CALL(*manager(), GetDownload(1))
        .WillRepeatedly(testing::Return(&dangerous_download_));
    safe_browsing::DownloadProtectionService::SetDownloadProtectionData(
        &dangerous_download_, "token",
        safe_browsing::ClientDownloadResponse::DANGEROUS,
        safe_browsing::ClientDownloadResponse::TailoredVerdict());
  }

  std::unique_ptr<safe_browsing::TestSafeBrowsingServiceFactory>
      test_safe_browsing_factory_;
  raw_ptr<TestingBrowserProcess> browser_process_;
  scoped_refptr<safe_browsing::SafeBrowsingService> sb_service_;
  GURL download_url_ = GURL(kTestDangerousDownloadUrl);
  testing::StrictMock<download::MockDownloadItem> dangerous_download_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(DownloadsDOMHandlerWithFakeSafeBrowsingTest, DiscardDangerous) {
  SetUpDangerousDownload();

  TestDownloadsDOMHandler handler(page_.BindAndGetRemote(), manager(),
                                  web_ui());

  // Dangerous items should be removed on DiscardDangerous.
  EXPECT_CALL(dangerous_download_, Remove());
  handler.DiscardDangerous("1");

  // Verify that dangerous download report is not sent because the feature flag
  // is disabled.
  EXPECT_TRUE(test_safe_browsing_factory_->test_safe_browsing_service()
                  ->serilized_download_report()
                  .empty());
}

class DownloadsDOMHandlerWithFakeSafeBrowsingTestNewCsbrrTrigger
    : public DownloadsDOMHandlerWithFakeSafeBrowsingTest {
 public:
  DownloadsDOMHandlerWithFakeSafeBrowsingTestNewCsbrrTrigger() {
    feature_list_.InitAndEnableFeature(
        safe_browsing::kSafeBrowsingCsbrrNewDownloadTrigger);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(DownloadsDOMHandlerWithFakeSafeBrowsingTestNewCsbrrTrigger,
       DiscardDangerous) {
  SetUpDangerousDownload();

  TestDownloadsDOMHandler handler(page_.BindAndGetRemote(), manager(),
                                  web_ui());

  // Dangerous items should be removed on DiscardDangerous.
  EXPECT_CALL(dangerous_download_, Remove());
  handler.DiscardDangerous("1");

  // Verify that dangerous download report is sent.
  safe_browsing::ClientSafeBrowsingReportRequest expected_report;
  std::string expected_serialized_report;
  expected_report.set_url(GURL(kTestDangerousDownloadUrl).spec());
  expected_report.set_type(safe_browsing::ClientSafeBrowsingReportRequest::
                               DANGEROUS_DOWNLOAD_RECOVERY);
  expected_report.set_did_proceed(false);
  expected_report.set_download_verdict(
      safe_browsing::ClientDownloadResponse::DANGEROUS);
  expected_report.set_token("token");
  expected_report.SerializeToString(&expected_serialized_report);
  EXPECT_EQ(expected_serialized_report,
            test_safe_browsing_factory_->test_safe_browsing_service()
                ->serilized_download_report());
}

TEST_F(DownloadsDOMHandlerWithFakeSafeBrowsingTestNewCsbrrTrigger,
       DiscardDangerous_IsDone) {
  SetUpDangerousDownload();
  EXPECT_CALL(dangerous_download_, IsDone())
      .WillRepeatedly(testing::Return(true));

  TestDownloadsDOMHandler handler(page_.BindAndGetRemote(), manager(),
                                  web_ui());

  EXPECT_CALL(dangerous_download_, Remove());
  handler.DiscardDangerous("1");

  // Verify that dangerous download report is not sent because the download is
  // already in complete state.
  EXPECT_TRUE(test_safe_browsing_factory_->test_safe_browsing_service()
                  ->serilized_download_report()
                  .empty());
}

TEST_F(DownloadsDOMHandlerWithFakeSafeBrowsingTestNewCsbrrTrigger,
       DiscardDangerous_EmptyURL) {
  SetUpDangerousDownload();
  GURL empty_url = GURL();
  EXPECT_CALL(dangerous_download_, GetURL())
      .WillRepeatedly(testing::ReturnRef(empty_url));

  TestDownloadsDOMHandler handler(page_.BindAndGetRemote(), manager(),
                                  web_ui());

  EXPECT_CALL(dangerous_download_, Remove());
  handler.DiscardDangerous("1");

  // Verify that dangerous download report is not sent because the URL is empty.
  EXPECT_TRUE(test_safe_browsing_factory_->test_safe_browsing_service()
                  ->serilized_download_report()
                  .empty());
}

TEST_F(DownloadsDOMHandlerWithFakeSafeBrowsingTestNewCsbrrTrigger,
       DiscardDangerous_Incognito) {
  SetUpDangerousDownload();
  TestingProfile::Builder otr_profile_builder;
  otr_profile_builder.DisallowBrowserWindows();
  Profile* incognito_profile = otr_profile_builder.BuildIncognito(profile());
  ON_CALL(*manager(), GetBrowserContext())
      .WillByDefault(testing::Return(incognito_profile));

  TestDownloadsDOMHandler handler(page_.BindAndGetRemote(), manager(),
                                  web_ui());

  EXPECT_CALL(dangerous_download_, Remove());
  handler.DiscardDangerous("1");

  // Verify that dangerous download report is not sent because it's in
  // Incognito.
  EXPECT_TRUE(test_safe_browsing_factory_->test_safe_browsing_service()
                  ->serilized_download_report()
                  .empty());
}
