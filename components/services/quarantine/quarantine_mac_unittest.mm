// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/quarantine/quarantine.h"

#import <ApplicationServices/ApplicationServices.h>
#import <Foundation/Foundation.h>

#include "base/apple/bridging.h"
#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/task_environment.h"
#include "components/services/quarantine/common_mac.h"
#include "components/services/quarantine/test_support.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "url/gurl.h"

namespace quarantine {
namespace {

void CheckQuarantineResult(base::OnceClosure quit_closure,
                           QuarantineFileResult result,
                           QuarantineFileResult expected_result) {
  EXPECT_EQ(expected_result, result);
  std::move(quit_closure).Run();
}

class QuarantineMacTest : public testing::Test {
 public:
  // The trailing / is intentional and needed to match the
  // SanitizeUrlForQuarantine() output.
  QuarantineMacTest()
      : source_url_("http://www.source.example.com/"),
        referrer_url_("http://www.referrer.example.com/") {}

 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(
        base::CreateTemporaryFileInDir(temp_dir_.GetPath(), &test_file_));
    file_url_ = base::apple::FilePathToNSURL(test_file_);

    NSDictionary* properties = @{
      static_cast<NSString*>(kLSQuarantineAgentBundleIdentifierKey) :
          @"com.google.Chrome",
      static_cast<NSString*>(kLSQuarantineAgentNameKey) : @"Google Chrome.app",
      @"kLSQuarantineIsOwnedByCurrentUserKey" : @(1)
    };

    NSError* error = nullptr;
    bool success = [file_url_ setResourceValue:properties
                                        forKey:NSURLQuarantinePropertiesKey
                                         error:&error];
    ASSERT_TRUE(success) << error.localizedDescription;
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  base::FilePath test_file_;
  const GURL source_url_;
  const GURL referrer_url_;
  __strong NSURL* file_url_;
};

TEST_F(QuarantineMacTest, CheckMetadataSetCorrectly) {
  base::RunLoop run_loop;
  QuarantineFile(test_file_, source_url_, referrer_url_,
                 /*request_initiator=*/std::nullopt, "",
                 base::BindOnce(&CheckQuarantineResult, run_loop.QuitClosure(),
                                QuarantineFileResult::OK));
  run_loop.Run();
  EXPECT_TRUE(IsFileQuarantined(test_file_, source_url_, referrer_url_));
}

TEST_F(QuarantineMacTest, SetMetadataMultipleTimes) {
  {
    base::RunLoop run_loop;
    QuarantineFile(
        test_file_, source_url_, referrer_url_,
        /*request_initiator=*/std::nullopt, "",
        base::BindOnce(&CheckQuarantineResult, run_loop.QuitClosure(),
                       QuarantineFileResult::OK));
    run_loop.Run();
  }

  {
    base::RunLoop run_loop;
    GURL dummy_url("http://www.dummy.example.com");
    QuarantineFile(
        test_file_, dummy_url, dummy_url,
        /*request_initiator=*/std::nullopt, "",
        base::BindOnce(&CheckQuarantineResult, run_loop.QuitClosure(),
                       QuarantineFileResult::OK));
    run_loop.Run();
    EXPECT_TRUE(IsFileQuarantined(test_file_, source_url_, referrer_url_));
  }
}

TEST_F(QuarantineMacTest, IsFileQuarantined_NoFile) {
  base::FilePath does_not_exist = temp_dir_.GetPath().AppendASCII("a.jar");
  EXPECT_FALSE(IsFileQuarantined(does_not_exist, GURL(), GURL()));
}

TEST_F(QuarantineMacTest, IsFileQuarantined_NoAnnotationsOnFile) {
  ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_dir_.GetPath(), &test_file_));
  EXPECT_FALSE(IsFileQuarantined(test_file_, GURL(), GURL()));
}

TEST_F(QuarantineMacTest, IsFileQuarantined_SourceUrlOnly) {
  base::RunLoop run_loop;
  QuarantineFile(test_file_, source_url_, GURL(),
                 /*request_initiator=*/std::nullopt, std::string(),
                 base::BindOnce(&CheckQuarantineResult, run_loop.QuitClosure(),
                                QuarantineFileResult::OK));
  run_loop.Run();
  EXPECT_TRUE(IsFileQuarantined(test_file_, source_url_, GURL()));
  EXPECT_TRUE(IsFileQuarantined(test_file_, GURL(), GURL()));
  EXPECT_TRUE(IsFileQuarantined(test_file_, GURL(), referrer_url_));
}

