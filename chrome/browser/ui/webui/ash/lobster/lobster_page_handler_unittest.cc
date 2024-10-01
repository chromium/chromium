// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/lobster/lobster_page_handler.h"

#include <optional>
#include <string_view>
#include <vector>

#include "ash/public/cpp/lobster/lobster_result.h"
#include "ash/public/cpp/lobster/lobster_session.h"
#include "base/base64.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/test_future.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/download/download_core_service_impl.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

constexpr std::string_view kRawBytes1 = "a1b2c3";
constexpr std::string_view kRawBytes2 = "d4e5f6";

class FakeLobsterSession : public LobsterSession {
 public:
  FakeLobsterSession(const LobsterResult& result,
                     bool commit_or_download_status,
                     bool feedback_submission_status)
      : result_(result),
        commit_or_download_status_(commit_or_download_status),
        feedback_submission_status_(feedback_submission_status) {}
  ~FakeLobsterSession() override = default;

  void DownloadCandidate(int candidate_id,
                         const base::FilePath& file_path,
                         StatusCallback callback) override {
    std::move(callback).Run(commit_or_download_status_);
  }
  void CommitAsInsert(int candidate_id, StatusCallback callback) override {
    std::move(callback).Run(commit_or_download_status_);
  }
  void CommitAsDownload(int candidate_id,
                        const base::FilePath& file_path,
                        StatusCallback callback) override {
    std::move(callback).Run(commit_or_download_status_);
  }
  void RequestCandidates(const std::string& query,
                         int num_candidates,
                         RequestCandidatesCallback callback) override {
    std::move(callback).Run(result_);
  }
  void PreviewFeedback(int candidate_id,
                       LobsterPreviewFeedbackCallback callback) override {}
  bool SubmitFeedback(int candidate_id,
                      const std::string& description) override {
    return feedback_submission_status_;
  }
  void LoadUI(std::optional<std::string> query) override {}
  void ShowUI() override {}
  void CloseUI() override {}

 private:
  LobsterResult result_;
  bool commit_or_download_status_;
  bool feedback_submission_status_;
};

class LobsterPageHandlerTest : public testing::Test {
 public:
  void SetUp() override {
    DownloadCoreServiceFactory::GetForBrowserContext(&profile_)
        ->SetDownloadManagerDelegateForTesting(
            std::make_unique<ChromeDownloadManagerDelegate>(&profile_));

    // Use a temporary directory for downloads.
    ASSERT_TRUE(download_dir_.CreateUniqueTempDir());
    DownloadPrefs* prefs =
        DownloadPrefs::FromDownloadManager(profile_.GetDownloadManager());
    prefs->SetDownloadPath(download_dir_.GetPath());
    prefs->SkipSanitizeDownloadTargetPathForTesting();
  }
  TestingProfile& profile() { return profile_; }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  base::ScopedTempDir download_dir_;

 private:
  TestingProfile profile_;
};

TEST_F(LobsterPageHandlerTest,
       RequestCandidatesReturnsImagesInCorrectJpegFormat) {
  std::vector<LobsterImageCandidate> image_candidates = {
      LobsterImageCandidate(/*id=*/0, /*image_bytes=*/kRawBytes1.data(),
                            /*seed=*/20,
                            /*query=*/"a nice strawberry"),
      LobsterImageCandidate(/*id=*/1, /*image_bytes=*/kRawBytes2.data(),
                            /*seed=*/21,
                            /*query=*/"a nice strawberry")};
  FakeLobsterSession session(std::move(image_candidates),
                             /*commit_or_download_status=*/true,
                             /*feedback_submission_status=*/true);
  LobsterPageHandler page_handler = LobsterPageHandler(&session, &profile());
  base::test::TestFuture<lobster::mojom::ResponsePtr> future;

  page_handler.RequestCandidates("a nice strawberry", 2, future.GetCallback());

  EXPECT_TRUE(future.Get()->is_candidates());

  auto& actual_candidates = future.Get()->get_candidates();

  EXPECT_EQ(actual_candidates.size(), 2u);
  EXPECT_EQ(actual_candidates[0]->id, 0u);
  EXPECT_EQ(actual_candidates[0]->data_url,
            GURL(base::StrCat(
                {"data:image/jpeg;base64,", base::Base64Encode(kRawBytes1)})));
  EXPECT_EQ(actual_candidates[1]->id, 1u);
  EXPECT_EQ(actual_candidates[1]->data_url,
            GURL(base::StrCat(
                {"data:image/jpeg;base64,", base::Base64Encode(kRawBytes2)})));
}

