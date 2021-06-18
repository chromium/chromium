// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/memory/userspace_swap/userspace_swap.h"

#include <string>

#include "chromeos/memory/userspace_swap/region.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/os_metrics.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace memory {
namespace userspace_swap {

namespace {
using memory_instrumentation::mojom::VmRegion;
using memory_instrumentation::mojom::VmRegionPtr;

// DefaultEligibleRegion is a region which is eligible by default which can be
// made ineligible by changing one or more of the properties.
VmRegion DefaultEligibleRegion() {
  VmRegion r;
  r.start_address = 0xF00F00BA4;
  r.size_in_bytes =
      UserspaceSwapConfig::Get().vma_region_minimum_size_bytes + 1;
  r.protection_flags =
      VmRegion::kProtectionFlagsRead | VmRegion::kProtectionFlagsWrite;

  // These aren't necessary but we list them to be explicit about the
  // requirements.
  r.byte_locked = 0;
  r.mapped_file = std::string();

  return r;
}

}  // namespace

TEST(EligibleVMA, DefaultIsEligible) {
  ASSERT_TRUE(IsVMASwapEligible(DefaultEligibleRegion().Clone()));
}

TEST(EligibleVMA, SharedIsNotEligible) {
  auto r = DefaultEligibleRegion();
  r.protection_flags |= VmRegion::kProtectionFlagsMayshare;

  ASSERT_FALSE(IsVMASwapEligible(r.Clone()));
}

TEST(EligibleVMA, ProtNoneIsNotEligible) {
  auto r = DefaultEligibleRegion();
  r.protection_flags = 0;

  ASSERT_FALSE(IsVMASwapEligible(r.Clone()));
}

TEST(EligibleVMA, WrOnlyIsNotEligible) {
  auto r = DefaultEligibleRegion();
  r.protection_flags = VmRegion::kProtectionFlagsWrite;

  ASSERT_FALSE(IsVMASwapEligible(r.Clone()));
}

TEST(EligibleVMA, RdOnlyIsNotEligible) {
  auto r = DefaultEligibleRegion();
  r.protection_flags = VmRegion::kProtectionFlagsRead;

  ASSERT_FALSE(IsVMASwapEligible(r.Clone()));
}

TEST(EligibleVMA, ExecIsNotEligible) {
  auto r = DefaultEligibleRegion();
  r.protection_flags |= VmRegion::kProtectionFlagsExec;

  ASSERT_FALSE(IsVMASwapEligible(r.Clone()));
}

TEST(EligibleVMA, FileBackedIsNotEligible) {
  auto r = DefaultEligibleRegion();
  r.mapped_file = "/some/file/foo.so";

  ASSERT_FALSE(IsVMASwapEligible(r.Clone()));
}

TEST(EligibleVMA, AnyLockedRegionIsNotEligible) {
  auto r = DefaultEligibleRegion();
  r.byte_locked = 20 << 10;  // Any non-zero locked will do.
  ASSERT_FALSE(IsVMASwapEligible(r.Clone()));
}

TEST(EligibleVMA, RegionTooSmallIsNotEligible) {
  auto r = DefaultEligibleRegion();
  r.size_in_bytes =
      UserspaceSwapConfig::Get().vma_region_minimum_size_bytes - 1;
  ASSERT_FALSE(IsVMASwapEligible(r.Clone()));
}

TEST(EligibleVMA, RegionTooLargeIsNotEligible) {
  auto r = DefaultEligibleRegion();
  r.size_in_bytes =
      UserspaceSwapConfig::Get().vma_region_maximum_size_bytes + 1;
  ASSERT_FALSE(IsVMASwapEligible(r.Clone()));
}

TEST(GetAllSwapEligibleVMAs, SimpleVerification) {
  std::vector<Region> regions;
  ASSERT_TRUE(GetAllSwapEligibleVMAs(getpid(), &regions));
  ASSERT_GT(regions.size(), 0u);
}

}  // namespace userspace_swap
}  // namespace memory
}  // namespace chromeos
