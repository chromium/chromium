// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/downloads/downloads_dom_handler.h"

#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_service.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/browser/ui/hats/mock_trust_safety_sentiment_service.h"
#include "chrome/browser/ui/hats/trust_safety_sentiment_service_factory.h"
#include "chrome/browser/ui/webui/downloads/downloads.mojom.h"
#include "chrome/browser/ui/webui/downloads/mock_downloads_page.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/download/public/common/mock_download_item.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_download_manager.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_web_ui.h"
#include "content/public/test/web_contents_tester.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::ReturnRefOfCopy;

const char kTestDangerousDownloadUrl[] = "http://evildownload.com";
const char kTestDangerousDownloadReferrerUrl[] = "http://referrer.test";

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
    web_contents_ =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
    web_ui()->set_web_contents(web_contents_.get());
  }

  void SimulateMouseGestureOnWebUI() {
    content::WebContentsTester::For(web_ui()->GetWebContents())
        ->TestDidReceiveMouseDownEvent();
  }

  TestingProfile* profile() { return &profile_; }
  content::MockDownloadManager* manager() { return &manager_; }
  content::TestWebUI* web_ui() { return &web_ui_; }

 protected:
  testing::StrictMock<MockPage> page_;
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  // NOTE: The initialization order of these members matters.
  TestingProfile profile_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  std::unique_ptr<content::WebContents> web_contents_;

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
  std::vector<raw_ptr<download::DownloadItem, VectorExperimental>> downloads;

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
            new safe_browsing::TestSafeBrowsingServiceFactory()) {}

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
        .WillRepeatedly(Return(true));
    ON_CALL(dangerous_download_, IsInsecure()).WillByDefault(Return(false));
    EXPECT_CALL(dangerous_download_, IsDone()).WillRepeatedly(Return(false));
    EXPECT_CALL(dangerous_download_, GetURL())
        .WillRepeatedly(ReturnRef(download_url_));
    EXPECT_CALL(dangerous_download_, GetReferrerUrl())
        .WillRepeatedly(ReturnRef(referrer_url_));
    ON_CALL(dangerous_download_, GetTargetFilePath())
        .WillByDefault(
            ReturnRefOfCopy(base::FilePath(FILE_PATH_LITERAL("foo.pdf"))));
    EXPECT_CALL(*manager(), GetDownload(1))
        .WillRepeatedly(Return(&dangerous_download_));
    safe_browsing::DownloadProtectionService::SetDownloadProtectionData(
        &dangerous_download_, "token",
        safe_browsing::ClientDownloadResponse::DANGEROUS,
        safe_browsing::ClientDownloadResponse::TailoredVerdict());
    content::DownloadItemUtils::AttachInfoForTesting(&dangerous_download_,
                                                     profile(), nullptr);
  }

  void SetUpInsecureDownload() {
    ON_CALL(dangerous_download_, IsDangerous()).WillByDefault(Return(false));
    EXPECT_CALL(dangerous_download_, IsInsecure()).WillRepeatedly(Return(true));
    EXPECT_CALL(dangerous_download_, IsDone()).WillRepeatedly(Return(false));
    EXPECT_CALL(dangerous_download_, GetURL())
        .WillRepeatedly(ReturnRef(download_url_));
    EXPECT_CALL(*manager(), GetDownload(1))
        .WillRepeatedly(Return(&dangerous_download_));
    content::DownloadItemUtils::AttachInfoForTesting(&dangerous_download_,
                                                     profile(), nullptr);
  }

  std::unique_ptr<safe_browsing::TestSafeBrowsingServiceFactory>
      test_safe_browsing_factory_;
  raw_ptr<TestingBrowserProcess> browser_process_;
  scoped_refptr<safe_browsing::SafeBrowsingService> sb_service_;
  GURL download_url_ = GURL(kTestDangerousDownloadUrl);
  GURL referrer_url_ = GURL(kTestDangerousDownloadReferrerUrl);
  testing::NiceMock<download::MockDownloadItem> dangerous_download_;
};

