// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_creation/notes/core/server/note_data.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace content_creation {

// Tests that NoteData constructor extracts the highlight directive and web page
// url properly from the full url.
TEST(NoteDataTest, ContructorExtractsHighlightDirectiveFromFullUrl) {
  std::string valid_url = "https://example.com/";
  std::string directive_delimiter = "#:~:text=";
  std::string highlight_directive = "patate%20frite";
  std::string full_url = valid_url + directive_delimiter + highlight_directive;

  NoteData test_note_data("quote", full_url);
  EXPECT_EQ(valid_url, test_note_data.webpage_url.spec());
  EXPECT_EQ(highlight_directive, test_note_data.highlight_directive);
}

}  // namespace content_creation
