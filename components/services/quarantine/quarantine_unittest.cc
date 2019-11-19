// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/quarantine/quarantine.h"

#include <iterator>
#include <string>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/stl_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace quarantine {

namespace {

const char kTestData[] = "It's okay to have a trailing nul.";
const char kInternetURL[] = "http://example.com/some-url";
const char kInternetReferrerURL[] = "http://example.com/some-other-url";
const char kTestGUID[] = "69f8621d-c46a-4e88-b915-1ce5415cb008";

}  // namespace

TEST(QuarantineTest, FileCanBeOpenedForReadAfterAnnotation) {
  base::ScopedTempDir test_dir;
  ASSERT_TRUE(test_dir.CreateUniqueTempDir());

  base::FilePath test_file = test_dir.GetPath().AppendASCII("foo.class");
  ASSERT_EQ(static_cast<int>(base::size(kTestData)),
            base::WriteFile(test_file, kTestData, base::size(kTestData)));

  EXPECT_EQ(QuarantineFileResult::OK,
            QuarantineFile(test_file, GURL(kInternetURL),
                           GURL(kInternetReferrerURL), kTestGUID));

  std::string contents;
  EXPECT_TRUE(base::ReadFileToString(test_file, &contents));
  EXPECT_EQ(std::string(std::begin(kTestData), std::end(kTestData)), contents);
}

TEST(QuarantineTest, FileCanBeAnnotatedWithNoGUID) {
  base::ScopedTempDir test_dir;
  ASSERT_TRUE(test_dir.CreateUniqueTempDir());

  base::FilePath test_file = test_dir.GetPath().AppendASCII("foo.class");
  ASSERT_EQ(static_cast<int>(base::size(kTestData)),
            base::WriteFile(test_file, kTestData, base::size(kTestData)));

  EXPECT_EQ(QuarantineFileResult::OK,
            QuarantineFile(test_file, GURL(kInternetURL),
                           GURL(kInternetReferrerURL), std::string()));
}

}  // namespace quarantine
