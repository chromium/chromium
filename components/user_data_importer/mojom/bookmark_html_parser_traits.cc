// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_data_importer/mojom/bookmark_html_parser_traits.h"

#include "mojo/public/cpp/base/string16_mojom_traits.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "url/mojom/url_gurl_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<user_data_importer::mojom::ImportedBookmarkEntryDataView,
                  user_data_importer::ImportedBookmarkEntry>::
    Read(user_data_importer::mojom::ImportedBookmarkEntryDataView data,
         user_data_importer::ImportedBookmarkEntry* out) {
  out->is_folder = data.is_folder();
  out->in_toolbar = data.in_toolbar();
  return data.ReadUrl(&out->url) && data.ReadPath(&out->path) &&
         data.ReadTitle(&out->title) &&
         data.ReadCreationTime(&out->creation_time) &&
         data.ReadLastVisitTime(&out->last_visit_time);
}

// static
bool StructTraits<user_data_importer::mojom::SearchEngineInfoDataView,
                  user_data_importer::SearchEngineInfo>::
    Read(user_data_importer::mojom::SearchEngineInfoDataView data,
         user_data_importer::SearchEngineInfo* out) {
  return data.ReadDisplayName(&out->display_name) &&
         data.ReadKeyword(&out->keyword) && data.ReadUrl(&out->url);
}

// static
bool StructTraits<user_data_importer::mojom::FaviconUsageDataDataView,
                  favicon_base::FaviconUsageData>::
    Read(user_data_importer::mojom::FaviconUsageDataDataView data,
         favicon_base::FaviconUsageData* out) {
  if (!data.ReadFaviconUrl(&out->favicon_url) ||
      !data.ReadPngData(&out->png_data)) {
    return false;
  }

  // We can't deserialize as a std::set, so we have to manually copy from the
  // vector.
  out->urls.clear();
  std::vector<GURL> mojo_urls;
  if (!data.ReadUrls(&mojo_urls)) {
    return false;
  }
  out->urls.insert(mojo_urls.begin(), mojo_urls.end());
  if (mojo_urls.size() != out->urls.size()) {
    return false;
  }

  return true;
}

// static
bool StructTraits<user_data_importer::mojom::ParsedBookmarksDataView,
                  user_data_importer::BookmarkParser::ParsedBookmarks>::
    Read(user_data_importer::mojom::ParsedBookmarksDataView data,
         user_data_importer::BookmarkParser::ParsedBookmarks* out) {
  return data.ReadBookmarks(&out->bookmarks) &&
         data.ReadReadingList(&out->reading_list) &&
         data.ReadSearchEngines(&out->search_engines) &&
         data.ReadFavicons(&out->favicons);
}

}  // namespace mojo
