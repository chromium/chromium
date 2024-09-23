// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/utility/importer/bookmarks_file_importer.h"

#include <stddef.h>

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace internal {

bool CanImportURL(const GURL& url);

}  // namespace internal

TEST(BookmarksFileImporterTest, CanImportURL) {
  struct TestCase {
    const std::string url;
    const bool can_be_imported;
  } test_cases[] = {
    { "http://www.example.com", true },
    { "https://www.example.com", true },
    { "ftp://www.example.com", true },
    { "aim:GoIm?screenname=myscreenname&message=hello", true },
    { "chrome://version", true },
    { "chrome://chrome-urls", true },
    { "chrome://kill", true },
    { "chrome://about", true },
    { "about:version", true },
    { "about:blank", true },
    { "about:credits", true },
    { "wyciwyg://example.com", false },
    { "place://google.com", false },
    { "about:config", false },
    { "about:moon", false },
  };

  for (size_t i = 0; i < std::size(test_cases); ++i) {
    EXPECT_EQ(test_cases[i].can_be_imported,
              internal::CanImportURL(GURL(test_cases[i].url)));
  }
}
