// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/memory/userspace_swap/region.h"

#include <sys/uio.h>

#include <cstdint>
#include <ostream>
#include <string_view>
#include <vector>

#include "base/containers/span.h"

namespace ash {
namespace memory {
namespace userspace_swap {

// AsIovec will return the iovec representation of this Region.
struct iovec COMPONENT_EXPORT(USERSPACE_SWAP) Region::AsIovec() const {
  return {.iov_base = reinterpret_cast<void*>(address), .iov_len = length};
}

std::string_view COMPONENT_EXPORT(
    USERSPACE_SWAP) Region::AsStringPiece() const {
  return std::string_view(reinterpret_cast<char*>(address), length);
}

RegionOverlap COMPONENT_EXPORT(USERSPACE_SWAP) Region::CalculateRegionOverlap(
    const Region& range) const {
  RegionOverlap overlap;

  // We have four possible situations here related to how a region actually fits
  // within a range, as only a portion of a region we manage may have been
  // removed or unmapped. When situations such as 2, 3, and 4 happen we need to
  // deal with them and we will do that by restoring the remaining portion of
  // the region.
  //
  //          [ region_start           region_end ]
  //   1.     [ range_start             range_end ]
  //   2.     [range_start   range_end]
  //   3.                 [range_start   range_end]
  //   4.           [range_start   range_end]
  //
  //   Given these scenarios RemainingRegions will populate |before|
  //   and |after| if there are portions which were unaffected by an
  //   operation. This allows the Removed/Unmapped callbacks to determine how to
  //   restore any memory that was not removed or unmapped.
  uintptr_t region_start = address;
  uintptr_t region_end = address + length;
  uintptr_t range_start = range.address;
  uintptr_t range_end = range.address + range.length;

  CHECK_LE(range_start, range_end);
  CHECK_LE(region_start, region_end);

  if (range_end < region_start || range_start > region_end) {
    return overlap;  // There is no overlap
  }

  // Since we only care about the pieces that related to this region we will
  // clamp range_start and range_end.
  range_start = std::max(range_start, region_start);
  range_end = std::min(range_end, region_end);

  if (range_start > region_start) {
    overlap.before = Region(region_start, range_start - region_start);
  }

  overlap.intersection = Region(range_start, range_end - range_start);

  if (range_end < region_end) {
    overlap.after = Region(range_end, region_end - range_end);
  }

  return overlap;
}

// Easily print a region to a stream, useful for debugging.
std::ostream& COMPONENT_EXPORT(USERSPACE_SWAP) operator<<(
    std::ostream& os,
    const Region& region) {
  os << "[" << reinterpret_cast<void*>(region.address) << "-"
     << reinterpret_cast<void*>(region.address + region.length) << "]";
  return os;
}

RegionOverlap::RegionOverlap() = default;
RegionOverlap::~RegionOverlap() = default;
RegionOverlap::RegionOverlap(const RegionOverlap&) = default;

}  // namespace userspace_swap
}  // namespace memory
}  // namespace ash
