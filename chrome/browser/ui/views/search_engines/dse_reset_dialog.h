// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SEARCH_ENGINES_DSE_RESET_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_SEARCH_ENGINES_DSE_RESET_DIALOG_H_

#include "components/omnibox/browser/autocomplete_match.h"

class Browser;

namespace search_engines {
// Shows a bubble informing the user that their
// default search engine settings have been reset.
void MaybeShowSearchEngineResetNotification(Browser* browser,
                                            AutocompleteMatch::Type match_type);
}  // namespace search_engines

#endif  // CHROME_BROWSER_UI_VIEWS_SEARCH_ENGINES_DSE_RESET_DIALOG_H_
