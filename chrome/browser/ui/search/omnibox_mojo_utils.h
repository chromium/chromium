// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_OMNIBOX_MOJO_UTILS_H_
#define CHROME_BROWSER_UI_SEARCH_OMNIBOX_MOJO_UTILS_H_

#include <string>
#include <vector>

#include "chrome/common/search/omnibox.mojom-forward.h"

class AutocompleteResult;
class PrefService;

namespace bookmarks {
class BookmarkModel;
}  // namespace bookmarks

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace omnibox {

extern const char kGoogleGIconResourceName[];
extern const char kBookmarkIconResourceName[];
extern const char kCalculatorIconResourceName[];
extern const char kChromeProductIconResourceName[];
extern const char kClockIconResourceName[];
extern const char kDriveDocsIconResourceName[];
extern const char kDriveFolderIconResourceName[];
extern const char kDriveFormIconResourceName[];
extern const char kDriveImageIconResourceName[];
extern const char kDriveLogoIconResourceName[];
extern const char kDrivePdfIconResourceName[];
extern const char kDriveSheetsIconResourceName[];
extern const char kDriveSlidesIconResourceName[];
extern const char kDriveVideoIconResourceName[];
extern const char kExtensionAppIconResourceName[];
extern const char kPageIconResourceName[];
extern const char kSearchIconResourceName[];
extern const char kTrendingUpIconResourceName[];

std::string AutocompleteMatchVectorIconToResourceName(
    const gfx::VectorIcon& icon);

std::vector<search::mojom::AutocompleteMatchPtr> CreateAutocompleteMatches(
    const AutocompleteResult& result,
    bookmarks::BookmarkModel* bookmark_model);

search::mojom::AutocompleteResultPtr CreateAutocompleteResult(
    const std::u16string& input,
    const AutocompleteResult& result,
    bookmarks::BookmarkModel* bookmark_model,
    PrefService* prefs);

}  // namespace omnibox

#endif  // CHROME_BROWSER_UI_SEARCH_OMNIBOX_MOJO_UTILS_H_
