// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/installer_downloader/installer_downloader_controller.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/base_paths.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_path_override.h"
#include "chrome/browser/win/installer_downloader/installer_downloader_feature.h"
#include "chrome/browser/win/installer_downloader/installer_downloader_model.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_download_manager.h"
#include "content/public/test/test_web_contents_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using base::test::RunOnceCallback;
using ::testing::_;
using ::testing::AllOf;
using ::testing::ContainsRegex;
using ::testing::HasSubstr;
using ::testing::Invoke;
using ::testing::MatchesRegex;
using ::testing::Not;
using ::testing::Property;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StrictMock;

namespace installer_downloader {
namespace {

// A simple, valid template: IIDGUID and STATS are placeholders that the
// production code will substitute.
constexpr char kUrlTemplate[] =
    "https://example.com/installer.exe?iid=IIDGUID&stats=STATS&lang=LANGUAGE";

class MockInstallerDownloaderModel : public InstallerDownloaderModel {
 public:
  MOCK_METHOD(void,
              StartDownload,
              (const GURL&,
               const base::FilePath&,
               content::DownloadManager&,
               CompletionCallback),
              (override));
  MOCK_METHOD(void, CheckEligibility, (EligibilityCheckCallback), (override));
  MOCK_METHOD(bool, CanShowInfobar, (), (const, override));
  MOCK_METHOD(void, IncrementShowCount, (), (override));
  MOCK_METHOD(void, PreventFutureDisplay, (), (override));
  MOCK_METHOD(bool, ShouldByPassEligibilityCheck, (), (const, override));
};

class InstallerDownloaderControllerTest : public testing::Test {
 protected:
  InstallerDownloaderControllerTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        kInstallerDownloader,
        {{kInstallerUrlTemplateParam.name, kUrlTemplate}});

    auto download_manager = std::make_unique<content::MockDownloadManager>();
    mock_download_manager_ = download_manager.get();
    profile_.SetDownloadManagerForTesting(std::move(download_manager));

    web_contents_ = web_contents_factory_.CreateWebContents(&profile_);

    auto model = std::make_unique<StrictMock<MockInstallerDownloaderModel>>();
    mock_model_ = model.get();

    controller_ = std::make_unique<InstallerDownloaderController>(
        show_infobar_callback_.Get(), is_metric_enabled_mock_callback_.Get(),
        std::move(model));

    controller_->SetActiveWebContentsCallbackForTesting(
        base::BindLambdaForTesting(
            [&]() -> content::WebContents* { return web_contents_; }));
  }

  base::test::ScopedFeatureList feature_list_;

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;

  StrictMock<
      base::MockCallback<InstallerDownloaderController::ShowInfobarCallback>>
      show_infobar_callback_;
  content::TestWebContentsFactory web_contents_factory_;
  // Owned by `web_contents_factory_`.
  raw_ptr<content::WebContents> web_contents_;
  std::unique_ptr<InstallerDownloaderController> controller_;
  raw_ptr<MockInstallerDownloaderModel> mock_model_;
  raw_ptr<content::MockDownloadManager> mock_download_manager_;
  StrictMock<base::MockCallback<base::RepeatingCallback<bool()>>>
      is_metric_enabled_mock_callback_;
};

TEST_F(InstallerDownloaderControllerTest, BailsWhenInfobarCannotShow) {
  EXPECT_CALL(*mock_model_, CanShowInfobar()).WillOnce(Return(false));

  controller_->MaybeShowInfoBar();
}

TEST_F(InstallerDownloaderControllerTest, CallsEligibilityWhenInfobarCanShow) {
  EXPECT_CALL(*mock_model_, CanShowInfobar()).WillOnce(Return(true));
  EXPECT_CALL(*mock_model_, ShouldByPassEligibilityCheck())
      .WillOnce(Return(false));
  EXPECT_CALL(*mock_model_, CheckEligibility(_))
      .WillOnce(RunOnceCallback<0>(std::nullopt));

  controller_->MaybeShowInfoBar();
}

