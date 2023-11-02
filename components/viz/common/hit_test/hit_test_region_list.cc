// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/hit_test/hit_test_region_list.h"

namespace viz {

HitTestRegionList::HitTestRegionList() = default;
HitTestRegionList::~HitTestRegionList() = default;

HitTestRegionList::HitTestRegionList(const HitTestRegionList&) = default;
HitTestRegionList& HitTestRegionList::operator=(const HitTestRegionList&) =
    default;

HitTestRegionList::HitTestRegionList(HitTestRegionList&&) = default;
HitTestRegionList& HitTestRegionList::operator=(HitTestRegionList&&) = default;

bool HitTestRegionList::IsEqual(const HitTestRegionList& u,
                                const HitTestRegionList& v) {
  // Simple checks first.
  bool ret = u.flags == v.flags && u.bounds == v.bounds &&
             u.transform == v.transform && u.regions.size() == v.regions.size();
  if (!ret)
    return false;
  for (size_t i = u.regions.size(); i > 0 && ret; --i)
    ret &= HitTestRegion::IsEqual(u.regions[i - 1], v.regions[i - 1]);
  return ret;
}

bool HitTestRegion::IsEqual(const HitTestRegion& u, const HitTestRegion& v) {
  return u.flags == v.flags && u.frame_sink_id == v.frame_sink_id &&
         u.rect == v.rect && u.transform == v.transform;
}

}  // namespace viz
