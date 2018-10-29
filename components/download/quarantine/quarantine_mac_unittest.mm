// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/quarantine/quarantine.h"

#include <sys/xattr.h>

#import <Foundation/Foundation.h>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/mac/availability.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "components/download/quarantine/test_support.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "url/gurl.h"

namespace download {
namespace {

class QuarantineMacTest : public testing::Test {
 public:
  QuarantineMacTest()
      : source_url_("http://www.source.example.com"),
        referrer_url_("http://www.referrer.example.com") {}

 protected:
  void SetUp() override {
    if (@available(macos 10.10, *)) {
      ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
      ASSERT_TRUE(
          base::CreateTemporaryFileInDir(temp_dir_.GetPath(), &test_file_));
      file_url_.reset([[NSURL alloc]
          initFileURLWithPath:base::SysUTF8ToNSString(test_file_.value())]);

      base::scoped_nsobject<NSMutableDictionary> properties(
          [[NSMutableDictionary alloc] init]);
      [properties setValue:@"com.google.Chrome"
                    forKey:static_cast<NSString*>(
                               kLSQuarantineAgentBundleIdentifierKey)];
      [properties setValue:@"Google Chrome.app"
                    forKey:static_cast<NSString*>(kLSQuarantineAgentNameKey)];
      [properties setValue:@(1) forKey:@"kLSQuarantineIsOwnedByCurrentUserKey"];
      bool success = [file_url_ setResourceValue:properties
                                          forKey:NSURLQuarantinePropertiesKey
                                           error:nullptr];
      ASSERT_TRUE(success);
    } else {
      LOG(WARNING) << "Test suite requires Mac OS X 10.10 or later";
    }
  }

  base::ScopedTempDir temp_dir_;
  base::FilePath test_file_;
  GURL source_url_;
  GURL referrer_url_;
  base::scoped_nsobject<NSURL> file_url_;
};

TEST_F(QuarantineMacTest, CheckMetadataSetCorrectly) {
  if (base::mac::IsAtMostOS10_9())
    return;
  EXPECT_EQ(QuarantineFileResult::OK,
            QuarantineFile(test_file_, source_url_, referrer_url_, ""));
  EXPECT_TRUE(IsFileQuarantined(test_file_, source_url_, referrer_url_));
}

TEST_F(QuarantineMacTest, SetMetadataMultipleTimes) {
  if (base::mac::IsAtMostOS10_9())
    return;
  GURL dummy_url("http://www.dummy.example.com");
  EXPECT_EQ(QuarantineFileResult::OK,
            QuarantineFile(test_file_, source_url_, referrer_url_, ""));
  EXPECT_EQ(QuarantineFileResult::OK,
            QuarantineFile(test_file_, dummy_url, dummy_url, ""));
  EXPECT_TRUE(IsFileQuarantined(test_file_, source_url_, referrer_url_));
}

TEST_F(QuarantineMacTest, IsFileQuarantined_NoFile) {
  if (base::mac::IsAtMostOS10_9())
    return;
  base::FilePath does_not_exist = temp_dir_.GetPath().AppendASCII("a.jar");
  EXPECT_FALSE(IsFileQuarantined(does_not_exist, GURL(), GURL()));
}

TEST_F(QuarantineMacTest, IsFileQuarantined_NoAnnotationsOnFile) {
  if (base::mac::IsAtMostOS10_9())
    return;
  EXPECT_FALSE(IsFileQuarantined(test_file_, GURL(), GURL()));
}

TEST_F(QuarantineMacTest, IsFileQuarantined_SourceUrlOnly) {
  if (base::mac::IsAtMostOS10_9())
    return;
  ASSERT_EQ(QuarantineFileResult::OK,
            QuarantineFile(test_file_, source_url_, GURL(), std::string()));
  EXPECT_TRUE(IsFileQuarantined(test_file_, source_url_, GURL()));
  EXPECT_TRUE(IsFileQuarantined(test_file_, GURL(), GURL()));
  EXPECT_TRUE(IsFileQuarantined(test_file_, GURL(), referrer_url_));
  EXPECT_FALSE(IsFileQuarantined(test_file_, referrer_url_, GURL()));
}

TEST_F(QuarantineMacTest, IsFileQuarantined_FullMetadata) {
  if (base::mac::IsAtMostOS10_9())
    return;
  ASSERT_EQ(
      QuarantineFileResult::OK,
      QuarantineFile(test_file_, source_url_, referrer_url_, std::string()));
  EXPECT_TRUE(IsFileQuarantined(test_file_, GURL(), GURL()));
  EXPECT_TRUE(IsFileQuarantined(test_file_, source_url_, GURL()));
  EXPECT_TRUE(IsFileQuarantined(test_file_, source_url_, referrer_url_));
  EXPECT_TRUE(IsFileQuarantined(test_file_, GURL(), referrer_url_));
  EXPECT_FALSE(IsFileQuarantined(test_file_, source_url_, source_url_));
  EXPECT_FALSE(IsFileQuarantined(test_file_, referrer_url_, referrer_url_));
}

}  // namespace
}  // namespace downlod
