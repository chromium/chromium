// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_data_importer/utility/bookmark_util.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "components/reading_list/core/fake_reading_list_model_storage.h"
#include "components/reading_list/core/reading_list_entry.h"
#include "components/reading_list/core/reading_list_model_impl.h"
#include "components/user_data_importer/common/imported_bookmark_entry.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace user_data_importer {

namespace {

using testing::ElementsAre;
using testing::IsEmpty;
using testing::UnorderedElementsAre;

MATCHER_P2(IsReadingListEntryWithTitleAndUrl,
           expected_title,
           expected_url,
           "") {
  return arg && arg->Title() == expected_title && arg->URL() == expected_url;
}

ImportedBookmarkEntry CreateImportedEntry(std::string_view url,
                                          std::string_view title,
                                          bool is_folder = false) {
  ImportedBookmarkEntry entry;
  entry.url = GURL(url);
  entry.title = base::UTF8ToUTF16(title);
  entry.is_folder = is_folder;
  entry.creation_time = base::Time::FromSecondsSinceUnixEpoch(1700000000);
  return entry;
}

// Creates a loaded `ReadingListModelImpl` for testing `ImportReadingList`.
std::unique_ptr<ReadingListModelImpl> CreateLoadedReadingListModel() {
  auto storage = std::make_unique<FakeReadingListModelStorage>();
  FakeReadingListModelStorage* storage_ptr = storage.get();
  auto model = std::make_unique<ReadingListModelImpl>(
      std::move(storage), syncer::StorageType::kUnspecified,
      syncer::WipeModelUponSyncDisabledBehavior::kNever,
      base::DefaultClock::GetInstance());
  storage_ptr->TriggerLoadCompletion();
  return model;
}

// Tests that `ImportReadingList` returns 0 when given a null `ReadingListModel`
// or an empty list of entries.
TEST(BookmarkUtilTest, ImportReadingListHandlesNullModelAndEmptyInput) {
  std::unique_ptr<ReadingListModelImpl> model = CreateLoadedReadingListModel();

  EXPECT_EQ(
      ImportReadingList(/*reading_list_model=*/nullptr,
                        {CreateImportedEntry("https://example.com/", "Title")}),
      0u);
  EXPECT_EQ(ImportReadingList(model.get(), {}), 0u);
  EXPECT_THAT(model->GetKeys(), IsEmpty());
}

// Tests that `ImportReadingList` imports valid HTTP and HTTPS entries into
// `ReadingListModel`.
TEST(BookmarkUtilTest, ImportReadingListImportsSupportedHttpAndHttpsUrls) {
  constexpr char kHttpUrl[] = "http://example.com/";
  constexpr char kHttpTitle[] = "HTTP Example";
  constexpr char kHttpsUrl[] = "https://example.org/article";
  constexpr char kHttpsTitle[] = "HTTPS Example";
  std::unique_ptr<ReadingListModelImpl> model = CreateLoadedReadingListModel();

  const size_t imported_count = ImportReadingList(
      model.get(), {CreateImportedEntry(kHttpUrl, kHttpTitle),
                    CreateImportedEntry(kHttpsUrl, kHttpsTitle)});

  EXPECT_EQ(imported_count, 2u);
  EXPECT_THAT(model->GetKeys(),
              UnorderedElementsAre(GURL(kHttpUrl), GURL(kHttpsUrl)));
  EXPECT_THAT(model->GetEntryByURL(GURL(kHttpUrl)),
              IsReadingListEntryWithTitleAndUrl(kHttpTitle, GURL(kHttpUrl)));
  EXPECT_THAT(model->GetEntryByURL(GURL(kHttpsUrl)),
              IsReadingListEntryWithTitleAndUrl(kHttpsTitle, GURL(kHttpsUrl)));
}

// Tests that `ImportReadingList` skips entries with invalid URLs, unsupported
// URL schemes (such as `mailto:`, `javascript:`, `tel:`, `file:`, `chrome:`,
// `data:`, and `about:`), or folder entries.
TEST(BookmarkUtilTest,
     ImportReadingListSkipsUnsupportedSchemesInvalidUrlsAndFolders) {
  constexpr char kValidUrl[] = "https://valid.example.com/";
  constexpr char kValidTitle[] = "Valid Entry";
  std::unique_ptr<ReadingListModelImpl> model = CreateLoadedReadingListModel();

  const size_t imported_count = ImportReadingList(
      model.get(),
      {CreateImportedEntry("", "Empty URL"),
       CreateImportedEntry("not a valid url", "Invalid URL"),
       CreateImportedEntry("mailto:user@example.com", "Mailto"),
       CreateImportedEntry("javascript:void(0)", "JavaScript"),
       CreateImportedEntry("tel:+1234567890", "Tel"),
       CreateImportedEntry("file:///tmp/index.html", "File"),
       CreateImportedEntry("chrome://settings/", "Chrome"),
       CreateImportedEntry("data:text/plain,hello", "Data"),
       CreateImportedEntry("about:blank", "About"),
       CreateImportedEntry("https://folder.example.com/", "Folder",
                           /*is_folder=*/true),
       CreateImportedEntry(kValidUrl, kValidTitle)});

  EXPECT_EQ(imported_count, 1u);
  EXPECT_THAT(model->GetKeys(), ElementsAre(GURL(kValidUrl)));
  EXPECT_THAT(model->GetEntryByURL(GURL(kValidUrl)),
              IsReadingListEntryWithTitleAndUrl(kValidTitle, GURL(kValidUrl)));
}

}  // namespace

}  // namespace user_data_importer
