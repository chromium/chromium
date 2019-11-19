// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/common/file_icon_util.h"

#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/file_manager/grit/file_manager_resources.h"

namespace app_list {

class AppListFileIconUtilTest
    : public testing::TestWithParam<std::pair<std::string, int>> {};
INSTANTIATE_TEST_SUITE_P(
    ,
    AppListFileIconUtilTest,
    testing::ValuesIn((std::pair<std::string, int>[]){
        {"/my/test/file.pdf", IDR_FILE_MANAGER_IMG_LAUNCHER_FILETYPE_2X_PDF},
        {"/my/test/file.Pdf", IDR_FILE_MANAGER_IMG_LAUNCHER_FILETYPE_2X_PDF},
        {"/my/test/file.tar.gz",
         IDR_FILE_MANAGER_IMG_LAUNCHER_FILETYPE_2X_ARCHIVE},
        {"/my/test/.gslides",
         IDR_FILE_MANAGER_IMG_LAUNCHER_FILETYPE_2X_GSLIDES},
        {"/my/test/noextension",
         IDR_FILE_MANAGER_IMG_LAUNCHER_FILETYPE_2X_GENERIC},
        {"/my/test/file.missing",
         IDR_FILE_MANAGER_IMG_LAUNCHER_FILETYPE_2X_GENERIC}}));

TEST_P(AppListFileIconUtilTest, IconResourceIdForFilepath) {
  const auto& arg = GetParam();
  EXPECT_EQ(::app_list::internal::GetIconResourceIdForLocalFilePath(
                base::FilePath(arg.first)),
            arg.second);
}

}  // namespace app_list
