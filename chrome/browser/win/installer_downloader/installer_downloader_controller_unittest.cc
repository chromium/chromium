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
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/win/installer_downloader/installer_downloader_feature.h"
#include "chrome/browser/win/installer_downloader/installer_downloader_model.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::_;
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
};

class InstallerDownloaderControllerTest : public testing::Test {
 protected:
  InstallerDownloaderControllerTest() {
    auto mock_model_ptr =
        std::make_unique<StrictMock<MockInstallerDownloaderModel>>();
    // Keep a raw pointer to it for setting expectations and verifications.
    mock_model_ = mock_model_ptr.get();

    controller_ = std::make_unique<InstallerDownloaderController>(
        std::move(mock_model_ptr));
  }

  base::test::ScopedFeatureList feature_list_{kInstallerDownloader};

  std::unique_ptr<InstallerDownloaderController> controller_;
  raw_ptr<StrictMock<MockInstallerDownloaderModel>> mock_model_;
};

TEST_F(InstallerDownloaderControllerTest, NoCallOrCrashExpected) {
  controller_->MaybeShowInfoBar();
  controller_->OnDownloadRequestAccepted(/*web_contents=*/nullptr);
}

}  // namespace
}  // namespace installer_downloader
