// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/webshare/win/fake_buffer.h"

#include <wrl/implements.h>

#include "testing/gtest/include/gtest/gtest-spi.h"
#include "testing/gtest/include/gtest/gtest.h"

using Microsoft::WRL::Make;

namespace webshare {

TEST(FakeBufferTest, Length) {
  auto buffer = Make<FakeBuffer>(47);

  UINT32 capacity;
  ASSERT_HRESULT_SUCCEEDED(buffer->get_Capacity(&capacity));
  ASSERT_EQ(capacity, 47u);

  UINT32 length;
  ASSERT_HRESULT_SUCCEEDED(buffer->get_Length(&length));
  ASSERT_EQ(length, 0u);

  ASSERT_HRESULT_SUCCEEDED(buffer->put_Length(23));
  ASSERT_HRESULT_SUCCEEDED(buffer->get_Length(&length));
  ASSERT_EQ(length, 23u);

  ASSERT_HRESULT_SUCCEEDED(buffer->put_Length(47));
  ASSERT_HRESULT_SUCCEEDED(buffer->get_Length(&length));
  ASSERT_EQ(length, 47u);

  EXPECT_NONFATAL_FAILURE(ASSERT_HRESULT_FAILED(buffer->put_Length(48)),
                          "put_Length");
}

TEST(FakeBufferTest, Bytes) {
  auto buffer = Make<FakeBuffer>(2);

  byte* raw_buffer;
  ASSERT_HRESULT_SUCCEEDED(buffer->Buffer(&raw_buffer));

  raw_buffer[0] = 'a';
  raw_buffer[1] = 'b';

  auto buffer2 = buffer;
  byte* raw_buffer_2;
  ASSERT_HRESULT_SUCCEEDED(buffer2->Buffer(&raw_buffer_2));

  ASSERT_EQ(raw_buffer, raw_buffer_2);
  ASSERT_EQ(raw_buffer_2[0], 'a');
  ASSERT_EQ(raw_buffer_2[1], 'b');
}

}  // namespace webshare
