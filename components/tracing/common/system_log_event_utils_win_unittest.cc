// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tracing/common/system_log_event_utils_win.h"

#include <windows.h>

#include <algorithm>
#include <optional>

#include "base/containers/buffer_iterator.h"
#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/win/sid.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tracing {

TEST(SystemLogEventUtilsTest, CopyStringEmptyIterator) {
  base::BufferIterator<const uint8_t> iterator;
  ASSERT_EQ(CopyString(iterator), std::nullopt);
}

TEST(SystemLogEventUtilsTest, CopyStringNoTerminator) {
  static constexpr uint8_t kBytes[] = {0x01, 0xff};
  base::BufferIterator<const uint8_t> iterator(
      (base::span<const uint8_t>(kBytes)));
  ASSERT_EQ(CopyString(iterator), std::nullopt);
  ASSERT_EQ(iterator.position(), 0u);
}

TEST(SystemLogEventUtilsTest, CopyStringEmptyString) {
  static constexpr uint8_t kBytes[] = {0x00, 0x01};
  base::BufferIterator<const uint8_t> iterator(
      (base::span<const uint8_t>(kBytes)));
  ASSERT_EQ(CopyString(iterator), std::string());
  ASSERT_EQ(iterator.position(), sizeof(std::string::value_type));
}

TEST(SystemLogEventUtilsTest, CopyString) {
  static constexpr uint8_t kBytes[] = {0x48, 0x55, 0x4d, 0x00, 0x01};
  base::BufferIterator<const uint8_t> iterator(
      (base::span<const uint8_t>(kBytes)));
  ASSERT_EQ(CopyString(iterator), std::string("HUM"));
  ASSERT_EQ(iterator.position(), 4 * sizeof(std::string::value_type));
}

TEST(SystemLogEventUtilsTest, CopyWStringEmptyIterator) {
  base::BufferIterator<const uint8_t> empty;
  ASSERT_EQ(CopyWString(empty), std::nullopt);
}

TEST(SystemLogEventUtilsTest, CopyWStringTooSmall) {
  static constexpr uint8_t kBytes[] = {0x01};
  base::BufferIterator<const uint8_t> iterator(
      (base::span<const uint8_t>(kBytes)));
  ASSERT_EQ(CopyWString(iterator), std::nullopt);
  ASSERT_EQ(iterator.position(), 0u);
}

TEST(SystemLogEventUtilsTest, CopyWStringNoTerminator) {
  static constexpr uint8_t kBytes[] = {0x01, 0xff};
  base::BufferIterator<const uint8_t> iterator(
      (base::span<const uint8_t>(kBytes)));
  ASSERT_EQ(CopyWString(iterator), std::nullopt);
  ASSERT_EQ(iterator.position(), 0u);
}

TEST(SystemLogEventUtilsTest, CopyWStringEmptyString) {
  static constexpr uint8_t kBytes[] = {0x00, 0x00, 0x01};
  base::BufferIterator<const uint8_t> iterator(
      (base::span<const uint8_t>(kBytes)));
  ASSERT_EQ(CopyWString(iterator), std::wstring());
  ASSERT_EQ(iterator.position(), sizeof(std::wstring::value_type));
}

TEST(SystemLogEventUtilsTest, CopyWString) {
#if defined(ARCH_CPU_LITTLE_ENDIAN)
  static constexpr uint8_t kBytes[] = {0x48, 0x00, 0x55, 0x00, 0x4d,
                                       0x00, 0x00, 0x00, 0x01};
#else
  static constexpr uint8_t kBytes[] = {0x00, 0x48, 0x00, 0x55, 0x00,
                                       0x4d, 0x00, 0x00, 0x01};
#endif
  base::BufferIterator<const uint8_t> iterator(
      (base::span<const uint8_t>(kBytes)));
  ASSERT_EQ(CopyWString(iterator), std::wstring(L"HUM"));
  ASSERT_EQ(iterator.position(), 4 * sizeof(std::wstring::value_type));
}

TEST(SystemLogEventUtilsTest, CopyWStringUnaligned) {
#if defined(ARCH_CPU_LITTLE_ENDIAN)
  static constexpr uint8_t kBytes[] = {0x00, 0x48, 0x00, 0x55, 0x00,
                                       0x4d, 0x00, 0x00, 0x00, 0x01};
#else
  static constexpr uint8_t kBytes[] = {0x00, 0x00, 0x48, 0x00, 0x55,
                                       0x00, 0x4d, 0x00, 0x00, 0x01};
#endif
  base::BufferIterator<const uint8_t> iterator(
      (base::span<const uint8_t>(kBytes)));
  ASSERT_NE(iterator.Object<uint8_t>(), nullptr);
  ASSERT_EQ(CopyWString(iterator), std::wstring(L"HUM"));
  ASSERT_EQ(iterator.position(), 1 + 4 * sizeof(std::wstring::value_type));
}