TEST_F(DownloadsDOMHandlerWithFakeSafeBrowsingTest, DiscardDangerous) {
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
                ->serialized_download_report());
}

TEST_F(DownloadsDOMHandlerWithFakeSafeBrowsingTest, DiscardDangerous_IsDone) {
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
                  ->serialized_download_report()
                  .empty());
}

TEST_F(DownloadsDOMHandlerWithFakeSafeBrowsingTest,
       SaveSuspiciousRequiringGesture) {
  SetUpDangerousDownload();

  TestDownloadsDOMHandler handler(page_.BindAndGetRemote(), manager(),
                                  web_ui());

  SimulateMouseGestureOnWebUI();

  EXPECT_CALL(dangerous_download_, ValidateDangerousDownload());
  handler.SaveSuspiciousRequiringGesture("1");

  // Verify that dangerous download report is sent.
  safe_browsing::ClientSafeBrowsingReportRequest expected_report;
  std::string expected_serialized_report;
  expected_report.set_url(GURL(kTestDangerousDownloadUrl).spec());
  expected_report.set_type(safe_browsing::ClientSafeBrowsingReportRequest::
                               DANGEROUS_DOWNLOAD_RECOVERY);
  expected_report.set_did_proceed(true);
  expected_report.set_download_verdict(
      safe_browsing::ClientDownloadResponse::DANGEROUS);
  expected_report.set_token("token");
  expected_report.SerializeToString(&expected_serialized_report);
  EXPECT_EQ(expected_serialized_report,
            test_safe_browsing_factory_->test_safe_browsing_service()
                ->serialized_download_report());
}

TEST_F(DownloadsDOMHandlerWithFakeSafeBrowsingTest,
       SaveSuspiciousRequiringGesture_InsecureDownload) {
  SetUpInsecureDownload();

  TestDownloadsDOMHandler handler(page_.BindAndGetRemote(), manager(),
                                  web_ui());

  SimulateMouseGestureOnWebUI();

  EXPECT_CALL(dangerous_download_, ValidateInsecureDownload());
  handler.SaveSuspiciousRequiringGesture("1");

  // No dangerous download report is sent for insecure downloads.
  EXPECT_TRUE(test_safe_browsing_factory_->test_safe_browsing_service()
                  ->serialized_download_report()
                  .empty());
}

TEST_F(DownloadsDOMHandlerWithFakeSafeBrowsingTest,
       SaveSuspiciousRequiringGesture_NoRecentInteraction) {
  SetUpDangerousDownload();

  TestDownloadsDOMHandler handler(page_.BindAndGetRemote(), manager(),
                                  web_ui());

  EXPECT_CALL(dangerous_download_, ValidateDangerousDownload()).Times(0);
  handler.SaveSuspiciousRequiringGesture("1");
}

TEST_F(DownloadsDOMHandlerWithFakeSafeBrowsingTest,
       SaveDangerousFromDialogRequiringGesture) {
  SetUpDangerousDownload();

  TestDownloadsDOMHandler handler(page_.BindAndGetRemote(), manager(),
                                  web_ui());

  SimulateMouseGestureOnWebUI();

  EXPECT_CALL(dangerous_download_, ValidateDangerousDownload());
  handler.SaveDangerousFromDialogRequiringGesture("1");

  // Verify that dangerous download report is sent.
  safe_browsing::ClientSafeBrowsingReportRequest expected_report;
  std::string expected_serialized_report;
  expected_report.set_url(GURL(kTestDangerousDownloadUrl).spec());
  expected_report.set_type(safe_browsing::ClientSafeBrowsingReportRequest::
                               DANGEROUS_DOWNLOAD_RECOVERY);
  expected_report.set_did_proceed(true);
  expected_report.set_download_verdict(
      safe_browsing::ClientDownloadResponse::DANGEROUS);
  expected_report.set_token("token");
  expected_report.SerializeToString(&expected_serialized_report);
  EXPECT_EQ(expected_serialized_report,
            test_safe_browsing_factory_->test_safe_browsing_service()
                ->serialized_download_report());
}

