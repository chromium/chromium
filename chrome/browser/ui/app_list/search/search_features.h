// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_FEATURES_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_FEATURES_H_

#include "base/feature_list.h"

namespace search_features {

// Enables cloud game search in the launcher.
extern const base::Feature kLauncherGameSearch;

bool IsLauncherGameSearchEnabled();

}  // namespace search_features

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_FEATURES_H_
