// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/parse_data_path.h"

#include <optional>

#include "base/unguessable_token.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace printing {

TEST(ParseDataPathTest, ParseDataPath) {
  base::UnguessableToken token = base::UnguessableToken::Create();
  std::optional<PrintPreviewIdAndPageIndex> parsed =
      ParseDataPath(token.ToString() + "/4/print.pdf");
  ASSERT_TRUE(parsed);

  EXPECT_EQ(parsed->ui_id, token);
  EXPECT_EQ(parsed->page_index, 4);
}

TEST(ParseDataPathTest, ParseDataPathTest) {
  std::optional<PrintPreviewIdAndPageIndex> parsed =
      ParseDataPath("123456789abcdef00fedcba987654321/0/test.pdf");
  ASSERT_TRUE(parsed);

  EXPECT_EQ(parsed->ui_id, base::UnguessableToken::Deserialize(
                               0x123456789abcdef0, 0x0fedcba987654321)
                               .value());
  EXPECT_EQ(parsed->page_index, 0);
}

TEST(ParseDataPathTest, ParseDataPathValid) {
  base::UnguessableToken token = base::UnguessableToken::Create();
  EXPECT_TRUE(ParseDataPath(token.ToString() + "/2/print.pdf"));
}

TEST(ParseDataPathTest, ParseDataPathInvalid) {
  // Doesn't end in print.pdf
  EXPECT_FALSE(ParseDataPath("pdf/browser_api.js"));
  // Doesn't have both page index and UI ID.
  EXPECT_FALSE(ParseDataPath("1234567890abcdef1234567890abcdef/print.pdf"));
  // Invalid UI ID (not hex)
  EXPECT_FALSE(ParseDataPath("z234567890abcdef1234567890abcdef/0/print.pdf"));
  // Invalid UI ID (too short)
  EXPECT_FALSE(ParseDataPath("1234567890abcdef1234567890abcde/0/print.pdf"));
  // Non-integer page index
  base::UnguessableToken token = base::UnguessableToken::Create();
  EXPECT_FALSE(ParseDataPath(token.ToString() + "/foo/print.pdf"));
}

}  // namespace printing