TEST(SystemLogEventUtilsTest, CopySidTooSmall) {
  // An encoded SID must be at least one uint32_t plus two pointers plus two
  // more uint8_ts.
  static constexpr size_t kMinSize =
      sizeof(uint32_t) + 2 * sizeof(void*) + 2 * sizeof(uint8_t);
  static constexpr uint8_t kBytes[kMinSize] = {};  // Contents are irrelevant.

  base::span<const uint8_t> byte_span(kBytes);
  for (size_t i = 0; i < byte_span.size(); ++i) {
    base::BufferIterator<const uint8_t> iterator(byte_span.subspan(0u, i));
    ASSERT_EQ(CopySid(sizeof(void*), iterator), std::nullopt);
    ASSERT_EQ(iterator.position(), 0u);
  }
}

TEST(SystemLogEventUtilsTest, CopySidInvalidSid) {
  // The MOF encoding of a sid, including the leading uint32_t and TOKEN_USER,
  // for which the contained SID is invalid (the version is 254).
  static constexpr uint8_t kBytes[] = {
      0x04, 0x00, 0x00, 0x00, 0x20, 0xA8, 0xA4, 0x5C, 0x86, 0xD1, 0xFF,
      0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFE, 0x01,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x12, 0x00, 0x00, 0x00};

  base::BufferIterator<const uint8_t> iterator(
      (base::span<const uint8_t>(kBytes)));
  ASSERT_EQ(CopySid(sizeof(void*), iterator), std::nullopt);
  ASSERT_EQ(iterator.position(), 0u);
}

TEST(SystemLogEventUtilsTest, CopySidSidTooBig) {
  // The MOF encoding of a sid, including the leading uint32_t and TOKEN_USER,
  // for which the contained SID is too long to fit in the buffer (the
  // SubAuthorityCount is two, but there is space for only one).
  static constexpr uint8_t kBytes[] = {
      0x04, 0x00, 0x00, 0x00, 0x20, 0xA8, 0xA4, 0x5C, 0x86, 0xD1, 0xFF,
      0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x12, 0x00, 0x00, 0x00};

  base::BufferIterator<const uint8_t> iterator(
      (base::span<const uint8_t>(kBytes)));
  ASSERT_EQ(CopySid(sizeof(void*), iterator), std::nullopt);
  ASSERT_EQ(iterator.position(), 0u);
}

TEST(SystemLogEventUtilsTest, CopySidx64) {
  // The MOF encoding of a sid, including the leading uint32_t and TOKEN_USER.
  static constexpr uint8_t kBytes[] = {
      0x04, 0x00, 0x00, 0x00, 0x20, 0xA8, 0xA4, 0x5C, 0x86, 0xD1, 0xFF,
      0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x12, 0x00, 0x00, 0x00};
  base::BufferIterator<const uint8_t> iterator(
      (base::span<const uint8_t>(kBytes)));
  ASSERT_EQ(
      CopySid(sizeof(uint64_t), iterator),
      base::win::Sid::FromKnownSid(base::win::WellKnownSid::kLocalSystem));
  ASSERT_EQ(iterator.position(), iterator.total_size());
}

TEST(SystemLogEventUtilsTest, CopyUserSidx64) {
  // The MOF encoding of a sid, including the leading uint32_t and TOKEN_USER.
  static constexpr uint8_t kBytes[] = {
      0x00, 0x00, 0x00, 0x00, 0x60, 0x82, 0x4D, 0x65, 0x88, 0xD1, 0xFF, 0xFF,
      0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x05, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x05, 0x15, 0x00, 0x00, 0x00, 0x98, 0x12, 0x57, 0x02,
      0xE2, 0x31, 0x50, 0x2C, 0xAA, 0x26, 0x7A, 0x08, 0x51, 0xF4, 0x05, 0x00};
  const base::span<const uint8_t> bytes_span(kBytes);
  base::BufferIterator<const uint8_t> iterator(bytes_span);
  ASSERT_NE(CopySid(sizeof(uint64_t), iterator), std::nullopt);
  ASSERT_EQ(iterator.position(), iterator.total_size());

  // Try again with a buffer that is one byte too short.
  base::BufferIterator<const uint8_t> iterator2(
      bytes_span.first(bytes_span.size() - 1));
  ASSERT_EQ(CopySid(sizeof(uint64_t), iterator2), std::nullopt);
  ASSERT_EQ(iterator2.position(), 0u);

  // Now try again with a larger buffer and be sure that the iterator doesn't go
  // too far.
  auto more_bytes = base::HeapArray<uint8_t>::Uninit(bytes_span.size() + 1);
  // Copy the data and set some byte at the end.
  *std::ranges::copy(bytes_span, more_bytes.begin()).out = 0x44;
  // Read out the sid.
  base::BufferIterator<const uint8_t> iterator3(more_bytes);
  ASSERT_NE(CopySid(sizeof(uint64_t), iterator3), std::nullopt);
  ASSERT_EQ(iterator3.position(), iterator.position());
  auto* last_byte = iterator3.Object<uint8_t>();
  ASSERT_NE(last_byte, nullptr);
  ASSERT_EQ(*last_byte, 0x44);
}

}  // namespace tracing
