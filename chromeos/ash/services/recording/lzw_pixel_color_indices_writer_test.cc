// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/recording/lzw_pixel_color_indices_writer.h"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "chromeos/ash/services/recording/gif_file_writer.h"
#include "chromeos/ash/services/recording/public/mojom/recording_service.mojom.h"
#include "chromeos/ash/services/recording/recording_file_io_helper.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace recording {

class FakeDelegate : public RecordingFileIoHelper::Delegate {
 public:
  FakeDelegate() = default;
  FakeDelegate(const FakeDelegate&) = delete;
  FakeDelegate& operator=(const FakeDelegate&) = delete;
  ~FakeDelegate() override = default;

  // RecordingFileIoHelper::Delegate:
  void NotifyFailure(mojom::RecordingStatus status) override {}
};

class LzwTest : public testing::Test {
 public:
  LzwTest() {
    const bool dir_created = temp_dir_.CreateUniqueTempDir();
    DCHECK(dir_created);
  }
  LzwTest(const LzwTest&) = delete;
  LzwTest& operator=(const LzwTest&) = delete;
  ~LzwTest() override = default;

  // Returns a path for the given `file_name` under the temp dir created by this
  // fixture.
  base::FilePath GetPathInTempDir(const std::string& file_name) {
    return temp_dir_.GetPath().Append(file_name);
  }

 private:
  base::ScopedTempDir temp_dir_;
};

TEST_F(LzwTest, VerifyOutputStream) {
  const auto gif_path = GetPathInTempDir("test.gif");
  FakeDelegate delegate;
  GifFileWriter gif_file_writer(
      mojo::PendingRemote<mojom::DriveFsQuotaDelegate>(), gif_path, &delegate);
  LzwPixelColorIndicesWriter lzw_encoder(&gif_file_writer);

  // Let's assume we have the following image with only 3 colors; red, green,
  // and blue.
  //
  // +---+---+---+
  // | R | R | R |
  // +---+---+---+
  // | G | G | G |
  // +---+---+---+
  // | B | B | B |
  // +---+---+---+
  //
  // This means we only have 3 color indices; R => 0, G => 1, B => 2. The bit
  // depth in this case is 2. Let's feed this pixel color indices to the LZW
  // encoder.
  const ColorIndices color_indices{0, 0, 0, 1, 1, 1, 2, 2, 2};
  lzw_encoder.EncodeAndWrite(color_indices, /*color_bit_depth=*/2);

  // Verify that the contents of the file are the expected output of the
  // encoder.
  std::optional<std::vector<uint8_t>> actual_file_contents =
      base::ReadFileToBytes(gif_path);
  ASSERT_TRUE(actual_file_contents.has_value());
  EXPECT_THAT(
      *actual_file_contents,
      testing::ElementsAre(0x02,    // LZW minimum code size.
                           0x04,    // Number of bytes in the data sub-block.
                           0x84,    // -+
                           0x83,    //  +-- LZW-compressed indices.
                           0xA2,    // -+
                           0x54,    // Clear code + EoI code.
                           0x00));  // Block terminator.
}

TEST_F(LzwTest, ReusesTableAcrossFrames) {
  FakeDelegate delegate;

  const auto multi_frame_path = GetPathInTempDir("multi_frame.gif");
  GifFileWriter multi_frame_writer(
      mojo::PendingRemote<mojom::DriveFsQuotaDelegate>(), multi_frame_path,
      &delegate);
  LzwPixelColorIndicesWriter lzw_encoder(&multi_frame_writer);

  // Encode a first frame to populate `code_table_` with multi-pixel patterns.
  const ColorIndices first_frame{3, 2, 1, 0, 3, 2, 1, 0, 2, 2, 1, 1, 0, 0};
  lzw_encoder.EncodeAndWrite(first_frame, /*color_bit_depth=*/2);
  std::optional<std::vector<uint8_t>> first_frame_bytes =
      base::ReadFileToBytes(multi_frame_path);
  ASSERT_TRUE(first_frame_bytes.has_value());

  // Encode a second frame on the same `lzw_encoder` instance and verify the
  // appended bytes match a fresh encoder with no prior table state.
  const ColorIndices second_frame{0, 0, 0, 1, 1, 1, 2, 2, 2};
  lzw_encoder.EncodeAndWrite(second_frame, /*color_bit_depth=*/2);
  std::optional<std::vector<uint8_t>> both_frames_bytes =
      base::ReadFileToBytes(multi_frame_path);
  ASSERT_TRUE(both_frames_bytes.has_value());
  ASSERT_GT(both_frames_bytes->size(), first_frame_bytes->size());

  const base::span<const uint8_t> second_frame_bytes =
      base::span(*both_frames_bytes).subspan(first_frame_bytes->size());
  EXPECT_THAT(second_frame_bytes,
              testing::ElementsAre(0x02, 0x04, 0x84, 0x83, 0xA2, 0x54, 0x00));
}

