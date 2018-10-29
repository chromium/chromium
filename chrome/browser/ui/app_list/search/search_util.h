// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_UTIL_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_UTIL_H_

namespace app_list {

// The type of the chrome search result. This is used for logging so do not
// change the order of this enum.
enum SearchResultType {
  // A result that forwards an omnibox search result.
  OMNIBOX_SEARCH_RESULT,
  // An app result.
  APP_SEARCH_RESULT,
  // A search result from the webstore (Deprecated).
  WEBSTORE_SEARCH_RESULT_DEPRECATED,
  // A result that opens a webstore search (Deprecated)
  SEARCH_WEBSTORE_SEARCH_RESULT_DEPRECATED,
  // A result that opens a people search (Deprecated).
  SEARCH_PEOPLE_SEARCH_RESULT_DEPRECATED,
  // A result that opens a suggestion (Deprecated).
  SUGGESTIONS_SEARCH_RESULT_DEPRECATED,
  // A result that is provided by the custom launcher search provider.
  LAUNCHER_SEARCH_PROVIDER_RESULT,
  // A result that is an uninstalled app from a Play Store app search.
  PLAY_STORE_UNINSTALLED_APP,
  // A result that is an instant app from a Play Store app search.
  PLAY_STORE_INSTANT_APP,
  // A result that is an answer card.
  ANSWER_CARD,
  // A result that open a specific activity in an app installed from Play Store.
  PLAY_STORE_APP_SHORTCUT,
  // Boundary is always last.
  SEARCH_RESULT_TYPE_BOUNDARY
};

// Record a UMA histogram.
void RecordHistogram(SearchResultType type);

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_UTIL_H_
