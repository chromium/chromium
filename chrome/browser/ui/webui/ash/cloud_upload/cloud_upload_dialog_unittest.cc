// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_dialog.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::cloud_upload {

TEST(CloudUploadDialogTest,
     GetEmailFromAndroidOneDriveRootDoc_ConventionalEmail) {
  EXPECT_EQ(GetEmailFromAndroidOneDriveRootDoc("pivots%2Fuser@gmail.com"),
            "user@gmail.com");
}

TEST(CloudUploadDialogTest, GetEmailFromAndroidOneDriveRootDoc_NotEscaped) {
  EXPECT_EQ(GetEmailFromAndroidOneDriveRootDoc("pivots/user@gmail.com"),
            "user@gmail.com");
}

TEST(CloudUploadDialogTest, GetEmailFromAndroidOneDriveRootDoc_UnusualEmail) {
  EXPECT_EQ(GetEmailFromAndroidOneDriveRootDoc("pivots%2Fan_email"),
            "an_email");
}

TEST(CloudUploadDialogTest, GetEmailFromAndroidOneDriveRootDoc_CannotUnescape) {
  EXPECT_FALSE(
      GetEmailFromAndroidOneDriveRootDoc("pivots%2user@gmail.com").has_value());
}

TEST(CloudUploadDialogTest, GetEmailFromAndroidOneDriveRootDoc_EmailEscaped) {
  EXPECT_FALSE(GetEmailFromAndroidOneDriveRootDoc("pivots%2Fuser%2F@gmail.com")
                   .has_value());
}

TEST(CloudUploadDialogTest, GetEmailFromAndroidOneDriveRootDoc_NoPivots) {
  EXPECT_FALSE(GetEmailFromAndroidOneDriveRootDoc("something%2Fuser@gmail.com")
                   .has_value());
}

}  // namespace ash::cloud_upload
