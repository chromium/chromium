// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/system_profile_user_stream.h"

#include <optional>
#include <string>
#include <string_view>

#include "base/memory/read_only_shared_memory_region.h"
#include "components/crash/core/common/shared_memory_user_stream_reader.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {

class SystemProfileUserStreamTest : public testing::Test {
 public:
  static constexpr uint32_t kSystemProfileSlotCapacityBytes =
      SystemProfileUserStream::kSystemProfileSlotCapacityBytes;

 protected:
  SystemProfileUserStream stream_;
};

namespace {

TEST_F(SystemProfileUserStreamTest, InitializeAndDuplicateRegion) {
  // Gracefully fails if not initialized.
  EXPECT_FALSE(stream_.DuplicateSharedMemoryRegion().IsValid());

  stream_.Initialize();

  base::ReadOnlySharedMemoryRegion region1 =
      stream_.DuplicateSharedMemoryRegion();
  EXPECT_TRUE(region1.IsValid());

  // Repeated calls to Initialize should be harmless.
  stream_.Initialize();
  base::ReadOnlySharedMemoryRegion region2 =
      stream_.DuplicateSharedMemoryRegion();
  EXPECT_TRUE(region2.IsValid());

  // Verify that both handles point to the exact same backing shared memory.
  EXPECT_EQ(region1.GetGUID(), region2.GetGUID());
}

TEST_F(SystemProfileUserStreamTest, WritePayloadSuccess) {
  stream_.Initialize();
  base::ReadOnlySharedMemoryRegion region =
      stream_.DuplicateSharedMemoryRegion();
  ASSERT_TRUE(region.IsValid());

  constexpr std::string_view kPayload = "serialized_system_profile_proto_bytes";
  stream_.WritePayload(kPayload);

  base::ReadOnlySharedMemoryMapping mapping = region.Map();
  ASSERT_TRUE(mapping.IsValid());

  std::optional<crash_reporter::UserStreamData> data =
      crash_reporter::ExtractSharedMemoryUserStreamData(
          mapping.GetMemoryAsSpan<const uint8_t>());
  ASSERT_TRUE(data.has_value());
  EXPECT_EQ(data->user_stream_type, 0x4B6B0003u);
  EXPECT_EQ(
      std::string_view(reinterpret_cast<const char*>(data->payload.data()),
                       data->payload.size()),
      kPayload);
}

// Ensures that attempting to write an oversized payload does not crash.
TEST_F(SystemProfileUserStreamTest, WritePayloadOverflow) {
  stream_.Initialize();

  const std::string kOversizedPayload(kSystemProfileSlotCapacityBytes + 1, 'X');
  stream_.WritePayload(kOversizedPayload);
}

// Ensures that attempting to write a payload without initialization does not
// crash.
TEST_F(SystemProfileUserStreamTest, WritePayloadWithoutInitialization) {
  stream_.WritePayload("payload");
}

}  // namespace
}  // namespace metrics
