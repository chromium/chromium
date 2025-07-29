// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_DATA_IMPORTER_MOJOM_BOOKMARK_HTML_PARSER_TRAITS_H_
#define COMPONENTS_USER_DATA_IMPORTER_MOJOM_BOOKMARK_HTML_PARSER_TRAITS_H_

#include "base/time/time.h"
#include "components/favicon_base/favicon_usage_data.h"
#include "components/user_data_importer/mojom/bookmark_html_parser.mojom-shared.h"
#include "components/user_data_importer/utility/bookmark_parser.h"
#include "mojo/public/cpp/bindings/array_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "url/gurl.h"
#include "url/mojom/url.mojom-shared.h"

namespace mojo {

template <>
struct StructTraits<user_data_importer::mojom::ImportedBookmarkEntryDataView,
                    user_data_importer::ImportedBookmarkEntry> {
  static bool is_folder(
      const user_data_importer::ImportedBookmarkEntry& value) {
    return value.is_folder;
  }
  static const GURL& url(
      const user_data_importer::ImportedBookmarkEntry& value) {
    return value.url;
  }
  static const std::vector<std::u16string>& path(
      const user_data_importer::ImportedBookmarkEntry& value) {
    return value.path;
  }
  static const std::u16string& title(
      const user_data_importer::ImportedBookmarkEntry& value) {
    return value.title;
  }
  static base::Time creation_time(
      const user_data_importer::ImportedBookmarkEntry& value) {
    return value.creation_time;
  }
  static const std::optional<base::Time>& last_visit_time(
      const user_data_importer::ImportedBookmarkEntry& value) {
    return value.last_visit_time;
  }
  static bool in_toolbar(
      const user_data_importer::ImportedBookmarkEntry& value) {
    return value.in_toolbar;
  }
  static bool Read(
      user_data_importer::mojom::ImportedBookmarkEntryDataView data,
      user_data_importer::ImportedBookmarkEntry* out);
};

template <>
struct StructTraits<user_data_importer::mojom::SearchEngineInfoDataView,
                    user_data_importer::SearchEngineInfo> {
  static const std::u16string& display_name(
      const user_data_importer::SearchEngineInfo& value) {
    return value.display_name;
  }
  static const std::u16string& keyword(
      const user_data_importer::SearchEngineInfo& value) {
    return value.keyword;
  }
  static const std::u16string& url(
      const user_data_importer::SearchEngineInfo& value) {
    return value.url;
  }

  static bool Read(user_data_importer::mojom::SearchEngineInfoDataView data,
                   user_data_importer::SearchEngineInfo* out);
};

template <>
struct StructTraits<user_data_importer::mojom::FaviconUsageDataDataView,
                    favicon_base::FaviconUsageData> {
  static const GURL& favicon_url(const favicon_base::FaviconUsageData& value) {
    return value.favicon_url;
  }
  static const std::vector<unsigned char>& png_data(
      const favicon_base::FaviconUsageData& value) {
    return value.png_data;
  }
  static const std::set<GURL>& urls(
      const favicon_base::FaviconUsageData& value) {
    return value.urls;
  }

  static bool Read(user_data_importer::mojom::FaviconUsageDataDataView data,
                   favicon_base::FaviconUsageData* out);
};

template <>
struct StructTraits<user_data_importer::mojom::ParsedBookmarksDataView,
                    user_data_importer::BookmarkParser::ParsedBookmarks> {
  static const std::vector<user_data_importer::ImportedBookmarkEntry>&
  bookmarks(const user_data_importer::BookmarkParser::ParsedBookmarks& value) {
    return value.bookmarks;
  }
  static const std::vector<user_data_importer::ImportedBookmarkEntry>&
  reading_list(
      const user_data_importer::BookmarkParser::ParsedBookmarks& value) {
    return value.reading_list;
  }
  static const std::vector<user_data_importer::SearchEngineInfo>&
  search_engines(
      const user_data_importer::BookmarkParser::ParsedBookmarks& value) {
    return value.search_engines;
  }
  static const std::vector<favicon_base::FaviconUsageData>& favicons(
      const user_data_importer::BookmarkParser::ParsedBookmarks& value) {
    return value.favicons;
  }

  static bool Read(user_data_importer::mojom::ParsedBookmarksDataView data,
                   user_data_importer::BookmarkParser::ParsedBookmarks* out);
};

}  // namespace mojo

#endif  // COMPONENTS_USER_DATA_IMPORTER_MOJOM_BOOKMARK_HTML_PARSER_TRAITS_H_
