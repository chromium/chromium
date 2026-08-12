// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/core/app/shared_memory_user_stream_data_source.h"

#include <stdint.h>

#include <algorithm>
#include <string>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "components/crash/core/common/shared_memory_user_stream_writer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/crashpad/crashpad/minidump/minidump_user_extension_stream_data_source.h"

namespace crash_reporter::internal {

namespace {

class FakeDelegate
    : public crashpad::MinidumpUserExtensionStreamDataSource::Delegate {
 public:
  bool ExtensionStreamDataSourceRead(const void* data, size_t size) override {
    const auto data_span =
        UNSAFE_BUFFERS(base::span(static_cast<const uint8_t*>(data), size));
    data_.assign(data_span.begin(), data_span.end());
    return true;
  }

  const std::vector<uint8_t>& data() const { return data_; }

 private:
  std::vector<uint8_t> data_;
};

}  // namespace

TEST(SharedMemoryUserStreamDataSourceTest, ProducesStreamFromWriter) {
  SharedMemoryUserStreamWriter writer(0x1234, 128);

  const std::string test_data = "Crashpad Shared Memory Stream Payload";
  const bool write_ok = writer.WriteData([&](base::span<uint8_t> target) {
    std::ranges::copy(test_data, target.begin());
    return test_data.size();
  });
  ASSERT_TRUE(write_ok);

  SharedMemoryUserStreamDataSource source(writer.DuplicateRegion().Map());
  const auto data_source = source.ProduceStreamData(nullptr);
  ASSERT_NE(data_source, nullptr);
  EXPECT_EQ(data_source->stream_type(), 0x1234u);
  EXPECT_EQ(data_source->StreamDataSize(), test_data.size());

  FakeDelegate delegate;
  EXPECT_TRUE(data_source->ReadStreamData(&delegate));
  const std::string result(delegate.data().begin(), delegate.data().end());
  EXPECT_EQ(result, test_data);
}

TEST(SharedMemoryUserStreamDataSourceTest, RejectsInvalidMapping) {
  base::ReadOnlySharedMemoryMapping empty_mapping;
  SharedMemoryUserStreamDataSource source(std::move(empty_mapping));
  EXPECT_FALSE(source.ProduceStreamData(nullptr));
}

TEST(SharedMemoryUserStreamDataSourceTest, RejectsInvalidStreamData) {
  base::MappedReadOnlyRegion mapped_region =
      base::ReadOnlySharedMemoryRegion::Create(64);
  ASSERT_TRUE(mapped_region.IsValid());

  SharedMemoryUserStreamDataSource source(mapped_region.region.Map());
  EXPECT_FALSE(source.ProduceStreamData(nullptr));
}

}  // namespace crash_reporter::internal
