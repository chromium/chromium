// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_TAB_URL_PROVIDER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_TAB_URL_PROVIDER_H_

#include <vector>

#include "base/time/time.h"
#include "url/gurl.h"

namespace optimization_guide {

// A class to handle querying for the tab URLs for a user.
class TabUrlProvider {
 public:
  virtual ~TabUrlProvider() = default;
  TabUrlProvider(const TabUrlProvider&) = delete;
  TabUrlProvider& operator=(const TabUrlProvider&) = delete;

  // Returns URLS of tabs that are considered active for the user, as
  // represented by |profile|. Tabs are considered active if they were last
  // shown within |duration_since_last_shown|. The returned vector will be
  // sorted by descending time since last shown.
  virtual const std::vector<GURL> GetUrlsOfActiveTabs(
      const base::TimeDelta& duration_since_last_shown) = 0;

 protected:
  TabUrlProvider() = default;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_TAB_URL_PROVIDER_H_
