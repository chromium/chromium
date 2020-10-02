// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINE_UTILS_H_
#define COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINE_UTILS_H_

#include "components/search_engines/search_engine_type.h"

class GURL;

namespace SearchEngineUtils {

// Like the above, but takes a GURL which is expected to represent a search URL.
// This may be called on any thread.
SearchEngineType GetEngineType(const GURL& url);

}  // namespace SearchEngineUtils

#endif  // COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINE_UTILS_H_
