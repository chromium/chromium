// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/model/offline_page_model_utils.h"

#include <string_view>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace offline_pages {

TEST(OfflinePageModelUtilsTest, ToNamespaceEnum) {
  EXPECT_EQ(model_utils::ToNamespaceEnum(kDefaultNamespace),
            OfflinePagesNamespaceEnumeration::DEFAULT);
  EXPECT_EQ(model_utils::ToNamespaceEnum(kBookmarkNamespace),
            OfflinePagesNamespaceEnumeration::BOOKMARK);
  EXPECT_EQ(model_utils::ToNamespaceEnum(kLastNNamespace),
            OfflinePagesNamespaceEnumeration::LAST_N);
  EXPECT_EQ(model_utils::ToNamespaceEnum(kAsyncNamespace),
            OfflinePagesNamespaceEnumeration::ASYNC_LOADING);
  EXPECT_EQ(model_utils::ToNamespaceEnum(kCCTNamespace),
            OfflinePagesNamespaceEnumeration::CUSTOM_TABS);
  EXPECT_EQ(model_utils::ToNamespaceEnum(kDownloadNamespace),
            OfflinePagesNamespaceEnumeration::DOWNLOAD);
  EXPECT_EQ(model_utils::ToNamespaceEnum(kNTPSuggestionsNamespace),
            OfflinePagesNamespaceEnumeration::NTP_SUGGESTION);
  EXPECT_EQ(model_utils::ToNamespaceEnum(kBrowserActionsNamespace),
            OfflinePagesNamespaceEnumeration::BROWSER_ACTIONS);
}

struct GenerateUniqueFilenameTestCase {
  const std::u16string page_title;
  const GURL page_url;
  const base::FilePath::CharType* expected_basename;
};

const std::vector<GenerateUniqueFilenameTestCase>& UniqueFilenameCases() {
  static base::NoDestructor<std::vector<GenerateUniqueFilenameTestCase>> cases{{
      {u"wikipedia.org-Main_Page", GURL("http://www.wikipedia.org/Main_Page"),
       FILE_PATH_LITERAL("wikipedia.org-Main_Page.mhtml")},
      {u"wikipedia.org-Main_Page", GURL("http://www.wikipedia.org/Main_Page"),
       FILE_PATH_LITERAL("wikipedia.org-Main_Page (1).mhtml")},
      {u"wikipedia.org-Main_Page", GURL("http://www.wikipedia.org/Main_Page"),
       FILE_PATH_LITERAL("wikipedia.org-Main_Page (2).mhtml")},
      {u"wikipedia.org-Main_Page.mhtml",
       GURL("http://www.wikipedia.org/Main_Page"),
       FILE_PATH_LITERAL("wikipedia.org-Main_Page (3).mhtml")},
      {u"wikipedia.org-Main_Page", GURL("http://www.wikipedia.org/Main_Page"),
       FILE_PATH_LITERAL("wikipedia.org-Main_Page (4).mhtml")},
      {u"wikipedia.org", GURL("http://www.wikipedia.org/Main_Page"),
       FILE_PATH_LITERAL("wikipedia.org.mhtml")},
      {u"wikipedia.org", GURL("http://www.wikipedia.org/Main_Page"),
       FILE_PATH_LITERAL("wikipedia.org (1).mhtml")},
      {u"bücher.com", GURL("http://xn--bcher-kva.com"),
       FILE_PATH_LITERAL("bücher.com.mhtml")},
      {u"http://foo.com/path/title.html", GURL("http://foo.com"),
       FILE_PATH_LITERAL("http___foo.com_path_title.html.mhtml")},
      {u"foo.com/foo-%40.html", GURL("http://foo.com/foo-%40.html"),
       FILE_PATH_LITERAL("foo-@.html.mhtml")},
      {u"Viva%40%40%40-TestTitle", GURL("http://foo.com/%40.html"),
       FILE_PATH_LITERAL("Viva%40%40%40-TestTitle.mhtml")},
  }};
  return *cases;
}

// Crashing on Windows, see http://crbug.com/79365
#if BUILDFLAG(IS_WIN)
#define MAYBE_TestGenerateUniqueFilename DISABLED_TestGenerateUniqueFilename
#else
#define MAYBE_TestGenerateUniqueFilename TestGenerateUniqueFilename
#endif
TEST(OfflinePageModelUtilsTest, MAYBE_TestGenerateUniqueFilename) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  for (const auto& test_case : UniqueFilenameCases()) {
    base::FilePath path = model_utils::GenerateUniqueFilenameForOfflinePage(
        test_case.page_title, test_case.page_url, temp_dir.GetPath());
    // Writing a dummy file so the uniquifier can increase.
    base::WriteFile(path, std::string_view());
    EXPECT_EQ(path.BaseName().value(), test_case.expected_basename);
  }
}

}  // namespace offline_pages
