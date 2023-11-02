// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NO_STATE_PREFETCH_BROWSER_PRERENDER_CONFIG_H_
#define COMPONENTS_NO_STATE_PREFETCH_BROWSER_PRERENDER_CONFIG_H_

#include <stddef.h>

#include "base/time/time.h"
#include "ui/gfx/geometry/rect.h"

namespace prerender {

struct Config {
  Config();
  ~Config();

  // Maximum memory use for a prerendered page until it is killed.
  size_t max_bytes;

  // Number of simultaneous prerender pages from link elements allowed. Enforced
  // by NoStatePrefetchLinkManager.
  size_t max_link_concurrency;

  // Number of simultaneous prerender pages from link elements allowed per
  // launching page. Enforced by NoStatePrefetchLinkManager.
  size_t max_link_concurrency_per_launcher;

  // Is rate limiting enabled?
  bool rate_limit_enabled;

  // The maximum time that a prerender can wait for launch in the
  // NoStatePrefetchLinkManager.
  base::TimeDelta max_wait_to_launch;

  // The default time to live of a newly created prerender. May be shortened to
  // abandon_time_to_live, below.
  base::TimeDelta time_to_live;

  // After a prerender has been abandoned by the user navigating away from the
  // source page or otherwise mooting the launcher, how long until the prerender
  // should be removed. This exists because a prerendered page is often
  // navigated to through a chain of redirects; removing the prerender when the
  // link element is removed because of navigation would destroy prerenders just
  // before they were going to be used.
  base::TimeDelta abandon_time_to_live;

  // The default tab bounds used as the prerenderer tab size when the active tab
  // cannot be accessed.
  gfx::Rect default_tab_bounds;
};

}  // namespace prerender

#endif  // COMPONENTS_NO_STATE_PREFETCH_BROWSER_PRERENDER_CONFIG_H_
