// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/importer/bookmarks_file_importer.h"

#include <stddef.h>

#include <string_view>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "chrome/common/importer/imported_bookmark_entry.h"
#include "chrome/common/importer/importer_bridge.h"
#include "chrome/common/importer/importer_data_types.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/utility/importer/bookmark_html_reader.h"
#include "components/favicon_base/favicon_usage_data.h"
#include "components/url_formatter/url_fixer.h"
#include "content/public/common/url_constants.h"

namespace {

bool IsImporterCancelled(BookmarksFileImporter* importer) {
  return importer->cancelled();
}

}  // namespace

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
    if (url.host_piece() == chrome::kChromeUIAboutHost) {
      return true;
    }

    GURL fixed_url(url_formatter::FixupURL(url.spec(), std::string()));
    const base::span<const base::cstring_view> hosts = chrome::ChromeURLHosts();
    for (const base::cstring_view host : hosts) {
      if (fixed_url.DomainIs(host)) {
        return true;
      }
    }

    if (base::Contains(chrome::ChromeDebugURLs(), fixed_url)) {
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

BookmarksFileImporter::BookmarksFileImporter() {}

BookmarksFileImporter::~BookmarksFileImporter() {}

void BookmarksFileImporter::StartImport(
    const importer::SourceProfile& source_profile,
    uint16_t items,
    ImporterBridge* bridge) {
  // The only thing this importer can import is a bookmarks file, aka
  // "favorites".
  DCHECK_EQ(importer::FAVORITES, items);

  bridge->NotifyStarted();
  bridge->NotifyItemStarted(importer::FAVORITES);

  std::vector<ImportedBookmarkEntry> bookmarks;
  std::vector<importer::SearchEngineInfo> search_engines;
  favicon_base::FaviconUsageDataList favicons;

  bookmark_html_reader::ImportBookmarksFile(
      base::BindRepeating(IsImporterCancelled, base::Unretained(this)),
      base::BindRepeating(internal::CanImportURL), source_profile.source_path,
      &bookmarks, &search_engines, &favicons);

  if (!bookmarks.empty() && !cancelled()) {
    std::u16string first_folder_name =
        bridge->GetLocalizedString(IDS_BOOKMARK_GROUP);
    bridge->AddBookmarks(bookmarks, first_folder_name);
  }
  if (!search_engines.empty())
    bridge->SetKeywords(search_engines, false);
  if (!favicons.empty())
    bridge->SetFavicons(favicons);

  bridge->NotifyItemEnded(importer::FAVORITES);
  bridge->NotifyEnded();
}
