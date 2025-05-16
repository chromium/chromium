// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/installer_downloader/installer_downloader_controller.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/win/installer_downloader/installer_downloader_model.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using base::test::RunOnceCallback;
using ::testing::_;
using ::testing::Return;
using ::testing::StrictMock;

namespace installer_downloader {
namespace {

class MockInstallerDownloaderModel : public InstallerDownloaderModel {
 public:
  MOCK_METHOD(void,
              StartDownload,
              (const GURL&, const base::FilePath&, CompletionCallback),
              (override));
  MOCK_METHOD(void,
              CheckEligibility,
              (base::OnceCallback<void(const std::optional<base::FilePath>&)>),
              (override));
  MOCK_METHOD(bool, IsMaxShowCountReached, (), (const, override));
};

class InstallerDownloaderControllerTest : public testing::Test {
 protected:
  InstallerDownloaderControllerTest() {
    web_contents_ = web_contents_factory_.CreateWebContents(&profile_);

    auto model = std::make_unique<StrictMock<MockInstallerDownloaderModel>>();
    mock_model_ = model.get();

    controller_ = std::make_unique<InstallerDownloaderController>(
        show_infobar_callback_.Get(), std::move(model));

    controller_->SetActiveWebContentsCallbackForTesting(
        base::BindLambdaForTesting(
            [&]() -> content::WebContents* { return web_contents_; }));
  }

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
};

TEST_F(InstallerDownloaderControllerTest, BailsWhenShowCountExceeded) {
  EXPECT_CALL(*mock_model_, IsMaxShowCountReached()).WillOnce(Return(true));

  controller_->MaybeShowInfoBar();
}

TEST_F(InstallerDownloaderControllerTest,
       CallsEligibilityWhenShowCountNotExceeded) {
  EXPECT_CALL(*mock_model_, IsMaxShowCountReached()).WillOnce(Return(false));
  EXPECT_CALL(*mock_model_, CheckEligibility(_))
      .WillOnce(RunOnceCallback<0>(std::nullopt));

  controller_->MaybeShowInfoBar();
}

// All conditions satisfied  →  coordinator::Show should run exactly once.
TEST_F(InstallerDownloaderControllerTest, ShowsInfobarWhenEligible) {
  EXPECT_CALL(*mock_model_, IsMaxShowCountReached()).WillOnce(Return(false));
  EXPECT_CALL(*mock_model_, CheckEligibility(_))
      .WillOnce(base::test::RunOnceCallback<0>(
          std::optional<base::FilePath>(base::FilePath(L"C:\\foo"))));
  EXPECT_CALL(show_infobar_callback_, Run(_, _)).Times(1);

  controller_->MaybeShowInfoBar();
}

// If there is no active WebContents, Show() must *not* be called.
TEST_F(InstallerDownloaderControllerTest, SkipsWhenNoActiveContents) {
  controller_->SetActiveWebContentsCallbackForTesting(
      base::BindLambdaForTesting(
          [&]() -> content::WebContents* { return nullptr; }));

  EXPECT_CALL(*mock_model_, IsMaxShowCountReached()).WillOnce(Return(false));
  EXPECT_CALL(*mock_model_, CheckEligibility(_))
      .WillOnce(base::test::RunOnceCallback<0>(
          std::optional<base::FilePath>(base::FilePath(L"C:\\foo"))));

  controller_->MaybeShowInfoBar();
}

// If the eligibility callback returns `std::nullopt`, no infobar is shown.
TEST_F(InstallerDownloaderControllerTest, SkipsWhenNotEligible) {
  EXPECT_CALL(*mock_model_, IsMaxShowCountReached()).WillOnce(Return(false));
  EXPECT_CALL(*mock_model_, CheckEligibility(_))
      .WillOnce(base::test::RunOnceCallback<0>(std::nullopt));

  controller_->MaybeShowInfoBar();
}

}  // namespace
}  // namespace installer_downloader
