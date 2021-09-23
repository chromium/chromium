// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_COMMON_ACTIVITY_URL_FILTER_H_
#define CHROMECAST_COMMON_ACTIVITY_URL_FILTER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "components/url_matcher/url_matcher.h"
#include "url/gurl.h"

namespace chromecast {

class ActivityUrlFilter {
 public:
  // |url_filters| are applied to network requests from the Activity.
  explicit ActivityUrlFilter(const std::vector<std::string>& url_filters);

  ActivityUrlFilter(const ActivityUrlFilter&) = delete;
  ActivityUrlFilter& operator=(const ActivityUrlFilter&) = delete;

  ~ActivityUrlFilter();

  // Returns true if the given url matches to any whitelisted URL.
  bool UrlMatchesWhitelist(const GURL& url);

 private:
  std::unique_ptr<url_matcher::URLMatcher> url_matcher_;
};

}  // namespace chromecast

#endif  // CHROMECAST_COMMON_ACTIVITY_URL_FILTER_H_