// All conditions satisfied  →  coordinator::Show should run exactly once.
TEST_F(InstallerDownloaderControllerTest, ShowsInfobarWhenEligible) {
  EXPECT_CALL(*mock_model_, CanShowInfobar()).WillOnce(Return(true));
  EXPECT_CALL(*mock_model_, CheckEligibility(_))
      .WillOnce(base::test::RunOnceCallback<0>(
          std::optional<base::FilePath>(FILE_PATH_LITERAL("C:\\foo"))));
  EXPECT_CALL(show_infobar_callback_, Run(_, _, _))
      .WillOnce(Return(reinterpret_cast<infobars::InfoBar*>(0x1)));
  EXPECT_CALL(*mock_model_, IncrementShowCount()).Times(1);

  controller_->MaybeShowInfoBar();
}

// If there is no active WebContents, Show() must *not* be called.
TEST_F(InstallerDownloaderControllerTest, SkipsWhenNoActiveContents) {
  controller_->SetActiveWebContentsCallbackForTesting(
      base::BindLambdaForTesting(
          [&]() -> content::WebContents* { return nullptr; }));

  EXPECT_CALL(*mock_model_, CanShowInfobar()).WillOnce(Return(true));
  EXPECT_CALL(*mock_model_, CheckEligibility(_))
      .WillOnce(base::test::RunOnceCallback<0>(
          std::optional<base::FilePath>(FILE_PATH_LITERAL("C:\\foo"))));

  controller_->MaybeShowInfoBar();
}

// If the eligibility callback returns `std::nullopt`, no infobar is shown.
TEST_F(InstallerDownloaderControllerTest, SkipsWhenNotEligible) {
  EXPECT_CALL(*mock_model_, CanShowInfobar()).WillOnce(Return(true));
  EXPECT_CALL(*mock_model_, ShouldByPassEligibilityCheck())
      .WillOnce(Return(false));
  EXPECT_CALL(*mock_model_, CheckEligibility(_))
      .WillOnce(base::test::RunOnceCallback<0>(std::nullopt));

  controller_->MaybeShowInfoBar();
}

TEST_F(InstallerDownloaderControllerTest,
       DownloadUrlHasValidGuidAndNoPlaceholders) {
  EXPECT_CALL(is_metric_enabled_mock_callback_, Run()).WillOnce(Return(true));

  const base::FilePath destination(FILE_PATH_LITERAL("C:\\tmp"));
  EXPECT_CALL(
      *mock_model_,
      StartDownload(
          Property(
              &GURL::spec,
              AllOf(
                  // GUID in the iid= query param.
                  ContainsRegex(
                      "iid=[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-"
                      "[0-9a-f]{12}"),
                  // Metrics flag.
                  HasSubstr("&stats=1"),
                  // Language substitution.
                  HasSubstr("&lang=en"),
                  // No leftover placeholders.
                  Not(HasSubstr("IIDGUID")), Not(HasSubstr("STATS")),
                  Not(HasSubstr("LANGUAGE")))),
          destination.AppendASCII(kDownloadedInstallerFileName.Get()), _, _));

  controller_->OnDownloadRequestAccepted(destination);
}

TEST_F(InstallerDownloaderControllerTest, DownloadUrlStatsEnabled) {
  EXPECT_CALL(is_metric_enabled_mock_callback_, Run()).WillOnce(Return(true));

  const base::FilePath destination(FILE_PATH_LITERAL("C:\\tmp"));
  EXPECT_CALL(
      *mock_model_,
      StartDownload(Property(&GURL::spec, HasSubstr("&stats=1")),
                    destination.AppendASCII(kDownloadedInstallerFileName.Get()),
                    _, _));

  controller_->OnDownloadRequestAccepted(destination);
}

TEST_F(InstallerDownloaderControllerTest, DownloadUrlStatsDisabled) {
  EXPECT_CALL(is_metric_enabled_mock_callback_, Run()).WillOnce(Return(false));

  const base::FilePath destination(FILE_PATH_LITERAL("C:\\tmp"));
  EXPECT_CALL(
      *mock_model_,
      StartDownload(Property(&GURL::spec, HasSubstr("&stats=0")),
                    destination.AppendASCII(kDownloadedInstallerFileName.Get()),
                    _, _));

  controller_->OnDownloadRequestAccepted(destination);
}

