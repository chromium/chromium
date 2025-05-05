// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/installer_downloader/installer_downloader_model_impl.h"

#include "chrome/browser/win/installer_downloader/installer_downloader_pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace installer_downloader {
namespace {

class InstallerDownloaderModelTest : public testing::Test {
 protected:
  InstallerDownloaderModelTest() = default;

  TestingPrefServiceSimple& GetLocalState() { return *local_state_.Get(); }

  ScopedTestingLocalState local_state_{TestingBrowserProcess::GetGlobal()};
  InstallerDownloaderModelImpl model_;
};

TEST_F(InstallerDownloaderModelTest, MaxShowCountNotExceeded) {
  GetLocalState().SetInteger(prefs::kInstallerDownloaderInfobarShowCount,
                             InstallerDownloaderModelImpl::kMaxShowCount - 1);
  EXPECT_FALSE(model_.IsMaxShowCountReached());
}

TEST_F(InstallerDownloaderModelTest, MaxShowCountExactlyAtLimit) {
  GetLocalState().SetInteger(prefs::kInstallerDownloaderInfobarShowCount,
                             InstallerDownloaderModelImpl::kMaxShowCount);
  EXPECT_TRUE(model_.IsMaxShowCountReached());
}

TEST_F(InstallerDownloaderModelTest, MaxShowCountAboveLimit) {
  GetLocalState().SetInteger(prefs::kInstallerDownloaderInfobarShowCount,
                             InstallerDownloaderModelImpl::kMaxShowCount + 1);
  EXPECT_TRUE(model_.IsMaxShowCountReached());
}

}  // namespace
}  // namespace installer_downloader
