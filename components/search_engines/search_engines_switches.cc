// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/search_engines_switches.h"

namespace switches {

// Additional query params to insert in the search and instant URLs.  Useful for
// testing.
const char kExtraSearchQueryParams[] = "extra-search-query-params";

// Override the country used for search engine choice region checks.
// Intended for testing. Expects 2-letter country codes.
const char kSearchEngineChoiceCountry[] = "search-engine-choice-country";

}  // namespace switches