TEST_F(LzwTest, ResetsTableWhenExceedingMaxCodes) {
  const auto gif_path = GetPathInTempDir("overflow.gif");
  FakeDelegate delegate;
  GifFileWriter gif_file_writer(
      mojo::PendingRemote<mojom::DriveFsQuotaDelegate>(), gif_path, &delegate);
  LzwPixelColorIndicesWriter lzw_encoder(&gif_file_writer);

  // Build a deterministic 256-color stream large enough to fill and reset the
  // 4096-code LZW dictionary multiple times within a single frame.
  ColorIndices input(20000);
  uint32_t rng = 0x9E3779B9u;
  for (ColorIndex& pixel : input) {
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    pixel = static_cast<ColorIndex>(rng & 0xFF);
  }

  lzw_encoder.EncodeAndWrite(input, /*color_bit_depth=*/8);

  std::optional<std::vector<uint8_t>> encoded = base::ReadFileToBytes(gif_path);
  ASSERT_TRUE(encoded.has_value());
  ASSERT_GE(encoded->size(), 3u);
  EXPECT_EQ((*encoded)[0], 8u);
  EXPECT_EQ(encoded->back(), 0x00u);

  // Unpack GIF data sub-blocks into a contiguous bitstream buffer.
  std::vector<uint8_t> bitstream;
  size_t pos = 1;
  while (pos < encoded->size() && (*encoded)[pos] != 0x00) {
    const uint8_t sub_block_size = (*encoded)[pos++];
    ASSERT_LE(pos + sub_block_size, encoded->size());
    const base::span<const uint8_t> sub_block =
        base::span(*encoded).subspan(pos, sub_block_size);
    bitstream.insert(bitstream.end(), sub_block.begin(), sub_block.end());
    pos += sub_block_size;
  }
  EXPECT_EQ(pos + 1, encoded->size());

  // Decode the variable-length GIF LZW code stream and verify lossless round-
  // trip across mid-frame 4096-code dictionary resets.
  constexpr LzwCode kClearCode = 256;
  constexpr LzwCode kEoiCode = 257;
  constexpr LzwCode kMaxCodes = 4096;

  size_t bit_offset = 0;
  auto read_code = [&](uint8_t bit_width) -> std::optional<LzwCode> {
    if (bit_offset + bit_width > bitstream.size() * 8) {
      return std::nullopt;
    }
    LzwCode value = 0;
    for (uint8_t b = 0; b < bit_width; ++b) {
      const size_t idx = bit_offset + b;
      if ((bitstream[idx / 8] >> (idx % 8)) & 0x01) {
        value |= static_cast<LzwCode>(1u << b);
      }
    }
    bit_offset += bit_width;
    return value;
  };

  std::vector<ColorIndices> dictionary;
  uint8_t code_bit_size = 9;
  int clear_count = 0;
  auto reset_dictionary = [&]() {
    dictionary.resize(kEoiCode + 1);
    for (size_t c = 0; c < kClearCode; ++c) {
      dictionary[c] = {static_cast<ColorIndex>(c)};
    }
    code_bit_size = 9;
    ++clear_count;
  };

  ColorIndices decoded;
  decoded.reserve(input.size());
  std::optional<LzwCode> prev_code;
  while (std::optional<LzwCode> code = read_code(code_bit_size)) {
    if (*code == kClearCode) {
      reset_dictionary();
      prev_code.reset();
      continue;
    }
    if (*code == kEoiCode) {
      break;
    }
    ASSERT_GT(clear_count, 0);

    ColorIndices entry;
    if (*code < dictionary.size()) {
      entry = dictionary[*code];
    } else {
      ASSERT_TRUE(prev_code.has_value());
      ASSERT_EQ(*code, dictionary.size());
      entry = dictionary[*prev_code];
      entry.push_back(entry.front());
    }

    decoded.insert(decoded.end(), entry.begin(), entry.end());
    if (prev_code.has_value() && dictionary.size() < kMaxCodes) {
      ColorIndices new_entry = dictionary[*prev_code];
      new_entry.push_back(entry.front());
      dictionary.push_back(std::move(new_entry));
      if (dictionary.size() >= (1u << code_bit_size) && code_bit_size < 12) {
        ++code_bit_size;
      }
    }
    prev_code = *code;
  }

  // Expect initial clear + multiple mid-frame 4096-code clears + trailing
  // clear.
  EXPECT_GE(clear_count, 4);
  EXPECT_EQ(decoded, input);
}

}  // namespace recording
