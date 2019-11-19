// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/quarantine/quarantine.h"

#include <sys/xattr.h>

#import <ApplicationServices/ApplicationServices.h>
#import <Foundation/Foundation.h>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "components/services/quarantine/test_support.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "url/gurl.h"

namespace quarantine {
namespace {

class QuarantineMacTest : public testing::Test {
 public:
  QuarantineMacTest()
      : source_url_("http://www.source.example.com"),
        referrer_url_("http://www.referrer.example.com") {}

 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(
        base::CreateTemporaryFileInDir(temp_dir_.GetPath(), &test_file_));
    file_url_.reset([[NSURL alloc]
        initFileURLWithPath:base::SysUTF8ToNSString(test_file_.value())]);

    base::scoped_nsobject<NSMutableDictionary> properties(
        [[NSMutableDictionary alloc] init]);
    [properties
        setValue:@"com.google.Chrome"
          forKey:static_cast<NSString*>(kLSQuarantineAgentBundleIdentifierKey)];
    [properties setValue:@"Google Chrome.app"
                  forKey:static_cast<NSString*>(kLSQuarantineAgentNameKey)];
    [properties setValue:@(1) forKey:@"kLSQuarantineIsOwnedByCurrentUserKey"];
    bool success = [file_url_ setResourceValue:properties
                                        forKey:NSURLQuarantinePropertiesKey
                                         error:nullptr];
    ASSERT_TRUE(success);
  }

  base::ScopedTempDir temp_dir_;
  base::FilePath test_file_;
  const GURL source_url_;
  const GURL referrer_url_;
  base::scoped_nsobject<NSURL> file_url_;
};

TEST_F(QuarantineMacTest, CheckMetadataSetCorrectly) {
  EXPECT_EQ(QuarantineFileResult::OK,
            QuarantineFile(test_file_, source_url_, referrer_url_, ""));
  EXPECT_TRUE(IsFileQuarantined(test_file_, source_url_, referrer_url_));
}

TEST_F(QuarantineMacTest, SetMetadataMultipleTimes) {
  GURL dummy_url("http://www.dummy.example.com");
  EXPECT_EQ(QuarantineFileResult::OK,
            QuarantineFile(test_file_, source_url_, referrer_url_, ""));
  EXPECT_EQ(QuarantineFileResult::OK,
            QuarantineFile(test_file_, dummy_url, dummy_url, ""));
  EXPECT_TRUE(IsFileQuarantined(test_file_, source_url_, referrer_url_));
}

TEST_F(QuarantineMacTest, IsFileQuarantined_NoFile) {
  base::FilePath does_not_exist = temp_dir_.GetPath().AppendASCII("a.jar");
  EXPECT_FALSE(IsFileQuarantined(does_not_exist, GURL(), GURL()));
}

TEST_F(QuarantineMacTest, IsFileQuarantined_NoAnnotationsOnFile) {
  EXPECT_FALSE(IsFileQuarantined(test_file_, GURL(), GURL()));
}

TEST_F(QuarantineMacTest, IsFileQuarantined_SourceUrlOnly) {
  ASSERT_EQ(QuarantineFileResult::OK,
            QuarantineFile(test_file_, source_url_, GURL(), std::string()));
  EXPECT_TRUE(IsFileQuarantined(test_file_, source_url_, GURL()));
  EXPECT_TRUE(IsFileQuarantined(test_file_, GURL(), GURL()));
  EXPECT_TRUE(IsFileQuarantined(test_file_, GURL(), referrer_url_));
  EXPECT_FALSE(IsFileQuarantined(test_file_, referrer_url_, GURL()));
}

TEST_F(QuarantineMacTest, IsFileQuarantined_FullMetadata) {
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

TEST_F(QuarantineMacTest, IsFileQuarantined_Sanitize) {
  GURL host_url{"https://user:pass@example.com/foo/bar?x#y"};
  GURL host_url_clean{"https://example.com/foo/bar?x#y"};
  GURL referrer_url{"https://user:pass@example.com/foo/index?x#y"};
  GURL referrer_url_clean{"https://example.com/foo/index?x#y"};

  ASSERT_EQ(QuarantineFileResult::OK,
            QuarantineFile(test_file_, host_url, referrer_url, std::string()));
  EXPECT_TRUE(
      IsFileQuarantined(test_file_, host_url_clean, referrer_url_clean));
}

TEST_F(QuarantineMacTest, NoWhereFromsKeyIfNoURLs) {
  ASSERT_EQ(QuarantineFileResult::OK,
            QuarantineFile(test_file_, GURL(), GURL(), std::string()));

  NSString* file_path = base::mac::FilePathToNSString(test_file_);
  ASSERT_NE(nullptr, file_path);
  base::ScopedCFTypeRef<MDItemRef> md_item(
      MDItemCreate(kCFAllocatorDefault, base::mac::NSToCFCast(file_path)));
  if (!md_item) {
    // The quarantine code ignores it if adding origin metadata fails. If for
    // some reason MDItemCreate fails (which it seems to do on the bots, not
    // sure why) just stop on the test.
    return;
  }

  base::ScopedCFTypeRef<CFTypeRef> attr(
      MDItemCopyAttribute(md_item, kMDItemWhereFroms));
  EXPECT_FALSE(attr);
}

}  // namespace
}  // namespace quarantine
