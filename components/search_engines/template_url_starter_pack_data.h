// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_TEMPLATE_URL_STARTER_PACK_DATA_H_
#define COMPONENTS_SEARCH_ENGINES_TEMPLATE_URL_STARTER_PACK_DATA_H_

#include <memory>
#include <vector>

#include "components/search_engines/search_engine_type.h"

struct TemplateURLData;

// The Starter Pack is a set of built-in search engines that allow the user to
// search various parts of Chrome from the Omnibox through keyword mode.  Unlike
// prepopulated engines, starter pack scopes are not "search engines" that
// search the web. Instead, they use the built-in omnibox providers to provide
// suggestions. This file defines those search engines and util functions.

namespace TemplateURLStarterPackData {

struct StarterPackEngine {
  int name_message_id;
  int keyword_message_id;
  const char* const favicon_url;
  const char* const search_url;
  const int id;
  const SearchEngineType type;
};

extern const int kMaxStarterPackEngineID;
extern const int kCurrentDataVersion;

// Returns the current version of the starterpack data, so callers can know when
// they need to re-merge.
int GetDataVersion();

// Returns a vector of all starter pack engines, in TemplateURLData format.
std::vector<std::unique_ptr<TemplateURLData>> GetStarterPackEngines();

}  // namespace TemplateURLStarterPackData

#endif  // COMPONENTS_SEARCH_ENGINES_TEMPLATE_URL_STARTER_PACK_DATA_H_