TEST_F(DownloadsDOMHandlerWithFakeSafeBrowsingTest,
       SaveDangerousFromDialogRequiringGesture_NoRecentInteraction) {
  SetUpDangerousDownload();

  TestDownloadsDOMHandler handler(page_.BindAndGetRemote(), manager(),
                                  web_ui());

  EXPECT_CALL(dangerous_download_, ValidateDangerousDownload()).Times(0);
  handler.SaveDangerousFromDialogRequiringGesture("1");
}

TEST_F(DownloadsDOMHandlerWithFakeSafeBrowsingTest,
       RecordCancelBypassWarningDialog) {
  SetUpDangerousDownload();

  TestDownloadsDOMHandler handler(page_.BindAndGetRemote(), manager(),
                                  web_ui());

  EXPECT_CALL(dangerous_download_, ValidateDangerousDownload()).Times(0);
  handler.RecordCancelBypassWarningDialog("1");

  // Verify no cancel report is sent, since it's not a terminal action.
  EXPECT_TRUE(test_safe_browsing_factory_->test_safe_browsing_service()
                  ->serialized_download_report()
                  .empty());
}

class DownloadsDOMHandlerTestDangerousDownloadInterstitial
    : public DownloadsDOMHandlerWithFakeSafeBrowsingTest {
 public:
  DownloadsDOMHandlerTestDangerousDownloadInterstitial() = default;

  void SetUp() override {
    DownloadsDOMHandlerWithFakeSafeBrowsingTest::SetUp();
    SetUpDangerousDownload();
    handler_ = std::make_unique<TestDownloadsDOMHandler>(
        page_.BindAndGetRemote(), manager(), web_ui());
    handler_->RecordOpenBypassWarningInterstitial("1");
    task_environment_.FastForwardBy(base::Milliseconds(200));
  }

  void TearDown() override {
    DownloadsDOMHandlerWithFakeSafeBrowsingTest::TearDown();
  }

 protected:
  base::HistogramTester histogram_tester_;
  std::unique_ptr<TestDownloadsDOMHandler> handler_;

 private:
  base::test::ScopedFeatureList feature_list_{
      safe_browsing::kDangerousDownloadInterstitial};
};

TEST_F(DownloadsDOMHandlerTestDangerousDownloadInterstitial,
       RecordOpenBypassWarningInterstitial) {
  EXPECT_CALL(dangerous_download_, ValidateDangerousDownload()).Times(0);

  histogram_tester_.ExpectBucketCount(
      "Download.DangerousDownloadInterstitial.Action",
      DangerousDownloadInterstitialAction::kOpenInterstitial, 1);

  // Verify no report is sent, since it's not a terminal action.
  EXPECT_TRUE(test_safe_browsing_factory_->test_safe_browsing_service()
                  ->serialized_download_report()
                  .empty());
}

TEST_F(DownloadsDOMHandlerTestDangerousDownloadInterstitial,
       RecordOpenSurveyOnDangerousInterstitial) {
  EXPECT_CALL(dangerous_download_, ValidateDangerousDownload()).Times(0);
  handler_->RecordOpenSurveyOnDangerousInterstitial("1");

  histogram_tester_.ExpectBucketCount(
      "Download.DangerousDownloadInterstitial.Action",
      DangerousDownloadInterstitialAction::kOpenSurvey, 1);
  histogram_tester_.ExpectTimeBucketCount(
      "Download.DangerousDownloadInterstitial.InteractionTime.OpenSurvey",
      base::Milliseconds(200), 1);
}

TEST_F(DownloadsDOMHandlerTestDangerousDownloadInterstitial,
       RecordCancelBypassWarningInterstitial) {
  EXPECT_CALL(dangerous_download_, ValidateDangerousDownload()).Times(0);
  handler_->RecordCancelBypassWarningInterstitial("1");

  histogram_tester_.ExpectBucketCount(
      "Download.DangerousDownloadInterstitial.Action",
      DangerousDownloadInterstitialAction::kCancelInterstitial, 1);
  histogram_tester_.ExpectTimeBucketCount(
      "Download.DangerousDownloadInterstitial.InteractionTime."
      "CancelInterstitial",
      base::Milliseconds(200), 1);

  // Verify no report is sent, since it's not a terminal action.
  EXPECT_TRUE(test_safe_browsing_factory_->test_safe_browsing_service()
                  ->serialized_download_report()
                  .empty());
}

