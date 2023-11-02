// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/site_isolation/preloaded_isolated_origins.h"

#include "components/site_isolation/buildflags.h"
#include "content/public/browser/site_isolation_policy.h"
#include "url/gurl.h"

#if BUILDFLAG(USE_INTERNAL_ISOLATED_ORIGINS)
#include "components/site_isolation/internal/google_chrome_isolated_origins.h"
#endif

namespace site_isolation {

std::vector<url::Origin> GetBrowserSpecificBuiltInIsolatedOrigins() {
  std::vector<url::Origin> list;

#if BUILDFLAG(USE_INTERNAL_ISOLATED_ORIGINS)
  // Only apply preloaded isolated origins when allowed by site isolation
  // policy (e.g., when memory requirements are satisfied, and when not using
  // full site isolation).
  if (content::SiteIsolationPolicy::ArePreloadedIsolatedOriginsEnabled()) {
    list.reserve(kNumberOfBuiltInIsolatedOrigins);
    for (size_t i = 0; i < kNumberOfBuiltInIsolatedOrigins; i++)
      list.push_back(url::Origin::Create(GURL(kBuiltInIsolatedOrigins[i])));
  }
#endif

  return list;
}

}  // namespace site_isolation
