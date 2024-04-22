// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/starboard/media/media/starboard_resampler.h"

#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

#include "chromecast/media/base/cast_decoder_buffer_impl.h"
#include "chromecast/starboard/chromecast/starboard_cast_api/cast_starboard_api_types.h"
#include "chromecast/starboard/media/media/starboard_api_wrapper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace media {
namespace {

using ::testing::FloatEq;
using ::testing::Pointwise;

TEST(StarboardResamplerTest, PCM8ToS16) {
  size_t out_size;
  const std::vector<uint8_t> buffer_data = {0, 0x80, 0xFF};
  const std::vector<int16_t> expected_s16_data = {static_cast<int16_t>(0x8000),
                                                  0, 0x7FFF};

  scoped_refptr<CastDecoderBufferImpl> buffer(
      new CastDecoderBufferImpl(sizeof(uint8_t) * buffer_data.size()));
  memcpy(buffer->writable_data(), buffer_data.data(),
         sizeof(uint8_t) * buffer_data.size());

  std::unique_ptr<uint8_t[]> ret_array = ResamplePCMAudioDataForStarboard(
      kStarboardPcmSampleFormatS16, kSampleFormatU8, kCodecPCM, 2,
      *buffer.get(), out_size);
  const std::vector<int16_t> resampled_buffer(
      reinterpret_cast<const int16_t*>(ret_array.get()),
      reinterpret_cast<const int16_t*>(ret_array.get()) + out_size / 2);

  EXPECT_EQ(resampled_buffer, expected_s16_data);
}

TEST(StarboardResamplerTest, PCM8ToS32) {
  size_t out_size;
  const std::vector<uint8_t> buffer_data = {0, 0x80, 0xFF};
  const std::vector<int32_t> expected_s32_data = {
      static_cast<int32_t>(0x80000000), 0, 0x7FFFFFFF};

  scoped_refptr<CastDecoderBufferImpl> buffer(
      new CastDecoderBufferImpl(sizeof(uint8_t) * buffer_data.size()));
  memcpy(buffer->writable_data(), buffer_data.data(),
         sizeof(uint8_t) * buffer_data.size());

  std::unique_ptr<uint8_t[]> ret_array = ResamplePCMAudioDataForStarboard(
      kStarboardPcmSampleFormatS32, kSampleFormatU8, kCodecPCM, 2,
      *buffer.get(), out_size);
  const std::vector<int32_t> resampled_buffer(
      reinterpret_cast<const int32_t*>(ret_array.get()),
      reinterpret_cast<const int32_t*>(ret_array.get()) + out_size / 4);

  EXPECT_EQ(resampled_buffer, expected_s32_data);
}

TEST(StarboardResamplerTest, PCMU8ToFloat) {
  size_t out_size;
  const std::vector<uint8_t> buffer_data = {1, 2, 3, 4};
  const std::vector<float> expected_f32_data = {-0.9921875, -0.984375,
                                                -0.9765625, -0.96875};

  scoped_refptr<CastDecoderBufferImpl> buffer(
      new CastDecoderBufferImpl(sizeof(uint8_t) * buffer_data.size()));
  memcpy(buffer->writable_data(), buffer_data.data(),
         sizeof(uint8_t) * buffer_data.size());

  std::unique_ptr<uint8_t[]> ret_array = ResamplePCMAudioDataForStarboard(
      kStarboardPcmSampleFormatF32, kSampleFormatU8, kCodecPCM, 2,
      *buffer.get(), out_size);
  const std::vector<float> resampled_buffer(
      reinterpret_cast<const float*>(ret_array.get()),
      reinterpret_cast<const float*>(ret_array.get()) + out_size / 4);

  EXPECT_THAT(resampled_buffer, Pointwise(FloatEq(), expected_f32_data));
}

////// END OF U8 TO OTHER FORMATS
////// START OF S16 TO OTHER FORMATS

TEST(StarboardResamplerTest, PCMS16ToS16) {
  size_t out_size;
  const std::vector<int16_t> buffer_data = {-32768, 0, 32767};
  const std::vector<int16_t> expected_s16_data = {-32768, 0, 32767};

  scoped_refptr<CastDecoderBufferImpl> buffer(
      new CastDecoderBufferImpl(sizeof(int16_t) * buffer_data.size()));
  memcpy(buffer->writable_data(), buffer_data.data(),
         sizeof(int16_t) * buffer_data.size());

  std::unique_ptr<uint8_t[]> ret_array = ResamplePCMAudioDataForStarboard(
      kStarboardPcmSampleFormatS16, kSampleFormatS16, kCodecPCM, 2,
      *buffer.get(), out_size);
  const std::vector<int16_t> resampled_buffer(
      reinterpret_cast<const int16_t*>(ret_array.get()),
      reinterpret_cast<const int16_t*>(ret_array.get()) + out_size / 2);

  EXPECT_EQ(resampled_buffer, expected_s16_data);
}

TEST(StarboardResamplerTest, PCMS16ToS32) {
  size_t out_size;
  const std::vector<int16_t> buffer_data = {static_cast<int16_t>(0x8000), 0,
                                            0x7FFF};
  const std::vector<int32_t> expected_s32_data = {
      static_cast<int32_t>(0x80000000), 0, 0x7FFFFFFF};

  scoped_refptr<CastDecoderBufferImpl> buffer(
      new CastDecoderBufferImpl(sizeof(int16_t) * buffer_data.size()));
  memcpy(buffer->writable_data(), buffer_data.data(),
         sizeof(int16_t) * buffer_data.size());

  std::unique_ptr<uint8_t[]> ret_array = ResamplePCMAudioDataForStarboard(
      kStarboardPcmSampleFormatS32, kSampleFormatS16, kCodecPCM, 2,
      *buffer.get(), out_size);
  const std::vector<int32_t> resampled_buffer(
      reinterpret_cast<const int32_t*>(ret_array.get()),
      reinterpret_cast<const int32_t*>(ret_array.get()) + out_size / 4);

  EXPECT_EQ(resampled_buffer, expected_s32_data);
}

TEST(StarboardResamplerTest, PCMS16ToFloat) {
  size_t out_size;
  const std::vector<int16_t> buffer_data = {static_cast<int16_t>(0x8000),
                                            static_cast<int16_t>(0xC000), 0,
                                            0x3FFF, 0x7FFF};
  const std::vector<float> expected_f32_data = {-1.0f, -0.5f, 0.0f,
                                                0.499984741f, 1.0f};

  scoped_refptr<CastDecoderBufferImpl> buffer(
      new CastDecoderBufferImpl(sizeof(int16_t) * buffer_data.size()));
  memcpy(buffer->writable_data(), buffer_data.data(),
         sizeof(int16_t) * buffer_data.size());

  std::unique_ptr<uint8_t[]> ret_array = ResamplePCMAudioDataForStarboard(
      kStarboardPcmSampleFormatF32, kSampleFormatS16, kCodecPCM, 2,
      *buffer.get(), out_size);
  const std::vector<float> resampled_buffer(
      reinterpret_cast<const float*>(ret_array.get()),
      reinterpret_cast<const float*>(ret_array.get()) + out_size / 4);

  EXPECT_THAT(resampled_buffer, Pointwise(FloatEq(), expected_f32_data));
}

// End of Signed 16 Conversions
// Start of Signed 24 conversions

TEST(StarboardResamplerTest, PCMS24ToS16) {
  size_t out_size;
  const std::vector<uint8_t> buffer_data = {0xCC, 0xAA, 0xBB, 0,   0,
                                            0,    0xFF, 0xFF, 0x7F};
  const std::vector<int16_t> expected_s16_data = {-17493, 0, 32767};

  scoped_refptr<CastDecoderBufferImpl> buffer(
      new CastDecoderBufferImpl(buffer_data.size()));
  memcpy(buffer->writable_data(), buffer_data.data(), buffer_data.size());

  std::unique_ptr<uint8_t[]> ret_array = ResamplePCMAudioDataForStarboard(
      kStarboardPcmSampleFormatS16, kSampleFormatS24, kCodecPCM, 2,
      *buffer.get(), out_size);
  const std::vector<int16_t> resampled_buffer(
      reinterpret_cast<const int16_t*>(ret_array.get()),
      reinterpret_cast<const int16_t*>(ret_array.get()) + out_size / 2);

  EXPECT_EQ(resampled_buffer, expected_s16_data);
}

TEST(StarboardResamplerTest, PCMS24ToS32) {
  size_t out_size;
  // Represents {0x800000, 0x000000, 0x7FFFFF}, since it's in Little Endian byte
  // order.
  const std::vector<uint8_t> buffer_data = {0, 0,    0x80, 0,   0,
                                            0, 0xFF, 0xFF, 0x7F};
  const std::vector<int32_t> expected_s32_data = {
      static_cast<int32_t>(0x80000000), 0, 0x7FFFFFFF};

  scoped_refptr<CastDecoderBufferImpl> buffer(
      new CastDecoderBufferImpl(buffer_data.size()));
  memcpy(buffer->writable_data(), buffer_data.data(), buffer_data.size());

  std::unique_ptr<uint8_t[]> ret_array = ResamplePCMAudioDataForStarboard(
      kStarboardPcmSampleFormatS32, kSampleFormatS24, kCodecPCM, 2,
      *buffer.get(), out_size);
  const std::vector<int32_t> resampled_buffer(
      reinterpret_cast<const int32_t*>(ret_array.get()),
      reinterpret_cast<const int32_t*>(ret_array.get()) + out_size / 4);

  EXPECT_EQ(resampled_buffer, expected_s32_data);
}

TEST(StarboardResamplerTest, PCMS24ToFloat) {
  size_t out_size;
  // Represents {0x800000, 0x000000, 0x7FFFFF}, since it's in Little Endian byte
  // order.
  const std::vector<uint8_t> buffer_data = {0, 0,    0x80, 0,   0,
                                            0, 0xFF, 0xFF, 0x7F};
  const std::vector<float> expected_f32_data = {-1.0f, 0.0f, 1.0f};

  scoped_refptr<CastDecoderBufferImpl> buffer(
      new CastDecoderBufferImpl(buffer_data.size()));
  memcpy(buffer->writable_data(), buffer_data.data(), buffer_data.size());

  std::unique_ptr<uint8_t[]> ret_array = ResamplePCMAudioDataForStarboard(
      kStarboardPcmSampleFormatF32, kSampleFormatS24, kCodecPCM, 2,
      *buffer.get(), out_size);
  const std::vector<float> resampled_buffer(
      reinterpret_cast<const float*>(ret_array.get()),
      reinterpret_cast<const float*>(ret_array.get()) + out_size / 4);

  EXPECT_THAT(resampled_buffer, Pointwise(FloatEq(), expected_f32_data));
}

// End of Signed 24 Conversions
// Start of Signed 32 conversions

TEST(StarboardResamplerTest, PCMS32ToS16) {
  size_t out_size;
  const std::vector<int32_t> buffer_data = {-2147483648, 0, 70000, 2147483647};
  const std::vector<int16_t> expected_s16_data = {-32768, 0, 1, 32767};

  scoped_refptr<CastDecoderBufferImpl> buffer(
      new CastDecoderBufferImpl(sizeof(int32_t) * buffer_data.size()));
  memcpy(buffer->writable_data(), buffer_data.data(),
         sizeof(int32_t) * buffer_data.size());

  std::unique_ptr<uint8_t[]> ret_array = ResamplePCMAudioDataForStarboard(
      kStarboardPcmSampleFormatS16, kSampleFormatS32, kCodecPCM, 2,
      *buffer.get(), out_size);
  const std::vector<int16_t> resampled_buffer(
      reinterpret_cast<const int16_t*>(ret_array.get()),
      reinterpret_cast<const int16_t*>(ret_array.get()) + 4);

  EXPECT_EQ(resampled_buffer, expected_s16_data);
}

TEST(StarboardResamplerTest, PCMS32ToS32) {
  size_t out_size;
  const std::vector<int32_t> buffer_data = {-2147483647, 0, 2147483647};
  const std::vector<int32_t> expected_s32_data = {-2147483647, 0, 2147483647};

  scoped_refptr<CastDecoderBufferImpl> buffer(
      new CastDecoderBufferImpl(sizeof(int32_t) * buffer_data.size()));
  memcpy(buffer->writable_data(), buffer_data.data(),
         sizeof(int32_t) * buffer_data.size());

  std::unique_ptr<uint8_t[]> ret_array = ResamplePCMAudioDataForStarboard(
      kStarboardPcmSampleFormatS32, kSampleFormatS32, kCodecPCM, 2,
      *buffer.get(), out_size);
  const std::vector<int32_t> resampled_buffer(
      reinterpret_cast<const int32_t*>(ret_array.get()),
      reinterpret_cast<const int32_t*>(ret_array.get()) + out_size / 4);

  EXPECT_EQ(resampled_buffer, expected_s32_data);
}

TEST(StarboardResamplerTest, PCMS32ToFloat) {
  size_t out_size;
  const std::vector<int32_t> buffer_data = {static_cast<int32_t>(0x80000000), 0,
                                            0x7FFFFFFF};
  const std::vector<float> expected_f32_data = {-1.0f, 0.0f, 1.0f};

  scoped_refptr<CastDecoderBufferImpl> buffer(
      new CastDecoderBufferImpl(sizeof(int32_t) * buffer_data.size()));
  memcpy(buffer->writable_data(), buffer_data.data(),
         sizeof(int32_t) * buffer_data.size());

  std::unique_ptr<uint8_t[]> ret_array = ResamplePCMAudioDataForStarboard(
      kStarboardPcmSampleFormatF32, kSampleFormatS32, kCodecPCM, 2,
      *buffer.get(), out_size);
  const std::vector<float> resampled_buffer(
      reinterpret_cast<const float*>(ret_array.get()),
      reinterpret_cast<const float*>(ret_array.get()) + out_size / 4);

  EXPECT_THAT(resampled_buffer, Pointwise(FloatEq(), expected_f32_data));
}

// End of Signed 32 Conversions
// Start of Float conversions

TEST(StarboardResamplerTest, PCMFloatToS16) {
  size_t out_size;
  const std::vector<float> buffer_data = {-1.0f, 0.0f, 1.0f};
  const std::vector<int16_t> expected_s16_data = {-32768, 0, 32767};

  scoped_refptr<CastDecoderBufferImpl> buffer(
      new CastDecoderBufferImpl(sizeof(float) * buffer_data.size()));
  memcpy(buffer->writable_data(), buffer_data.data(),
         sizeof(float) * buffer_data.size());

  std::unique_ptr<uint8_t[]> ret_array = ResamplePCMAudioDataForStarboard(
      kStarboardPcmSampleFormatS16, kSampleFormatF32, kCodecPCM, 2,
      *buffer.get(), out_size);
  const std::vector<int16_t> resampled_buffer(
      reinterpret_cast<const int16_t*>(ret_array.get()),
      reinterpret_cast<const int16_t*>(ret_array.get()) + out_size / 2);

  EXPECT_EQ(resampled_buffer, expected_s16_data);
}

TEST(StarboardResamplerTest, PCMFloatToS32) {
  size_t out_size;
  const std::vector<float> buffer_data = {-1.0f, 0.0f, 1.0f};
  const std::vector<int32_t> expected_s32_data = {-2147483648, 0, 2147483647};

  scoped_refptr<CastDecoderBufferImpl> buffer(
      new CastDecoderBufferImpl(sizeof(float) * buffer_data.size()));
  memcpy(buffer->writable_data(), buffer_data.data(),
         sizeof(float) * buffer_data.size());

  std::unique_ptr<uint8_t[]> ret_array = ResamplePCMAudioDataForStarboard(
      kStarboardPcmSampleFormatS32, kSampleFormatF32, kCodecPCM, 2,
      *buffer.get(), out_size);
  const std::vector<int32_t> resampled_buffer(
      reinterpret_cast<const int32_t*>(ret_array.get()),
      reinterpret_cast<const int32_t*>(ret_array.get()) + out_size / 4);

  EXPECT_EQ(resampled_buffer, expected_s32_data);
}

TEST(StarboardResamplerTest, PCMFloatToFloat) {
  size_t out_size;
  const std::vector<float> buffer_data = {-1, 0.0234375, 0, 1};
  const std::vector<float> expected_f32_data = {-1, 0.0234375, 0, 1};

  scoped_refptr<CastDecoderBufferImpl> buffer(
      new CastDecoderBufferImpl(sizeof(float) * buffer_data.size()));
  memcpy(buffer->writable_data(), buffer_data.data(),
         sizeof(float) * buffer_data.size());

  std::unique_ptr<uint8_t[]> ret_array = ResamplePCMAudioDataForStarboard(
      kStarboardPcmSampleFormatF32, kSampleFormatF32, kCodecPCM, 2,
      *buffer.get(), out_size);
  const std::vector<float> resampled_buffer(
      reinterpret_cast<const float*>(ret_array.get()),
      reinterpret_cast<const float*>(ret_array.get()) + out_size / 4);

  EXPECT_THAT(resampled_buffer, Pointwise(FloatEq(), expected_f32_data));
}

// End of Float Conversions
// Start of Planar conversions

TEST(StarboardResamplerTest, PushesBufferToStarboardPlanarPCM16) {
  size_t out_size;
  const std::vector<int16_t> buffer_data = {-32768, -32768, 16384,
                                            16384,  32767,  32767};
  const std::vector<int16_t> expected_s16_data = {-32768, 16384, 32767,
                                                  -32768, 16384, 32767};

  scoped_refptr<CastDecoderBufferImpl> buffer(
      new CastDecoderBufferImpl(sizeof(int16_t) * buffer_data.size()));
  memcpy(buffer->writable_data(), buffer_data.data(),
         sizeof(int16_t) * buffer_data.size());

  std::unique_ptr<uint8_t[]> ret_array = ResamplePCMAudioDataForStarboard(
      kStarboardPcmSampleFormatS16, kSampleFormatPlanarS16, kCodecPCM, 3,
      *buffer.get(), out_size);

  const std::vector<int16_t> resampled_buffer(
      reinterpret_cast<const int16_t*>(ret_array.get()),
      reinterpret_cast<const int16_t*>(ret_array.get()) + out_size / 2);

  EXPECT_EQ(resampled_buffer, expected_s16_data);
}

TEST(StarboardResamplerTest, PushesBufferToStarboardPlanarPCM32) {
  size_t out_size;
  const std::vector<int32_t> buffer_data = {-2147483648, -2147483648, 1000000,
                                            1000000,     2000000,     2000000,
                                            2147483647,  2147483647};
  const std::vector<int16_t> expected_s16_data = {-32768, 15, 31, 32767,
                                                  -32768, 15, 31, 32767};

  scoped_refptr<CastDecoderBufferImpl> buffer(
      new CastDecoderBufferImpl(sizeof(int32_t) * buffer_data.size()));
  memcpy(buffer->writable_data(), buffer_data.data(),
         sizeof(int32_t) * buffer_data.size());

  std::unique_ptr<uint8_t[]> ret_array = ResamplePCMAudioDataForStarboard(
      kStarboardPcmSampleFormatS16, kSampleFormatPlanarS32, kCodecPCM, 4,
      *buffer.get(), out_size);
  const std::vector<int16_t> resampled_buffer(
      reinterpret_cast<const int16_t*>(ret_array.get()),
      reinterpret_cast<const int16_t*>(ret_array.get()) + out_size / 2);

  EXPECT_EQ(resampled_buffer, expected_s16_data);
}

TEST(StarboardResamplerTest, PushesBufferToStarboardPlanarPCMF32) {
  size_t out_size;
  const std::vector<float> buffer_data = {-1,          -1, 0.000465661,
                                          0.000465661, 1,  1};
  const std::vector<int16_t> expected_s16_data = {-32768, 15, 32767,
                                                  -32768, 15, 32767};

  scoped_refptr<CastDecoderBufferImpl> buffer(
      new CastDecoderBufferImpl(sizeof(float) * buffer_data.size()));
  memcpy(buffer->writable_data(), buffer_data.data(),
         sizeof(float) * buffer_data.size());

  std::unique_ptr<uint8_t[]> ret_array = ResamplePCMAudioDataForStarboard(
      kStarboardPcmSampleFormatS16, kSampleFormatPlanarF32, kCodecPCM, 3,
      *buffer.get(), out_size);
  const std::vector<int16_t> resampled_buffer(
      reinterpret_cast<const int16_t*>(ret_array.get()),
      reinterpret_cast<const int16_t*>(ret_array.get()) + out_size / 2);

  EXPECT_EQ(resampled_buffer, expected_s16_data);
}

TEST(StarboardResamplerTest, PushesBufferToStarboardPlanarPCM32Mono) {
  size_t out_size;
  // Everything should be shifted down by 16 bits.
  const std::vector<int32_t> buffer_data = {static_cast<int32_t>(0x80000000),
                                            0x1000000,
                                            0x2000000,
                                            0x3000000,
                                            0x4000000,
                                            0x5000000,
                                            0x6000000,
                                            0x7000000,
                                            0x8000000,
                                            0x9000000,
                                            0x7FFFFFFF};
  const std::vector<int16_t> expected_s16_data = {static_cast<int16_t>(0x8000),
                                                  0x100,
                                                  0x200,
                                                  0x300,
                                                  0x400,
                                                  0x500,
                                                  0x600,
                                                  0x700,
                                                  0x800,
                                                  0x900,
                                                  0x7FFF};

  scoped_refptr<CastDecoderBufferImpl> buffer(
      new CastDecoderBufferImpl(sizeof(int32_t) * buffer_data.size()));
  memcpy(buffer->writable_data(), buffer_data.data(),
         sizeof(int32_t) * buffer_data.size());

  std::unique_ptr<uint8_t[]> ret_array = ResamplePCMAudioDataForStarboard(
      kStarboardPcmSampleFormatS16, kSampleFormatPlanarS32, kCodecPCM, 1,
      *buffer.get(), out_size);
  const std::vector<int16_t> resampled_buffer(
      reinterpret_cast<const int16_t*>(ret_array.get()),
      reinterpret_cast<const int16_t*>(ret_array.get()) + out_size / 2);

  EXPECT_EQ(resampled_buffer, expected_s16_data);
}

TEST(StarboardResamplerTest, PushesBufferToStarboardPlanarPCM32MaxChannels) {
  size_t out_size;
  // Everything should be shifted down by 16 bits.
  const std::vector<int32_t> buffer_data = {
      0,         0,         0x1000000, 0x1000000, 0x2000000, 0x2000000,
      0x3000000, 0x3000000, 0x4000000, 0x4000000, 0x5000000, 0x5000000,
      0x6000000, 0x6000000, 0x7000000, 0x7000000};
  const std::vector<int16_t> expected_s16_data = {
      0x0, 0x100, 0x200, 0x300, 0x400, 0x500, 0x600, 0x700,
      0x0, 0x100, 0x200, 0x300, 0x400, 0x500, 0x600, 0x700};

  scoped_refptr<CastDecoderBufferImpl> buffer(
      new CastDecoderBufferImpl(sizeof(int32_t) * buffer_data.size()));
  memcpy(buffer->writable_data(), buffer_data.data(),
         sizeof(int32_t) * buffer_data.size());

  std::unique_ptr<uint8_t[]> ret_array = ResamplePCMAudioDataForStarboard(
      kStarboardPcmSampleFormatS16, kSampleFormatPlanarS32, kCodecPCM, 8,
      *buffer.get(), out_size);

  const std::vector<int16_t> resampled_buffer(
      reinterpret_cast<const int16_t*>(ret_array.get()),
      reinterpret_cast<const int16_t*>(ret_array.get()) + out_size / 2);

  EXPECT_EQ(resampled_buffer, expected_s16_data);
}

TEST(StarboardResamplerTest,
     PushesBufferToStarboardPCM16BigEndianToLittleEndian) {
  size_t out_size;
  const std::vector<int16_t> buffer_data = {32640, 1040, 4100, 255};
  const std::vector<int16_t> expected_s16_data = {-32641, 4100, 1040, -256};

  scoped_refptr<CastDecoderBufferImpl> buffer(
      new CastDecoderBufferImpl(sizeof(int16_t) * buffer_data.size()));
  memcpy(buffer->writable_data(), buffer_data.data(),
         sizeof(int16_t) * buffer_data.size());

  std::unique_ptr<uint8_t[]> ret_array = ResamplePCMAudioDataForStarboard(
      kStarboardPcmSampleFormatS16, kSampleFormatS16, kCodecPCM_S16BE, 2,
      *buffer.get(), out_size);
  const std::vector<int16_t> resampled_buffer(
      reinterpret_cast<const int16_t*>(ret_array.get()),
      reinterpret_cast<const int16_t*>(ret_array.get()) + out_size / 2);

  EXPECT_EQ(resampled_buffer, expected_s16_data);
}

// End of Signed 16 Conversions

}  // namespace
}  // namespace media
}  // namespace chromecast