TEST_F(DownloadsDOMHandlerTestDangerousDownloadInterstitial,
       SaveDangerousFromInterstitialNeedGesture) {
  SimulateMouseGestureOnWebUI();

  EXPECT_CALL(dangerous_download_, ValidateDangerousDownload());

  handler_->RecordOpenSurveyOnDangerousInterstitial("1");
  task_environment_.FastForwardBy(base::Milliseconds(100));
  handler_->SaveDangerousFromInterstitialNeedGesture(
      "1", downloads::mojom::DangerousDownloadInterstitialSurveyOptions::
               kAcceptRisk);

  histogram_tester_.ExpectBucketCount(
      "Download.DangerousDownloadInterstitial.Action",
      DangerousDownloadInterstitialAction::kSaveDangerous, 1);
  histogram_tester_.ExpectTimeBucketCount(
      "Download.DangerousDownloadInterstitial.InteractionTime.CompleteSurvey",
      base::Milliseconds(100), 1);
  histogram_tester_.ExpectTimeBucketCount(
      "Download.DangerousDownloadInterstitial.InteractionTime.SaveDangerous",
      base::Milliseconds(300), 1);
  histogram_tester_.ExpectBucketCount(
      "Download.DangerousDownloadInterstitial.SurveyResponse",
      downloads::mojom::DangerousDownloadInterstitialSurveyOptions::kAcceptRisk,
      1);

  // Verify that dangerous download report is sent.
  safe_browsing::ClientSafeBrowsingReportRequest expected_report;
  std::string expected_serialized_report;
  expected_report.set_url(GURL(kTestDangerousDownloadUrl).spec());
  expected_report.set_type(safe_browsing::ClientSafeBrowsingReportRequest::
                               DANGEROUS_DOWNLOAD_RECOVERY);
  expected_report.set_did_proceed(true);
  expected_report.set_download_verdict(
      safe_browsing::ClientDownloadResponse::DANGEROUS);
  expected_report.set_token("token");
  expected_report.SerializeToString(&expected_serialized_report);
  EXPECT_EQ(expected_serialized_report,
            test_safe_browsing_factory_->test_safe_browsing_service()
                ->serialized_download_report());
}

TEST_F(DownloadsDOMHandlerTestDangerousDownloadInterstitial,
       SaveDangerousFromInterstitialNeedGesture_NoRecentInteraction) {
  EXPECT_CALL(dangerous_download_, ValidateDangerousDownload()).Times(0);

  handler_->RecordOpenSurveyOnDangerousInterstitial("1");
  handler_->SaveDangerousFromInterstitialNeedGesture(
      "1",
      downloads::mojom::DangerousDownloadInterstitialSurveyOptions::kTrustSite);
  histogram_tester_.ExpectBucketCount(
      "Download.DangerousDownloadInterstitial.Action",
      DangerousDownloadInterstitialAction::kSaveDangerous, 0);
  histogram_tester_.ExpectBucketCount(
      "Download.DangerousDownloadInterstitial.SurveyResponse",
      downloads::mojom::DangerousDownloadInterstitialSurveyOptions::kTrustSite,
      0);
}

class DownloadsDOMHandlerWithFakeSafeBrowsingTestTrustSafetySentimentService
    : public DownloadsDOMHandlerWithFakeSafeBrowsingTest {
 public:
  DownloadsDOMHandlerWithFakeSafeBrowsingTestTrustSafetySentimentService() =
      default;

  void ExpectTrustSafetySentimentServiceCall(
      DownloadItemWarningData::WarningSurface surface,
      DownloadItemWarningData::WarningAction action) {
    mock_sentiment_service_ = static_cast<MockTrustSafetySentimentService*>(
        TrustSafetySentimentServiceFactory::GetInstance()
            ->SetTestingFactoryAndUse(
                profile(),
                base::BindRepeating(&BuildMockTrustSafetySentimentService)));
    EXPECT_CALL(*mock_sentiment_service_,
                InteractedWithDownloadWarningUI(surface, action));
  }

 private:
  raw_ptr<MockTrustSafetySentimentService> mock_sentiment_service_;
};

