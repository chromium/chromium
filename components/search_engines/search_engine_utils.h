// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINE_UTILS_H_
#define COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINE_UTILS_H_

#include "components/search_engines/search_engine_type.h"

class GURL;

namespace SearchEngineUtils {

// Takes a GURL and returns the matching enum if it matches the URL of a
// well-known search engine. This may be called on any thread.
SearchEngineType GetEngineType(const GURL& url);

}  // namespace SearchEngineUtils

#endif  // COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINE_UTILS_H_
