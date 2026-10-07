// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/core/app/shared_memory_user_stream_mach_port.h"

#include <mach/mach.h>

#include <string_view>
#include <utility>
#include <vector>

#include "base/apple/scoped_mach_port.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/shared_memory_mapping.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crash_reporter::internal {

namespace {

base::ReadOnlySharedMemoryRegion CreateSharedMemoryRegion(
    std::string_view payload) {
  auto region = base::ReadOnlySharedMemoryRegion::Create(payload.size());
  CHECK(region.IsValid());
  region.mapping.GetMemoryAsSpan<char>().copy_from(payload);
  return std::move(region.region);
}

}  // namespace

// The sender shouldn't create a Mach port for an empty set of shared-memory
// user streams.
TEST(SharedMemoryUserStreamMachPortTest, EmptyInputs) {
  base::apple::ScopedMachReceiveRight port =
      SendSharedMemoryUserStreamsToPort({});
  EXPECT_FALSE(port.is_valid());
}

// The receiver should return an empty list of shared-memory user streams if it
// gets an invalid Mach port.
TEST(SharedMemoryUserStreamMachPortTest, NullPort) {
  std::vector<base::ReadOnlySharedMemoryRegion> received =
      ReceiveSharedMemoryUserStreamsFromPort(
          base::apple::ScopedMachReceiveRight(MACH_PORT_NULL));
  EXPECT_TRUE(received.empty());
}

// If the receiver gets a valid port that doesn't have a message queued, it
// should return an empty list (and not block).
TEST(SharedMemoryUserStreamMachPortTest, ValidPortWithoutMessage) {
  base::apple::ScopedMachReceiveRight receive_right;
  kern_return_t kr = mach_port_allocate(
      mach_task_self(), MACH_PORT_RIGHT_RECEIVE,
      base::apple::ScopedMachReceiveRight::Receiver(receive_right).get());
  ASSERT_EQ(kr, KERN_SUCCESS);

  std::vector<base::ReadOnlySharedMemoryRegion> received =
      ReceiveSharedMemoryUserStreamsFromPort(std::move(receive_right));
  EXPECT_TRUE(received.empty());
}

TEST(SharedMemoryUserStreamMachPortTest, Success) {
  constexpr std::string_view kPayload = "Test SystemProfile payload 12345";
  auto region = CreateSharedMemoryRegion(kPayload);
  ASSERT_TRUE(region.IsValid());

  std::vector<base::ReadOnlySharedMemoryRegion> regions;
  regions.push_back(std::move(region));

  base::apple::ScopedMachReceiveRight port =
      SendSharedMemoryUserStreamsToPort(std::move(regions));
  ASSERT_TRUE(port.is_valid());

  std::vector<base::ReadOnlySharedMemoryRegion> received =
      ReceiveSharedMemoryUserStreamsFromPort(std::move(port));
  ASSERT_EQ(received.size(), 1u);
  ASSERT_TRUE(received[0].IsValid());
  EXPECT_EQ(received[0].GetSize(), kPayload.size());

  base::ReadOnlySharedMemoryMapping mapping = received[0].Map();
  ASSERT_TRUE(mapping.IsValid());
  std::string_view mapped_str(reinterpret_cast<const char*>(mapping.memory()),
                              mapping.size());
  EXPECT_EQ(mapped_str, kPayload);
}

// Multiple shared-memory user streams should be shared successfully.
// Invalid streams should be skipped by the receiver.
TEST(SharedMemoryUserStreamMachPortTest, SuccessMultipleRegions) {
  constexpr std::string_view kPayload1 = "some payload";
  constexpr std::string_view kPayload2 = "some other payload";

  std::vector<base::ReadOnlySharedMemoryRegion> regions;
  regions.push_back(base::ReadOnlySharedMemoryRegion());
  regions.push_back(CreateSharedMemoryRegion(kPayload1));
  regions.push_back(CreateSharedMemoryRegion(kPayload2));

  base::apple::ScopedMachReceiveRight port =
      SendSharedMemoryUserStreamsToPort(std::move(regions));
  ASSERT_TRUE(port.is_valid());

  std::vector<base::ReadOnlySharedMemoryRegion> received =
      ReceiveSharedMemoryUserStreamsFromPort(std::move(port));

  // The first region was empty and skipped by the receiver. The other two
  // regions' payloads should have been shared successfully.
  ASSERT_EQ(received.size(), 2u);
  EXPECT_TRUE(received[0].IsValid());
  EXPECT_EQ(received[0].GetSize(), kPayload1.size());
  base::ReadOnlySharedMemoryMapping mapping = received[0].Map();
  std::string_view mapped_str(reinterpret_cast<const char*>(mapping.memory()),
                              mapping.size());
  EXPECT_EQ(mapped_str, kPayload1);

  EXPECT_TRUE(received[1].IsValid());
  EXPECT_EQ(received[1].GetSize(), kPayload2.size());
  mapping = received[1].Map();
  mapped_str = std::string_view(reinterpret_cast<const char*>(mapping.memory()),
                                mapping.size());
  EXPECT_EQ(mapped_str, kPayload2);
}

}  // namespace crash_reporter::internal
