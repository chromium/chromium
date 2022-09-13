// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_COMMON_TEST_FAKE_ARC_ICON_CACHE_H_
#define COMPONENTS_ARC_COMMON_TEST_FAKE_ARC_ICON_CACHE_H_

#include <vector>

#include "components/arc/common/intent_helper/arc_icon_cache_delegate.h"

namespace arc {

// For tests to wrap the real mojo connection.
class FakeArcIconCache : public ArcIconCacheDelegate {
 public:
  FakeArcIconCache();
  FakeArcIconCache(const FakeArcIconCache&) = delete;
  FakeArcIconCache operator=(const FakeArcIconCache&) = delete;
  ~FakeArcIconCache() override;

  // ArcIconCacheDelegate:
  GetResult GetActivityIcons(const std::vector<ActivityName>& activities,
                             OnIconsReadyCallback callback) override;
};

}  // namespace arc

#endif  // COMPONENTS_ARC_COMMON_TEST_FAKE_ARC_ICON_CACHE_H_
