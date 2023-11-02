// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/drive/drive_api_util.h"

#include "base/files/scoped_temp_dir.h"
#include "base/hash/md5.h"
#include "google_apis/common/test_util.h"
#include "google_apis/drive/drive_api_parser.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace drive {
namespace util {

TEST(DriveApiUtilTest, EscapeQueryStringValue) {
  EXPECT_EQ("abcde", EscapeQueryStringValue("abcde"));
  EXPECT_EQ("\\'", EscapeQueryStringValue("'"));
  EXPECT_EQ("\\'abcde\\'", EscapeQueryStringValue("'abcde'"));
  EXPECT_EQ("\\\\", EscapeQueryStringValue("\\"));
  EXPECT_EQ("\\\\\\'", EscapeQueryStringValue("\\'"));
}

TEST(DriveApiUtilTest, TranslateQuery) {
  EXPECT_EQ("", TranslateQuery(""));
  EXPECT_EQ("fullText contains 'dog'", TranslateQuery("dog"));
  EXPECT_EQ("fullText contains 'dog' and fullText contains 'cat'",
            TranslateQuery("dog cat"));
  EXPECT_EQ("not fullText contains 'cat'", TranslateQuery("-cat"));
  EXPECT_EQ("fullText contains 'dog cat'", TranslateQuery("\"dog cat\""));

  // Should handles full-width white space correctly.
  // Note: \xE3\x80\x80 (\u3000) is Ideographic Space (a.k.a. Japanese
  //   full-width whitespace).
  EXPECT_EQ("fullText contains 'dog' and fullText contains 'cat'",
            TranslateQuery("dog" "\xE3\x80\x80" "cat"));

  // If the quoted token is not closed (i.e. the last '"' is missing),
  // we handle the remaining string is one token, as a fallback.
  EXPECT_EQ("fullText contains 'dog cat'", TranslateQuery("\"dog cat"));

  // For quoted text with leading '-'.
  EXPECT_EQ("not fullText contains 'dog cat'", TranslateQuery("-\"dog cat\""));

  // Empty tokens should be simply ignored.
  EXPECT_EQ("", TranslateQuery("-"));
  EXPECT_EQ("", TranslateQuery("\"\""));
  EXPECT_EQ("", TranslateQuery("-\"\""));
  EXPECT_EQ("", TranslateQuery("\"\"\"\""));
  EXPECT_EQ("", TranslateQuery("\"\" \"\""));
  EXPECT_EQ("fullText contains 'dog'", TranslateQuery("\"\" dog \"\""));
}

TEST(DriveAPIUtilTest, CanonicalizeResourceId) {
  std::string resource_id("1YsCnrMxxgp7LDdtlFDt-WdtEIth89vA9inrILtvK-Ug");

  // New style ID is unchanged.
  EXPECT_EQ(resource_id, CanonicalizeResourceId(resource_id));

  // Drop prefixes from old style IDs.
  EXPECT_EQ(resource_id, CanonicalizeResourceId("document:" + resource_id));
  EXPECT_EQ(resource_id, CanonicalizeResourceId("spreadsheet:" + resource_id));
  EXPECT_EQ(resource_id, CanonicalizeResourceId("presentation:" + resource_id));
  EXPECT_EQ(resource_id, CanonicalizeResourceId("drawing:" + resource_id));
  EXPECT_EQ(resource_id, CanonicalizeResourceId("table:" + resource_id));
  EXPECT_EQ(resource_id, CanonicalizeResourceId("externalapp:" + resource_id));
}

TEST(DriveAPIUtilTest, GetMd5Digest) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath path = temp_dir.GetPath().AppendASCII("test.txt");
  const char kTestData[] = "abcdefghijklmnopqrstuvwxyz0123456789";
  ASSERT_TRUE(google_apis::test_util::WriteStringToFile(path, kTestData));

  EXPECT_EQ(base::MD5String(kTestData), GetMd5Digest(path, nullptr));
}

TEST(DriveAPIUtilTest, HasHostedDocumentExtension) {
  EXPECT_TRUE(
      HasHostedDocumentExtension(base::FilePath::FromUTF8Unsafe("xx.gdoc")));
  EXPECT_TRUE(
      HasHostedDocumentExtension(base::FilePath::FromUTF8Unsafe("xx.gsheet")));
  EXPECT_TRUE(
      HasHostedDocumentExtension(base::FilePath::FromUTF8Unsafe("xx.gslides")));
  EXPECT_TRUE(
      HasHostedDocumentExtension(base::FilePath::FromUTF8Unsafe("xx.gdraw")));
  EXPECT_TRUE(
      HasHostedDocumentExtension(base::FilePath::FromUTF8Unsafe("xx.gtable")));
  EXPECT_TRUE(
      HasHostedDocumentExtension(base::FilePath::FromUTF8Unsafe("xx.gform")));
  EXPECT_TRUE(
      HasHostedDocumentExtension(base::FilePath::FromUTF8Unsafe("xx.gmaps")));
  EXPECT_TRUE(
      HasHostedDocumentExtension(base::FilePath::FromUTF8Unsafe("xx.gsite")));
  EXPECT_TRUE(
      HasHostedDocumentExtension(base::FilePath::FromUTF8Unsafe("xx.glink")));

  EXPECT_FALSE(
      HasHostedDocumentExtension(base::FilePath::FromUTF8Unsafe("xx.gdocs")));
  EXPECT_FALSE(
      HasHostedDocumentExtension(base::FilePath::FromUTF8Unsafe("xx.docx")));
  EXPECT_FALSE(
      HasHostedDocumentExtension(base::FilePath::FromUTF8Unsafe("xx.jpg")));
  EXPECT_FALSE(
      HasHostedDocumentExtension(base::FilePath::FromUTF8Unsafe("xx.gmap")));
}

}  // namespace util
}  // namespace drive