TEST_F(LobsterPageHandlerTest, RequestCandidatesReturnsError) {
  FakeLobsterSession session(
      base::unexpected(
          LobsterError(LobsterErrorCode::kInvalidArgument, "dummy error")),
      /*commit_or_download_status=*/false, /*feedback_submission_status=*/true);
  LobsterPageHandler page_handler = LobsterPageHandler(&session, &profile());
  base::test::TestFuture<lobster::mojom::ResponsePtr> future;

  page_handler.RequestCandidates("a nice strawberry", 2, future.GetCallback());

  EXPECT_TRUE(future.Get()->is_error());

  auto& actual_error = future.Get()->get_error();

  EXPECT_EQ(actual_error->code, LobsterErrorCode::kInvalidArgument);
  EXPECT_EQ(actual_error->message, "dummy error");
}

TEST_F(LobsterPageHandlerTest, DownloadCandidateSucceeds) {
  FakeLobsterSession session({}, /*commit_or_download_status=*/true,
                             /*feedback_submission_status=*/true);
  LobsterPageHandler page_handler = LobsterPageHandler(&session, &profile());
  base::test::TestFuture<bool> future;

  page_handler.DownloadCandidate(1, future.GetCallback());

  EXPECT_TRUE(future.Get());
}

TEST_F(LobsterPageHandlerTest, DownloadCandidateFails) {
  FakeLobsterSession session({}, /*commit_or_download_status=*/false,
                             /*feedback_submission_status=*/true);
  LobsterPageHandler page_handler = LobsterPageHandler(&session, &profile());
  base::test::TestFuture<bool> future;

  page_handler.DownloadCandidate(/*id=*/1, future.GetCallback());

  EXPECT_FALSE(future.Get());
}

TEST_F(LobsterPageHandlerTest, CommitAsDownloadSucceeds) {
  FakeLobsterSession session({}, /*commit_or_download_status=*/true,
                             /*feedback_submission_status=*/true);
  LobsterPageHandler page_handler = LobsterPageHandler(&session, &profile());
  base::test::TestFuture<bool> future;

  page_handler.CommitAsDownload(/*id=*/1, future.GetCallback());

  EXPECT_TRUE(future.Get());
}

TEST_F(LobsterPageHandlerTest, CommitAsDownloadFails) {
  FakeLobsterSession session({}, /*commit_or_download_status=*/false,
                             /*feedback_submission_status=*/true);
  LobsterPageHandler page_handler = LobsterPageHandler(&session, &profile());
  base::test::TestFuture<bool> future;

  page_handler.CommitAsDownload(/*id=*/1, future.GetCallback());

  EXPECT_FALSE(future.Get());
}

TEST_F(LobsterPageHandlerTest, CommitAsInsertSucceeds) {
  FakeLobsterSession session({}, /*commit_or_download_status=*/true,
                             /*feedback_submission_status=*/true);
  LobsterPageHandler page_handler = LobsterPageHandler(&session, &profile());
  base::test::TestFuture<bool> future;

  page_handler.CommitAsInsert(/*id=*/1, future.GetCallback());

  EXPECT_TRUE(future.Get());
}

TEST_F(LobsterPageHandlerTest, CommitAsInsertFails) {
  FakeLobsterSession session({}, /*commit_or_download_status=*/false,
                             /*feedback_submission_status=*/true);
  LobsterPageHandler page_handler = LobsterPageHandler(&session, &profile());
  base::test::TestFuture<bool> future;

  page_handler.CommitAsInsert(/*id=*/1, future.GetCallback());

  EXPECT_FALSE(future.Get());
}

TEST_F(LobsterPageHandlerTest, SubmitFeedbackFails) {
  FakeLobsterSession session({}, /*commit_or_download_status=*/false,
                             /*feedback_submission_status=*/false);
  LobsterPageHandler page_handler = LobsterPageHandler(&session, &profile());
  base::test::TestFuture<bool> future;

  page_handler.SubmitFeedback(/*id=*/1, /*description=*/"dummy description",
                              future.GetCallback());

  EXPECT_FALSE(future.Get());
}

TEST_F(LobsterPageHandlerTest, SubmitFeedbackSucceeds) {
  FakeLobsterSession session({}, /*commit_or_download_status=*/false,
                             /*feedback_submission_status=*/true);
  LobsterPageHandler page_handler = LobsterPageHandler(&session, &profile());
  base::test::TestFuture<bool> future;

  page_handler.SubmitFeedback(/*id=*/1, /*description=*/"dummy description",
                              future.GetCallback());

  EXPECT_TRUE(future.Get());
}

}  // namespace
}  // namespace ash
