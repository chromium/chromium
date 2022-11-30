// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/scanner/signature_matcher.h"

#include <memory>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "chrome/chrome_cleaner/scanner/matcher_util.h"
#include "chrome/chrome_cleaner/test/resources/grit/test_resources.h"
#include "chrome/chrome_cleaner/test/test_file_util.h"
#include "chrome/chrome_cleaner/test/test_strings.h"
#include "chrome/chrome_cleaner/test/test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_cleaner {

// Contents to be scanned.
const char kGoogleName1[] = "This is Google.";
const char kGoogleName2[] = "This is G00gle.";

namespace {

class SignatureMatcherTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    signature_matcher_ = std::make_unique<SignatureMatcher>();
  }

  SignatureMatcherAPI* signature_matcher() const {
    return signature_matcher_.get();
  }

 private:
  // The root of the scoped temporary folder that is used by some tests.
  base::ScopedTempDir temp_dir_;

  // The signature matcher under test.
  std::unique_ptr<SignatureMatcherAPI> signature_matcher_;
};

}  // namespace

TEST_F(SignatureMatcherTest, MatchFileDigestInfo) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());

  size_t filesize = 0;
  std::string digest;

  // Inexistent file.
  base::FilePath path1(scoped_temp_dir.GetPath().Append(L"file1"));
  EXPECT_FALSE(signature_matcher()->MatchFileDigestInfo(path1, &filesize,
                                                        &digest, {"", 0}));
  // Wrong size.
  CreateFileWithContent(path1, kGoogleName1, sizeof(kGoogleName1));
  EXPECT_FALSE(signature_matcher()->MatchFileDigestInfo(path1, &filesize,
                                                        &digest, {"", 0}));
  EXPECT_EQ(sizeof(kGoogleName1), filesize);
  filesize = 0;

  // Wrong digest.
  EXPECT_FALSE(signature_matcher()->MatchFileDigestInfo(
      path1, &filesize, &digest, {"", sizeof(kGoogleName1)}));
  EXPECT_EQ(sizeof(kGoogleName1), filesize);
  std::string digest1;
  ASSERT_TRUE(signature_matcher()->ComputeSHA256DigestOfPath(path1, &digest1));
  EXPECT_EQ(digest1, digest);

  // Successful match.
  EXPECT_TRUE(signature_matcher()->MatchFileDigestInfo(
      path1, &filesize, &digest, {digest1.c_str(), sizeof(kGoogleName1)}));
  // Should work again.
  EXPECT_TRUE(signature_matcher()->MatchFileDigestInfo(
      path1, &filesize, &digest, {digest1.c_str(), sizeof(kGoogleName1)}));
  // And now with another file.
  filesize = 0;
  digest.clear();

  base::FilePath path2(scoped_temp_dir.GetPath().Append(L"file2"));
  EXPECT_FALSE(signature_matcher()->MatchFileDigestInfo(
      path2, &filesize, &digest, {digest1.c_str(), sizeof(kGoogleName1)}));

  CreateFileWithContent(path2, kGoogleName2, sizeof(kGoogleName2));

  std::string digest2;
  ASSERT_TRUE(signature_matcher()->ComputeSHA256DigestOfPath(path2, &digest2));
  EXPECT_TRUE(signature_matcher()->MatchFileDigestInfo(
      path2, &filesize, &digest, {digest2.c_str(), sizeof(kGoogleName2)}));
  EXPECT_EQ(sizeof(kGoogleName2), filesize);
  EXPECT_EQ(digest2, digest);
  EXPECT_TRUE(signature_matcher()->MatchFileDigestInfo(
      path2, &filesize, &digest, {digest2.c_str(), sizeof(kGoogleName2)}));
}

}  // namespace chrome_cleaner
