// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_OMNIBOX_MOJO_UTILS_H_
#define CHROME_BROWSER_UI_SEARCH_OMNIBOX_MOJO_UTILS_H_

#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "chrome/common/search/omnibox.mojom-forward.h"

class AutocompleteResult;
class PrefService;

namespace gfx {
struct VectorIcon;
}

namespace omnibox {

extern const char kGoogleGIconResourceName[];
extern const char kBookmarkIconResourceName[];
extern const char kCalculatorIconResourceName[];
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
    const AutocompleteResult& result);

search::mojom::AutocompleteResultPtr CreateAutocompleteResult(
    const base::string16& input,
    const AutocompleteResult& result,
    PrefService* prefs);

}  // namespace omnibox

#endif  // CHROME_BROWSER_UI_SEARCH_OMNIBOX_MOJO_UTILS_H_
