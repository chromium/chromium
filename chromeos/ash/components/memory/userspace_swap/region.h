// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef CHROMEOS_ASH_COMPONENTS_MEMORY_USERSPACE_SWAP_REGION_H_
#define CHROMEOS_ASH_COMPONENTS_MEMORY_USERSPACE_SWAP_REGION_H_

#include <sys/uio.h>

#include <cstdint>
#include <optional>
#include <ostream>
#include <string_view>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/numerics/checked_math.h"

namespace ash {
namespace memory {
namespace userspace_swap {

struct RegionOverlap;

// A region describes a block of memory.
class Region {
 public:
  uintptr_t address = 0;
  uintptr_t length = 0;

  Region() = default;
  Region(Region&&) = default;
  Region(const Region&) = default;
  ~Region() = default;
  Region& operator=(const Region&) = default;
  Region& operator=(Region&&) = default;

  template <typename Address, typename Length>
  Region(Address* address, Length length)
      : address(reinterpret_cast<uintptr_t>(const_cast<Address*>(address))),
        length(length) {
    static_assert(std::is_integral<Length>::value,
                  "length must be an integral type");
    static_assert(sizeof(Length) <= sizeof(uintptr_t),
                  "Length cannot be longer than uint64_t");

    // Verify that the end of this region is valid and wouldn't overflow if we
    // added length to the address.
    CHECK((base::CheckedNumeric<uintptr_t>(this->address) + this->length)
              .IsValid());
  }

  template <typename Address, typename Length>
  Region(Address address, Length length)
      : Region(reinterpret_cast<void*>(address), length) {
    static_assert(sizeof(Address) <= sizeof(void*),
                  "Address cannot be longer than a pointer type");
  }

  template <typename Address>
  Region(Address address) : Region(address, 1) {
    static_assert(
        std::is_integral<Address>::value || std::is_pointer<Address>::value,
        "Adress must be integral or pointer type");
  }

  template <typename T>
  Region(const std::vector<T>& vec)
      : Region(vec.data(), vec.size() * sizeof(T)) {}

  template <typename T>
  base::span<T> AsSpan() const {
    return base::span<T>(reinterpret_cast<T*>(address), length);
  }

  struct iovec COMPONENT_EXPORT(USERSPACE_SWAP) AsIovec() const;
  std::string_view COMPONENT_EXPORT(USERSPACE_SWAP) AsStringPiece() const;

  bool operator<(const Region& other) const {
    // Because the standard library treats equality as !less(a,b) &&
    // !less(b,a) our definition of less than will be that this has to be
    // FULLY before other. Overlapping regions are not allowed and are
    // explicitly checked before inserting by using find() any overlap would
    // return equal, this also has the property that you can search for a
    // Region of length 1 to find the mapping for a fault.
    return ((address + length - 1) < other.address);
  }

  // CalculateRegionOverlap can be used to determine how a |range| overlaps with
  // this region. There are five possible outcomes:
  //  1. |range| does not overlap at all with this region, in this situation the
  //  returned RegionOverlap will have none of the members with values.
  //  2. |range| fully covers this region, in this situaton before and after in
  //  the RegionOverlap will be empty and intersection will be identical to this
  //  region.
  //  3. |range| overlaps from the start of of this region, in this case before
  //  will be empty and intersection will contain the overlapped portion and
  //  after will contain the piece that did not intersect.
  //  4. |range| overlaps from the end of this region, In this case before will
  //  contain the piece which does not intersect, intersection will contain the
  //  portion that overlaps and after will be empty.
  //  5. |range| is fully within this region, in this situation all fields will
  //  be set, before will contain the part before the intersection, intersection
  //  will contain an area equal to range, and after will contain the portion
  //  which doesn't intersect after range.
  COMPONENT_EXPORT(USERSPACE_SWAP)
  RegionOverlap CalculateRegionOverlap(const Region& range) const;

  friend std::ostream& operator<<(std::ostream& os, const Region& region);
};

struct COMPONENT_EXPORT(USERSPACE_SWAP) RegionOverlap {
  RegionOverlap();
  ~RegionOverlap();

  RegionOverlap(const RegionOverlap&);

  std::optional<Region> before;
  std::optional<Region> intersection;
  std::optional<Region> after;
};

}  // namespace userspace_swap
}  // namespace memory
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_MEMORY_USERSPACE_SWAP_REGION_H_
