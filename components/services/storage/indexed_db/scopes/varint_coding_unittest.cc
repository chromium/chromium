// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/indexed_db/scopes/varint_coding.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

static std::string WrappedEncodeVarInt(int64_t value) {
  std::string buffer;
  EncodeVarInt(value, &buffer);
  return buffer;
}

TEST(VarIntCoding, Encode) {
  EXPECT_EQ(1u, WrappedEncodeVarInt(0).size());
  EXPECT_EQ(1u, WrappedEncodeVarInt(1).size());
  EXPECT_EQ(2u, WrappedEncodeVarInt(255).size());
  EXPECT_EQ(2u, WrappedEncodeVarInt(256).size());
  EXPECT_EQ(5u, WrappedEncodeVarInt(0xffffffff).size());
  EXPECT_EQ(8u, WrappedEncodeVarInt(0xfffffffffffffll).size());
  EXPECT_EQ(9u, WrappedEncodeVarInt(0x7fffffffffffffffll).size());
#if !DCHECK_IS_ON()
  EXPECT_EQ(10u, WrappedEncodeVarInt(-100).size());
#endif
}

TEST(VarIntCoding, Decode) {
  std::vector<int64_t> test_cases = {
    0,
    1,
    255,
    256,
    65535,
    655536,
    7711192431755665792ll,
    0x7fffffffffffffffll,
#if !DCHECK_IS_ON()
    -3,
#endif
  };

  for (size_t i = 0; i < test_cases.size(); ++i) {
    int64_t n = test_cases[i];
    std::string v = WrappedEncodeVarInt(n);
    ASSERT_GT(v.size(), 0u);
    base::StringPiece slice(v);
    int64_t res;
    EXPECT_TRUE(DecodeVarInt(&slice, &res));
    EXPECT_EQ(n, res);
    EXPECT_TRUE(slice.empty());

    slice = base::StringPiece(&*v.begin(), v.size() - 1);
    EXPECT_FALSE(DecodeVarInt(&slice, &res));

    slice = base::StringPiece(&*v.begin(), static_cast<size_t>(0));
    EXPECT_FALSE(DecodeVarInt(&slice, &res));

    // Verify decoding at an offset, to detect unaligned memory access.
    v.insert(v.begin(), 1u, static_cast<char>(0));
    slice = base::StringPiece(&*v.begin() + 1, v.size() - 1);
    EXPECT_TRUE(DecodeVarInt(&slice, &res));
    EXPECT_EQ(n, res);
    EXPECT_TRUE(slice.empty());
  }
}

TEST(VarIntCoding, SingleByteCases) {
  std::vector<unsigned char> test_cases = {0, 1, 127};

  for (size_t i = 0; i < test_cases.size(); ++i) {
    unsigned char n = test_cases[i];

    std::string a = std::string(1, n);
    std::string b = WrappedEncodeVarInt(static_cast<int64_t>(n));

    EXPECT_EQ(a.size(), b.size());
    EXPECT_EQ(*a.begin(), *b.begin());
  }
}

}  // namespace
}  // namespace content
