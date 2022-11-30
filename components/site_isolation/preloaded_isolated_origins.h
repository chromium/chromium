// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SITE_ISOLATION_PRELOADED_ISOLATED_ORIGINS_H_
#define COMPONENTS_SITE_ISOLATION_PRELOADED_ISOLATED_ORIGINS_H_

#include <vector>

#include "url/origin.h"

namespace site_isolation {

// Retrieves a browser-specific list of isolated origins that should be loaded
// at startup.
std::vector<url::Origin> GetBrowserSpecificBuiltInIsolatedOrigins();

}  // namespace site_isolation

#endif  // COMPONENTS_SITE_ISOLATION_PRELOADED_ISOLATED_ORIGINS_H_