TEST_F(InstallerDownloaderControllerTest, DownloadUrlLanguageSubstitution) {
  EXPECT_CALL(is_metric_enabled_mock_callback_, Run()).WillOnce(Return(true));

  const base::FilePath destination(FILE_PATH_LITERAL("C:\\tmp"));
  EXPECT_CALL(
      *mock_model_,
      StartDownload(
          Property(&GURL::spec,
                   AllOf(HasSubstr("&lang=en"), Not(HasSubstr("LANGUAGE")))),
          destination.AppendASCII(kDownloadedInstallerFileName.Get()), _, _));

  controller_->OnDownloadRequestAccepted(destination);
}

TEST_F(InstallerDownloaderControllerTest,
       DownloadGeneratesDifferentGuidEachTime) {
  EXPECT_CALL(is_metric_enabled_mock_callback_, Run())
      .WillRepeatedly(Return(true));

  const base::FilePath destination(FILE_PATH_LITERAL("C:\\tmp"));
  const base::FilePath full_destination =
      destination.AppendASCII(kDownloadedInstallerFileName.Get());
  GURL first_url;
  GURL second_url;

  {
    ::testing::Sequence s;
    EXPECT_CALL(*mock_model_, StartDownload(_, full_destination, _, _))
        .InSequence(s)
        .WillOnce(SaveArg<0>(&first_url));

    EXPECT_CALL(*mock_model_, StartDownload(_, full_destination, _, _))
        .InSequence(s)
        .WillOnce(SaveArg<0>(&second_url));
  }

  controller_->OnDownloadRequestAccepted(destination);
  controller_->OnDownloadRequestAccepted(destination);

  EXPECT_NE(first_url, second_url);
}

// Bypass = true, eligibility callback returns std::nullopt → infobar shown.
TEST_F(InstallerDownloaderControllerTest,
       ShowsInfobarWhenBypassEnabledAndNoDestination) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  // Point DIR_USER_DESKTOP to a real, absolute temp location so that the
  // fallback path logic succeeds.
  base::ScopedPathOverride desktop_override(base::DIR_USER_DESKTOP,
                                            temp_dir.GetPath(),
                                            /*is_absolute=*/true,
                                            /*create=*/true);

  EXPECT_CALL(*mock_model_, CanShowInfobar()).WillOnce(Return(true));
  EXPECT_CALL(*mock_model_, ShouldByPassEligibilityCheck())
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_model_, IncrementShowCount()).Times(1);
  // Eligibility returns std::nullopt → controller must rely on the bypass path.
  EXPECT_CALL(*mock_model_, CheckEligibility(_))
      .WillOnce(RunOnceCallback<0>(std::nullopt));

  // When bypass is enabled we still expect the infobar to be shown.
  EXPECT_CALL(show_infobar_callback_, Run(_, _, _))
      .WillOnce(Return(reinterpret_cast<infobars::InfoBar*>(0x1)));

  controller_->MaybeShowInfoBar();
}

TEST_F(InstallerDownloaderControllerTest, IncrementOnlyOncePerShow) {
  EXPECT_CALL(*mock_model_, CanShowInfobar()).WillOnce(Return(true));
  EXPECT_CALL(*mock_model_, CheckEligibility(_))
      .WillOnce(base::test::RunOnceCallback<0>(
          std::optional<base::FilePath>(base::FilePath(L"C:\\foo"))));
  EXPECT_CALL(*mock_model_, IncrementShowCount()).Times(1);

  EXPECT_CALL(show_infobar_callback_, Run(_, _, _))
      .WillOnce(Return(reinterpret_cast<infobars::InfoBar*>(0x1)));

  controller_->MaybeShowInfoBar();
}

