// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/recording/lzw_pixel_color_indices_writer.h"

#include <cstdint>

#include "chromeos/ash/services/recording/gif_file_writer.h"

namespace recording {

namespace {

// The GIF specs specify a maximum of 12 bits per LZW compression code, which
// means that the maximum possible value for the codes is (2 ^ 12) - 1, which is
// equal to 4095, and the maximum number of codes is (2 ^ 12 = 4096).
constexpr LzwCode kMaxNumberOfLzwCodes = 1 << 12;

// The maximum number of bytes contained in a data sub block of the LZW encoded
// output.
constexpr uint8_t kMaxBytesPerDataSubBlock = 0xFF;

}  // namespace

LzwPixelColorIndicesWriter::LzwPixelColorIndicesWriter(
    GifFileWriter* gif_file_writer)
    : gif_file_writer_(gif_file_writer) {
  code_table_.reserve(kMaxNumberOfLzwCodes);
  byte_stream_buffer_.reserve(kMaxBytesPerDataSubBlock);
}

LzwPixelColorIndicesWriter::~LzwPixelColorIndicesWriter() = default;

void LzwPixelColorIndicesWriter::EncodeAndWrite(
    const ColorIndices& pixel_color_indices,
    uint8_t color_bit_depth) {
  if (pixel_color_indices.empty()) {
    return;
  }

  // Start the process with an empty table.
  code_table_.clear();

  // The `color_bit_depth` is the minimum number of bits needed to represent all
  // the indices in `pixel_color_indices`. For example, if we have 4 colors,
  // with indices 0, 1, 2, 3, The minimum number of bits needed to represent the
  // largest index (3) is 2 bits. This is the `color_bit_depth` and it's also
  // the value of the LZW minimum code size, and is required to be written to
  // the file as the first byte of the image data block.
  const LzwCode lzw_minimum_code_size = color_bit_depth;
  gif_file_writer_->WriteByte(lzw_minimum_code_size);

  // Remember, we're not writing color indices to the file, but rather LZW
  // compression codes. So we map each color index to an LZW code. So in the
  // above example with 4 colors, our LZW codes will be as follows:
  //
  // +-------------+----------+
  // | Color Index | LZW Code |
  // +-------------+----------+
  // |      0      |     0    |
  // |      1      |     1    |
  // |      2      |     2    |
  // |      3      |     3    |
  // +-------------+----------+
  //
  // We add 2 more control codes that act as directives to the decoder, a clear
  // code, and an End-of-Information (EoI) code. They're assigned the next
  // available codes 4, and 5 respectively.
  const LzwCode clear_code = 1 << color_bit_depth;
  const LzwCode eoi_code = clear_code + 1;

  // EoI code is currently the maximum LZW code used, and requires the minimum
  // number of bits `color_bit_depth + 1` to be represented in binary, which is
  // 3 bits in our above example.
  uint8_t current_code_bit_size = color_bit_depth + 1;

  // The next available unassigned LZW code is the value after the EoI code,
  // which is 6 in our above example.
  LzwCode next_available_code = eoi_code + 1;

  // First, output the `clear_code` with the `current_code_bit_size` to start
  // the compression process.
  AppendCodeToStreamBuffer(clear_code, current_code_bit_size);

  // We start at the very first pixel color index, and we use the value of that
  // index as the current LZW code (remember from the above table that color
  // indices map to LZW codes that have the same values).
  LzwCode current_code = pixel_color_indices[0];

  // Then we start iterating at the following index ...
  for (size_t i = 1; i < pixel_color_indices.size(); ++i) {
    // ... asking if the current sequence of indices represented by the
    // `current_code` when it gets appended with `next_color_index`, does it map
    // to an existing LZW code in the table?
    const ColorIndex next_color_index = pixel_color_indices[i];
    LzwCode& code_in_table =
        code_table_[current_code].next_index_to_code[next_color_index];
    if (code_in_table) {
      // If yes, then `code_in_table` is a valid one, which means we have seen
      // this pattern of indices before and mapped it to a code in the table.
      // So we use that code we just found and the current code to represent the
      // new pattern that results from appending `next_color_index` to the
      // pattern of indices we had before.
      current_code = code_in_table;
    } else {
      // If no, then this is the first time ever we see this new pattern, so we
      // output `current_code` since we're starting now a new pattern for a
      // sequence of indices that begin with `next_color_index`.
      AppendCodeToStreamBuffer(current_code, current_code_bit_size);

      // We also assign a new LZW code to that pattern we didn't see before.
      // Note that `code_in_table` is a reference, so changing its value changes
      // the entry in the table.
      code_in_table = next_available_code++;
      current_code = next_color_index;

      // If the code we just added can no longer be represented as a binary
      // value using the current minimum number of bits `current_code_bit_size`,
      // we need to increment it by 1.
      if (code_in_table >= (1 << current_code_bit_size)) {
        ++current_code_bit_size;
      }

      // If we are about to exceed the maximum 12 bits per LZW code defined by
      // the specs, we need to signal to the decoder that we're starting a new
      // compression table.
      if (next_available_code >= kMaxNumberOfLzwCodes) {
        AppendCodeToStreamBuffer(clear_code, current_code_bit_size);

        code_table_.clear();
        next_available_code = eoi_code + 1;
        current_code_bit_size = color_bit_depth + 1;
      }
    }
  }

  // At the end, we output the last code to the stream, and the control codes
  // `clear_code` and `eoi_code` to signal the end of the compression.
  AppendCodeToStreamBuffer(current_code, current_code_bit_size);
  AppendCodeToStreamBuffer(clear_code, current_code_bit_size);
  AppendCodeToStreamBuffer(eoi_code, current_code_bit_size);

  // We need to flush all the remaining data in `pending_byte_` and
  // `byte_stream_buffer_` to the file.
  if (next_bit_ != 0) {
    FlushPendingByteToStream();
  }

  if (!byte_stream_buffer_.empty()) {
    FlushStreamBufferToFile();
  }

  // Finally, we write the block terminator.
  gif_file_writer_->WriteByte(0x00);
}

// -----------------------------------------------------------------------------
// LzwPixelColorIndicesWriter::CodeTableEntry:

LzwPixelColorIndicesWriter::CodeTableEntry::CodeTableEntry() = default;
LzwPixelColorIndicesWriter::CodeTableEntry::CodeTableEntry(CodeTableEntry&&) =
    default;
LzwPixelColorIndicesWriter::CodeTableEntry&
LzwPixelColorIndicesWriter::CodeTableEntry::operator=(CodeTableEntry&&) =
    default;
LzwPixelColorIndicesWriter::CodeTableEntry::~CodeTableEntry() = default;

// -----------------------------------------------------------------------------
// LzwPixelColorIndicesWriter:

void LzwPixelColorIndicesWriter::AppendCodeToStreamBuffer(
    LzwCode code,
    uint8_t code_bit_size) {
  for (uint8_t i = 0; i < code_bit_size; ++i) {
    AppendBitToPendingByte(code & 0x01);
    code >>= 1;
  }
}

void LzwPixelColorIndicesWriter::AppendBitToPendingByte(uint8_t bit) {
  bit <<= next_bit_;
  pending_byte_ |= bit;
  ++next_bit_;

  if (next_bit_ > 7) {
    FlushPendingByteToStream();
  }
}

void LzwPixelColorIndicesWriter::FlushPendingByteToStream() {
  byte_stream_buffer_.push_back(pending_byte_);
  if (byte_stream_buffer_.size() == kMaxBytesPerDataSubBlock) {
    FlushStreamBufferToFile();
  }
  DCHECK_LE(byte_stream_buffer_.size(), kMaxBytesPerDataSubBlock);
  pending_byte_ = 0;
  next_bit_ = 0;
}

void LzwPixelColorIndicesWriter::FlushStreamBufferToFile() {
  DCHECK(!byte_stream_buffer_.empty());

  gif_file_writer_->WriteByte(byte_stream_buffer_.size());
  gif_file_writer_->WriteBuffer(byte_stream_buffer_);
  byte_stream_buffer_.clear();
}

}  // namespace recording
