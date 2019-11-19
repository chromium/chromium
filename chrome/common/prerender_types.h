// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PRERENDER_TYPES_H_
#define CHROME_COMMON_PRERENDER_TYPES_H_

namespace prerender {

enum PrerenderMode {
  // Neither prefetch nor prerender.
  NO_PRERENDER = 0,

  // Only used in tests. Can be removed after http://crbug.com/898955 is fixed.
  DEPRECATED_FULL_PRERENDER = 1,

  // Prefetch some network resources to warm up the cache.
  PREFETCH_ONLY = 2,

  PRERENDER_MODE_COUNT = 3,
};

}  // namespace prerender

#endif  // CHROME_COMMON_PRERENDER_TYPES_H_
