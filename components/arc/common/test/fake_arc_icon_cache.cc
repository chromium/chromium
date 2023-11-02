// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/common/test/fake_arc_icon_cache.h"

namespace arc {

FakeArcIconCache::FakeArcIconCache() = default;
FakeArcIconCache::~FakeArcIconCache() = default;

FakeArcIconCache::GetResult FakeArcIconCache::GetActivityIcons(
    const std::vector<ActivityName>& activities,
    OnIconsReadyCallback cb) {
  std::move(cb).Run(std::unique_ptr<ActivityToIconsMap>());
  return GetResult::SUCCEEDED_SYNC;
}

}  // namespace arc
