// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_TEMPLATE_URL_STARTER_PACK_DATA_H_
#define COMPONENTS_SEARCH_ENGINES_TEMPLATE_URL_STARTER_PACK_DATA_H_

#include <memory>
#include <string>
#include <vector>

#include "components/search_engines/search_engine_type.h"

struct TemplateURLData;

// The Starter Pack is a set of built-in search engines that allow the user to
// search various parts of Chrome from the Omnibox through keyword mode.  Unlike
// prepopulated engines, starter pack scopes are not "search engines" that
// search the web. Instead, they use the built-in omnibox providers to provide
// suggestions. This file defines those search engines and util functions.

namespace TemplateURLStarterPackData {

typedef enum {
  kBookmarks = 1,
  kHistory = 2,
  kTabs = 3,
  kGemini = 4,

  kMaxStarterPackID
} StarterPackID;

struct StarterPackEngine {
  int name_message_id;
  int keyword_message_id;
  const char* const favicon_url;
  const char* const search_url;
  const char* const destination_url;
  const StarterPackID id;
  const SearchEngineType type;
};

extern const int kCurrentDataVersion;
extern const int kFirstCompatibleDataVersion;

// Exposed for testing purposes
extern const StarterPackEngine bookmarks;
extern const StarterPackEngine history;
extern const StarterPackEngine tabs;
extern const StarterPackEngine Gemini;

// Returns the current version of the starterpack data, so callers can know when
// they need to re-merge.
int GetDataVersion();

// Returns the first compatible data version to the current data. Any starter
// pack data version before this will be force updated regardless of user edits.
int GetFirstCompatibleDataVersion();

// Returns a vector of all starter pack engines, in TemplateURLData format.
std::vector<std::unique_ptr<TemplateURLData>> GetStarterPackEngines();

// Returns the destination url for the starter pack engine associated with a
// given starter pack id.
std::u16string GetDestinationUrlForStarterPackID(int id);

}  // namespace TemplateURLStarterPackData

#endif  // COMPONENTS_SEARCH_ENGINES_TEMPLATE_URL_STARTER_PACK_DATA_H_