TEST_F(DownloadsDOMHandlerWithFakeSafeBrowsingTestTrustSafetySentimentService,
       DiscardDangerous_CallsTrustSafetySentimentService) {
  profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingSurveysEnabled, true);
  SetUpDangerousDownload();
  ExpectTrustSafetySentimentServiceCall(
      DownloadItemWarningData::WarningSurface::DOWNLOADS_PAGE,
      DownloadItemWarningData::WarningAction::DISCARD);

  TestDownloadsDOMHandler handler(page_.BindAndGetRemote(), manager(),
                                  web_ui());

  EXPECT_CALL(dangerous_download_, Remove());
  handler.DiscardDangerous("1");
}

TEST_F(DownloadsDOMHandlerWithFakeSafeBrowsingTestTrustSafetySentimentService,
       SaveSuspicious_CallsTrustSafetySentimentService) {
  profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingSurveysEnabled, true);
  SetUpDangerousDownload();
  ExpectTrustSafetySentimentServiceCall(
      DownloadItemWarningData::WarningSurface::DOWNLOADS_PAGE,
      DownloadItemWarningData::WarningAction::PROCEED);

  TestDownloadsDOMHandler handler(page_.BindAndGetRemote(), manager(),
                                  web_ui());

  SimulateMouseGestureOnWebUI();

  EXPECT_CALL(dangerous_download_, ValidateDangerousDownload());
  handler.SaveSuspiciousRequiringGesture("1");
}

TEST_F(DownloadsDOMHandlerWithFakeSafeBrowsingTestTrustSafetySentimentService,
       SaveDangerousFromDialog_CallsTrustSafetySentimentService) {
  profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingSurveysEnabled, true);
  SetUpDangerousDownload();
  ExpectTrustSafetySentimentServiceCall(
      DownloadItemWarningData::WarningSurface::DOWNLOAD_PROMPT,
      DownloadItemWarningData::WarningAction::PROCEED);

  TestDownloadsDOMHandler handler(page_.BindAndGetRemote(), manager(),
                                  web_ui());

  SimulateMouseGestureOnWebUI();

  EXPECT_CALL(dangerous_download_, ValidateDangerousDownload());
  handler.SaveDangerousFromDialogRequiringGesture("1");
}

TEST_F(DownloadsDOMHandlerWithFakeSafeBrowsingTestTrustSafetySentimentService,
       SaveDangerousFromInterstitial_CallsTrustSafetySentimentService) {
  base::test::ScopedFeatureList feature_list_;
  feature_list_.InitAndEnableFeature(
      safe_browsing::kDangerousDownloadInterstitial);

  profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingSurveysEnabled, true);
  SetUpDangerousDownload();
  ExpectTrustSafetySentimentServiceCall(
      DownloadItemWarningData::WarningSurface::DOWNLOAD_PROMPT,
      DownloadItemWarningData::WarningAction::PROCEED);

  TestDownloadsDOMHandler handler(page_.BindAndGetRemote(), manager(),
                                  web_ui());

  SimulateMouseGestureOnWebUI();

  EXPECT_CALL(dangerous_download_, ValidateDangerousDownload());

  handler.RecordOpenBypassWarningInterstitial("1");
  handler.RecordOpenSurveyOnDangerousInterstitial("1");
  handler.SaveDangerousFromInterstitialNeedGesture(
      "1", downloads::mojom::DangerousDownloadInterstitialSurveyOptions::
               kCreatedFile);

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectBucketCount(
      "Download.DangerousDownloadInterstitial.SurveyResponse",
      downloads::mojom::DangerousDownloadInterstitialSurveyOptions::
          kCreatedFile,
      0);
}