TEST_F(InstallerDownloaderControllerTest, InfobarShownLoggedOncePerSession) {
  base::HistogramTester histograms;

  EXPECT_CALL(*mock_model_, CanShowInfobar()).WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_model_, CheckEligibility(_))
      .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<0>(
          std::optional<base::FilePath>(base::FilePath(L"C:\\foo"))));
  EXPECT_CALL(*mock_model_, IncrementShowCount()).Times(1);

  // Stub Show() so the controller thinks the infobar has been created.
  EXPECT_CALL(show_infobar_callback_, Run(_, _, _))
      .Times(1)
      .WillOnce(Return(reinterpret_cast<infobars::InfoBar*>(0x1)));

  // First display: should log.
  controller_->MaybeShowInfoBar();
  // Second display in the same session: should NOT log again.
  controller_->MaybeShowInfoBar();

  histograms.ExpectUniqueSample("Windows.InstallerDownloader.InfobarShown",
                                /*true=*/1, /*expected_count=*/1);
}

TEST_F(InstallerDownloaderControllerTest, RequestAcceptedTrueMetric) {
  base::HistogramTester histograms;

  EXPECT_CALL(is_metric_enabled_mock_callback_, Run()).WillOnce(Return(true));
  EXPECT_CALL(*mock_model_, StartDownload(_, _, _, _)).Times(1);

  controller_->OnDownloadRequestAccepted(
      base::FilePath(FILE_PATH_LITERAL("C:\\tmp"))
          .AppendASCII(kDownloadedInstallerFileName.Get()));

  histograms.ExpectUniqueSample("Windows.InstallerDownloader.RequestAccepted",
                                /*true=*/1, /*expected_count=*/1);
}

TEST_F(InstallerDownloaderControllerTest, RequestAcceptedFalseMetric) {
  base::HistogramTester histograms;

  EXPECT_CALL(*mock_model_, PreventFutureDisplay()).Times(1);

  controller_->OnInfoBarDismissed();

  histograms.ExpectUniqueSample("Windows.InstallerDownloader.RequestAccepted",
                                /*false=*/0, /*expected_count=*/1);
}

TEST_F(InstallerDownloaderControllerTest, LogsDownloadResultMetric) {
  base::HistogramTester histograms;

  EXPECT_CALL(is_metric_enabled_mock_callback_, Run()).WillOnce(Return(true));

  CompletionCallback download_completion_callback;

  EXPECT_CALL(*mock_model_, StartDownload(_, _, _, _))
      .WillOnce([&](const GURL&, const base::FilePath&,
                    content::DownloadManager&, CompletionCallback callback) {
        download_completion_callback = std::move(callback);
      });
  EXPECT_CALL(*mock_model_, PreventFutureDisplay()).Times(1);

  controller_->OnDownloadRequestAccepted(
      base::FilePath(FILE_PATH_LITERAL("C:\\tmp"))
          .AppendASCII(kDownloadedInstallerFileName.Get()));

  ASSERT_TRUE(download_completion_callback);
  std::move(download_completion_callback).Run(/*success=*/true);

  histograms.ExpectUniqueSample("Windows.InstallerDownloader.DownloadSucceed",
                                /*success=*/1, /*expected_count=*/1);
}

TEST_F(InstallerDownloaderControllerTest,
       PreventFutureDisplayCalledOnInfoBarDismissed) {
  EXPECT_CALL(*mock_model_, PreventFutureDisplay()).Times(1);

  controller_->OnInfoBarDismissed();
}

TEST_F(InstallerDownloaderControllerTest,
       PreventFutureDisplayCalledOnDownloadCompleted) {
  EXPECT_CALL(is_metric_enabled_mock_callback_, Run()).WillOnce(Return(true));

  CompletionCallback completion_callback;
  EXPECT_CALL(*mock_model_, StartDownload(_, _, _, _))
      .WillOnce(
          [&](const GURL&, const base::FilePath&, content::DownloadManager&,
              CompletionCallback cb) { completion_callback = std::move(cb); });

  controller_->OnDownloadRequestAccepted(
      base::FilePath(FILE_PATH_LITERAL("C:\\tmp"))
          .AppendASCII(kDownloadedInstallerFileName.Get()));

  ASSERT_TRUE(completion_callback);

  EXPECT_CALL(*mock_model_, PreventFutureDisplay()).Times(1);
  std::move(completion_callback).Run(/*success=*/true);
}

}  // namespace
}  // namespace installer_downloader
