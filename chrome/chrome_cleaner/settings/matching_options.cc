// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/settings/matching_options.h"

namespace chrome_cleaner {

bool MatchingOptions::only_one_footprint() const {
  return only_one_footprint_;
}

void MatchingOptions::set_only_one_footprint(bool only_one_footprint) {
  only_one_footprint_ = only_one_footprint;
}

}  // namespace chrome_cleaner
