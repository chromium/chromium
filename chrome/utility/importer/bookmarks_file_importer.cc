// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/importer/bookmarks_file_importer.h"

#include <stddef.h>

#include <string_view>

#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/types/expected_macros.h"
#include "chrome/common/importer/importer_bridge.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/favicon_base/favicon_usage_data.h"
#include "components/url_formatter/url_fixer.h"
#include "components/user_data_importer/common/imported_bookmark_entry.h"
#include "components/user_data_importer/common/importer_data_types.h"
#include "components/user_data_importer/content/content_bookmark_parser_utils.h"
#include "components/user_data_importer/utility/bookmark_parser.h"
#include "content/public/common/url_constants.h"

namespace internal {

// Returns true if |url| has a valid scheme that we allow to import. We
// filter out the URL with a unsupported scheme.
bool CanImportURL(const GURL& url) {
  // The URL is not valid.
  if (!url.is_valid()) {
    return false;
  }

  // Filter out the URLs with unsupported schemes.
  for (const char* invalid_scheme : {"wyciwyg", "place"}) {
    if (url.SchemeIs(invalid_scheme)) {
      return false;
    }
  }

  // Check if |url| is about:blank.
  if (url == url::kAboutBlankURL) {
    return true;
  }

  // If |url| starts with chrome:// or about:, check if it's one of the URLs
  // that we support.
  if (url.SchemeIs(content::kChromeUIScheme) ||
      url.SchemeIs(url::kAboutScheme)) {
    if (url.host() == chrome::kChromeUIAboutHost) {
      return true;
    }

    GURL fixed_url(url_formatter::FixupURL(url.spec(), std::string()));
    const base::span<const base::cstring_view> hosts = chrome::ChromeURLHosts();
    for (const base::cstring_view host : hosts) {
      if (fixed_url.DomainIs(host)) {
        return true;
      }
    }

    if (base::Contains(chrome::ChromeDebugURLs(), fixed_url.spec())) {
      return true;
    }

    // If url has either chrome:// or about: schemes but wasn't found in the
    // above lists, it means we don't support it, so we don't allow the user
    // to import it.
    return false;
  }

  // Otherwise, we assume the url has a valid (importable) scheme.
  return true;
}

}  // namespace internal

BookmarksFileImporter::BookmarksFileImporter() = default;

BookmarksFileImporter::~BookmarksFileImporter() = default;

void BookmarksFileImporter::StartImport(
    const user_data_importer::SourceProfile& source_profile,
    uint16_t items,
    ImporterBridge* bridge) {
  // The only thing this importer can import is a bookmarks file, aka
  // "favorites".
  DCHECK_EQ(user_data_importer::FAVORITES, items);
  bridge_ = bridge;

  bridge->NotifyStarted();
  bridge->NotifyItemStarted(user_data_importer::FAVORITES);

  std::string raw_html;

  // ReadFileToString can return false, but still populate something into
  // `raw_html`. In that case, try to recover as much data as possible.
  base::ReadFileToString(source_profile.source_path, &raw_html);
  user_data_importer::BookmarkParser::ParsedBookmarks parsed_bookmarks =
      user_data_importer::ParseBookmarksUnsafe(raw_html);

  if (!parsed_bookmarks.bookmarks.empty()) {
    std::u16string first_folder_name =
        bridge_->GetLocalizedString(IDS_BOOKMARK_GROUP);
    std::erase_if(parsed_bookmarks.bookmarks,
                  [](user_data_importer::ImportedBookmarkEntry bookmark) {
                    return !internal::CanImportURL(bookmark.url);
                  });

    bridge_->AddBookmarks(parsed_bookmarks.bookmarks, first_folder_name);
  }
  if (!parsed_bookmarks.search_engines.empty()) {
    bridge_->SetKeywords(parsed_bookmarks.search_engines, false);
  }
  if (!parsed_bookmarks.favicons.empty()) {
    bridge_->SetFavicons(parsed_bookmarks.favicons);
  }

  bridge_->NotifyItemEnded(user_data_importer::FAVORITES);
  bridge_->NotifyEnded();
}