TEST_F(QuarantineMacTest, IsFileQuarantined_FullMetadata) {
  base::RunLoop run_loop;
  QuarantineFile(test_file_, source_url_, referrer_url_,
                 /*request_initiator=*/std::nullopt, std::string(),
                 base::BindOnce(&CheckQuarantineResult, run_loop.QuitClosure(),
                                QuarantineFileResult::OK));
  run_loop.Run();
  EXPECT_TRUE(IsFileQuarantined(test_file_, GURL(), GURL()));
  EXPECT_TRUE(IsFileQuarantined(test_file_, source_url_, GURL()));
  EXPECT_TRUE(IsFileQuarantined(test_file_, source_url_, referrer_url_));
  EXPECT_TRUE(IsFileQuarantined(test_file_, GURL(), referrer_url_));
}

TEST_F(QuarantineMacTest, IsFileQuarantined_Sanitize) {
  base::RunLoop run_loop;
  GURL host_url{"https://user:pass@example.com/foo/bar?x#y"};
  GURL host_url_clean{"https://example.com/foo/bar?x#y"};
  GURL referrer_url{"https://user:pass@example.com/foo/index?x#y"};
  GURL referrer_url_clean{"https://example.com/foo/index?x#y"};

  QuarantineFile(test_file_, host_url, referrer_url,
                 /*request_initiator=*/std::nullopt, std::string(),
                 base::BindOnce(&CheckQuarantineResult, run_loop.QuitClosure(),
                                QuarantineFileResult::OK));
  run_loop.Run();
  EXPECT_TRUE(
      IsFileQuarantined(test_file_, host_url_clean, referrer_url_clean));
}

TEST_F(QuarantineMacTest, IsFileQuarantined_AgentBundleIdentifier) {
  base::RunLoop run_loop;
  QuarantineFile(test_file_, source_url_, referrer_url_,
                 /*request_initiator=*/std::nullopt, "",
                 base::BindOnce(&CheckQuarantineResult, run_loop.QuitClosure(),
                                QuarantineFileResult::OK));
  run_loop.Run();

  NSDictionary* properties = GetQuarantineProperties(test_file_);
  ASSERT_TRUE(properties);

  NSMutableDictionary* mutable_properties = [properties mutableCopy];

  [mutable_properties
      removeObjectForKey:static_cast<NSString*>(
                             kLSQuarantineAgentBundleIdentifierKey)];
  NSError* error = nullptr;
  BOOL success = [file_url_ setResourceValue:mutable_properties
                                      forKey:NSURLQuarantinePropertiesKey
                                       error:&error];
  ASSERT_TRUE(success) << error.localizedDescription;

  EXPECT_FALSE(IsFileQuarantined(test_file_, source_url_, referrer_url_));
}

TEST_F(QuarantineMacTest, NoWhereFromsKeyIfNoURLs) {
  base::RunLoop run_loop;
  QuarantineFile(test_file_, GURL(), GURL(), /*request_initiator=*/std::nullopt,
                 std::string(),
                 base::BindOnce(&CheckQuarantineResult, run_loop.QuitClosure(),
                                QuarantineFileResult::OK));
  run_loop.Run();

  NSString* file_path = base::apple::FilePathToNSString(test_file_);
  ASSERT_NE(nullptr, file_path);
  base::apple::ScopedCFTypeRef<MDItemRef> md_item(
      MDItemCreate(kCFAllocatorDefault, base::apple::NSToCFPtrCast(file_path)));
  if (!md_item) {
    // The quarantine code ignores it if adding origin metadata fails. If for
    // some reason MDItemCreate fails (which it seems to do on the bots, not
    // sure why) just stop on the test.
    return;
  }

  base::apple::ScopedCFTypeRef<CFTypeRef> attr(
      MDItemCopyAttribute(md_item.get(), kMDItemWhereFroms));
  EXPECT_FALSE(attr);
}

TEST_F(QuarantineMacTest, RequestInitiatorReplacesSourceUrl) {
  base::RunLoop run_loop;
  QuarantineFile(test_file_, GURL("data://text/html,payload"), referrer_url_,
                 /*request_initiator=*/
                 url::Origin::Create(GURL("http://www.source.example.com/")),
                 "",
                 base::BindOnce(&CheckQuarantineResult, run_loop.QuitClosure(),
                                QuarantineFileResult::OK));
  run_loop.Run();
  EXPECT_TRUE(IsFileQuarantined(
      test_file_, GURL("http://www.source.example.com/"), referrer_url_));
}

}  // namespace
}  // namespace quarantine
