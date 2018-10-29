// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/quarantine/quarantine.h"

#include <errno.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/xattr.h>

#include <algorithm>
#include <sstream>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "components/download/quarantine/common_linux.h"
#include "components/download/quarantine/test_support.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace download {
namespace {

using std::istringstream;
using std::string;
using std::vector;

class QuarantineLinuxTest : public testing::Test {
 public:
  QuarantineLinuxTest()
      : source_url_("http://www.source.com"),
        referrer_url_("http://www.referrer.com"),
        is_xattr_supported_(false) {}

  const base::FilePath& test_file() const { return test_file_; }

  const base::FilePath& test_dir() const { return temp_dir_.GetPath(); }

  const GURL& source_url() const { return source_url_; }

  const GURL& referrer_url() const { return referrer_url_; }

  bool is_xattr_supported() const { return is_xattr_supported_; }

 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(
        base::CreateTemporaryFileInDir(temp_dir_.GetPath(), &test_file_));
    int result =
        setxattr(test_file_.value().c_str(), "user.test", "test", 4, 0);
    is_xattr_supported_ = (!result) || (errno != ENOTSUP);
    if (!is_xattr_supported_) {
      LOG(WARNING) << "Test will be skipped because extended attributes are "
                      "not supported on this OS/file system.";
    }
  }

  void GetExtendedAttributeNames(vector<string>* attr_names) const {
    ssize_t len = listxattr(test_file().value().c_str(), nullptr, 0);
    if (len <= static_cast<ssize_t>(0))
      return;
    char* buffer = new char[len];
    len = listxattr(test_file().value().c_str(), buffer, len);
    *attr_names =
        base::SplitString(string(buffer, len), std::string(1, '\0'),
                          base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    delete[] buffer;
  }

 private:
  base::ScopedTempDir temp_dir_;
  base::FilePath test_file_;
  GURL source_url_;
  GURL referrer_url_;
  bool is_xattr_supported_;
};

}  // namespace

TEST_F(QuarantineLinuxTest, CheckMetadataSetCorrectly) {
  if (!is_xattr_supported())
    return;
  EXPECT_EQ(
      QuarantineFileResult::OK,
      QuarantineFile(test_file(), source_url(), referrer_url(), std::string()));
  EXPECT_TRUE(IsFileQuarantined(test_file(), source_url(), referrer_url()));
}

TEST_F(QuarantineLinuxTest, SetMetadataMultipleTimes) {
  if (!is_xattr_supported())
    return;
  GURL dummy_url("http://www.dummy.com");
  EXPECT_EQ(QuarantineFileResult::OK,
            QuarantineFile(test_file(), dummy_url, dummy_url, std::string()));
  EXPECT_EQ(
      QuarantineFileResult::OK,
      QuarantineFile(test_file(), source_url(), referrer_url(), std::string()));
  EXPECT_TRUE(IsFileQuarantined(test_file(), source_url(), referrer_url()));
}

TEST_F(QuarantineLinuxTest, InvalidSourceURLTest) {
  if (!is_xattr_supported())
    return;
  GURL invalid_url;
  vector<string> attr_names;
  EXPECT_EQ(
      QuarantineFileResult::ANNOTATION_FAILED,
      QuarantineFile(test_file(), invalid_url, referrer_url(), std::string()));
  GetExtendedAttributeNames(&attr_names);
  EXPECT_FALSE(base::ContainsValue(attr_names, kSourceURLExtendedAttrName));
  EXPECT_TRUE(base::ContainsValue(attr_names, kReferrerURLExtendedAttrName));
}

TEST_F(QuarantineLinuxTest, InvalidReferrerURLTest) {
  if (!is_xattr_supported())
    return;
  GURL invalid_url;
  vector<string> attr_names;
  EXPECT_EQ(
      QuarantineFileResult::OK,
      QuarantineFile(test_file(), source_url(), invalid_url, std::string()));
  GetExtendedAttributeNames(&attr_names);
  EXPECT_FALSE(base::ContainsValue(attr_names, kReferrerURLExtendedAttrName));
  EXPECT_TRUE(IsFileQuarantined(test_file(), source_url(), GURL()));
}

TEST_F(QuarantineLinuxTest, InvalidURLsTest) {
  if (!is_xattr_supported())
    return;
  GURL invalid_url;
  vector<string> attr_names;
  EXPECT_EQ(
      QuarantineFileResult::ANNOTATION_FAILED,
      QuarantineFile(test_file(), invalid_url, invalid_url, std::string()));
  GetExtendedAttributeNames(&attr_names);
  EXPECT_FALSE(base::ContainsValue(attr_names, kSourceURLExtendedAttrName));
  EXPECT_FALSE(base::ContainsValue(attr_names, kReferrerURLExtendedAttrName));
  EXPECT_FALSE(IsFileQuarantined(test_file(), GURL(), GURL()));
}

TEST_F(QuarantineLinuxTest, IsFileQuarantined) {
  if (!is_xattr_supported())
    return;
  base::FilePath does_not_exist = test_dir().AppendASCII("a.jar");
  EXPECT_FALSE(IsFileQuarantined(does_not_exist, GURL(), GURL()));

  base::FilePath no_annotations = test_dir().AppendASCII("b.jar");
  ASSERT_EQ(5, base::WriteFile(no_annotations, "Hello", 5));
  EXPECT_FALSE(IsFileQuarantined(no_annotations, GURL(), GURL()));

  base::FilePath source_only = test_dir().AppendASCII("c.jar");
  ASSERT_EQ(5, base::WriteFile(source_only, "Hello", 5));
  ASSERT_EQ(QuarantineFileResult::OK,
            QuarantineFile(source_only, source_url(), GURL(), std::string()));
  EXPECT_TRUE(IsFileQuarantined(source_only, source_url(), GURL()));
  EXPECT_TRUE(IsFileQuarantined(source_only, GURL(), GURL()));
  EXPECT_TRUE(IsFileQuarantined(source_only, GURL(), referrer_url()));
  EXPECT_FALSE(IsFileQuarantined(source_only, referrer_url(), GURL()));

  base::FilePath fully_annotated = test_dir().AppendASCII("d.jar");
  ASSERT_EQ(5, base::WriteFile(fully_annotated, "Hello", 5));
  ASSERT_EQ(QuarantineFileResult::OK,
            QuarantineFile(fully_annotated, source_url(), referrer_url(),
                           std::string()));
  EXPECT_TRUE(IsFileQuarantined(fully_annotated, GURL(), GURL()));
  EXPECT_TRUE(IsFileQuarantined(fully_annotated, source_url(), GURL()));
  EXPECT_TRUE(IsFileQuarantined(fully_annotated, source_url(), referrer_url()));
  EXPECT_TRUE(IsFileQuarantined(fully_annotated, GURL(), referrer_url()));
  EXPECT_FALSE(IsFileQuarantined(fully_annotated, source_url(), source_url()));
  EXPECT_FALSE(
      IsFileQuarantined(fully_annotated, referrer_url(), referrer_url()));
}

}  // namespace download
