// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/no_state_prefetch/browser/no_state_prefetch_config.h"

namespace prerender {

NoStatePrefetchConfig::NoStatePrefetchConfig()
    : max_bytes(150 * 1024 * 1024),
      max_link_concurrency(1),
      max_link_concurrency_per_launcher(1),
      rate_limit_enabled(true),
      max_wait_to_launch(base::Minutes(4)),
      time_to_live(base::Minutes(5)),
      abandon_time_to_live(base::Seconds(3)),
      default_tab_bounds(640, 480) {}

NoStatePrefetchConfig::~NoStatePrefetchConfig() = default;

}  // namespace prerender
