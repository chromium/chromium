// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_RECORDING_LZW_PIXEL_COLOR_INDICES_WRITER_H_
#define CHROMEOS_ASH_SERVICES_RECORDING_LZW_PIXEL_COLOR_INDICES_WRITER_H_

#include <cstdint>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/services/recording/gif_encoding_types.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace recording {

class GifFileWriter;

// Background: After color quantization is performed on the video frame, we end
// up with:
// 1- A color table (or a color palette) that contains a list of colors (up to
//    256 colors) that can best represent all the colors in the video frame.
// 2- A list of indices (`pixel_color_indices`), which defines a mapping between
//    each pixel in the video frame and an index of a color in the above
//    mentioned color palette. Pixels are processed from left to right and from
//    top to bottom.
// If we try to write these pixel indices as they are to the GIF file, that
// could be a huge waste of space (magine an animated GIF image of size 1920 x
// 1080, this means we would write (1920 x 1080 = 2073600) indices for each
// video frame.
// In order to solve this problem, GIF uses the Variable-Length-Code Lempel-Ziv-
// Welch (LZW) compression algorithm, which tries to take advantage of the fact
// that many neighboring pixels may share the same color, and therefore, it’s
// possible to use special codes to indicate a sequence of colors, rather than
// one color at a time.
// To have even more compression when writing those LZW codes to the file, the
// GIF specs employs the Variable-Length-Code aspect of the algorithm as
// follows:
// - The number of bits needed to represent the currently used compression codes
//   is tracked as `current_code_bit_size` (see .cc file).
// - Instead of writing whole bytes for these codes to the file, we write only
//   the bits needed to represent them using the `current_code_bit_size`. These
//   bits are packages in a byte stream and eventually get written to the file.
// - As we add new codes, when the new number of codes exceeds what's currently
//   representable by `current_code_bit_size`, we increment it by one.
//
// This class implements the GIF LZW compression of a given
// `pixel_color_indices`, and takes care of writing the output to the GIF file
// using the given `gif_file_writer`.
class LzwPixelColorIndicesWriter {
 public:
  explicit LzwPixelColorIndicesWriter(GifFileWriter* gif_file_writer);
  LzwPixelColorIndicesWriter(const LzwPixelColorIndicesWriter&) = delete;
  LzwPixelColorIndicesWriter& operator=(const LzwPixelColorIndicesWriter&) =
      delete;
  ~LzwPixelColorIndicesWriter();

  // Encodes (using the variable-length-code LZW algorithm) the given
  // `pixel_color_indices`, which correspond to a new video frame, and writes
  // the output to the GIF file using `gif_file_writer_`. Note that the given
  // `color_bit_depth` is the least number of bits needed to represent all the
  // color indices in `pixel_color_indices` as binary values (e.g. if we have 4
  // colors, then 2 bits are needed to represent them as binary values, and
  // therefore `color_bit_depth` is 2).
  void EncodeAndWrite(const ColorIndices& pixel_color_indices,
                      uint8_t color_bit_depth);

 private:
  // A packed key into `code_table_` combining `current_code` and
  // `next_color_index`.
  using CodeTableKey = uint32_t;
  static constexpr CodeTableKey MakeCodeTableKey(LzwCode current_code,
                                                 ColorIndex next_color_index) {
    static_assert(sizeof(LzwCode) + sizeof(ColorIndex) <= sizeof(CodeTableKey));
    return (CodeTableKey{current_code} << (sizeof(ColorIndex) * 8)) |
           CodeTableKey{next_color_index};
  }

  // Clears all entries from `code_table_` while retaining its reserved backing
  // array (`absl::flat_hash_map::clear()` deallocates the backing storage).
  void ResetCodeTable();

  // Appends the given `code` to the `byte_stream_buffer_` by first appending it
  // bit-by-bit to `pending_byte_` using the number of bits provided in
  // `code_bit_size` (this is done by calling `AppendBitToPendingByte()`). Once
  // `pending_byte_` is complete, it will be pushed back to
  // `byte_stream_buffer_` and `byte_stream_buffer_` will be flushed to the GIF
  // file once it reaches the `kMaxBytesPerDataSubBlock`.
  void AppendCodeToStreamBuffer(LzwCode code, uint8_t code_bit_size);

  // Appends the least significant bit of `bit` to `pending_byte_` at the
  // `next_bit_` index. Once `pending_byte_` gets filled completely, i.e. 8 bits
  // have been appended to it (i.e. `next_bit_` > 7), `pending_byte_` will be
  // pushed back to the `byte_stream_buffer_` using the below call to
  // `FlushPendingByteToStream()`.
  void AppendBitToPendingByte(uint8_t bit);

  // Pushes back the `pending_byte_` to the `byte_stream_buffer_` and resets
  // `pending_byte_` and `next_bit_` back to zeros. Once the size of
  // `byte_stream_buffer_` exceeds `kMaxBytesPerDataSubBlock`, the contents of
  // the buffer will be flushed to the GIF file via a call to
  // `FlushStreamBufferToFile()`.
  void FlushPendingByteToStream();

  // Writes the number of bytes in `byte_stream_buffer_` to the GIF file,
  // followed by the contents of the buffer. The buffer is then cleared.
  void FlushStreamBufferToFile();

  // Used for writing bytes to the GIF file and takes care of handling IO errors
  // and disk space / DriveFS quota issues.
  const raw_ptr<GifFileWriter> gif_file_writer_;

  // See above background, we don't write the generated LZW codes directly to
  // the file, however we try to do further compression by writing only the bits
  // needed to represent them with the number bits as the value of
  // `current_code_bit_size`. Those bits are packages in `pending_byte_` at the
  // current `next_bit_` (which is the index of the bit at which the next bit
  // will be appended to `pending_byte_`). Once a whole byte has been written to
  // `pending_byte_` (i.e. when `next_bit_` is greater than 7), `pending_byte_`
  // will be pushed back to the below `byte_stream_buffer_`, and both
  // `pending_byte_` and `next_bit_` will be reset back to zeros waiting for the
  // next bits to be appended.
  uint8_t pending_byte_ = 0;
  uint8_t next_bit_ = 0;

  // See the above comment for `pending_byte_`. Once `pending_byte_` is
  // complete, it will be pushed back to this vector. The contents of this
  // vector will not be written to the file until:
  // - Either the number of bytes reached `kMaxBytesPerDataSubBlock` (which is
  //   `0xFF` or 255).
  // - Or at the end of the compression, to output the remaining bytes in this
  //   buffer resulting in a partial data sub-block.
  // Before the bytes in this buffer is written to the file, the number of bytes
  // is written to the file first (see `FlushStreamBufferToFile()`).
  std::vector<uint8_t> byte_stream_buffer_;

  // This is the LZW compression table, which is re-built every frame or when
  // the number of generated codes reaches `kMaxNumberOfLzwCodes` (4096).
  // We key into this table with `MakeCodeTableKey(current_code,
  // next_color_index)`, where `current_code` represents the pattern of
  // color indices seen so far in the input stream `pixel_color_indices`. What
  // this table tells us is, for the current LZW code [G] when the next color
  // index in the input stream [K] is appended: Do we have an existing LZW code
  // for this new pattern (indices represented by [G])[K]?
  // - Either yes, and in this case `code_table_[MakeCodeTableKey(G, K)]` is a
  //   valid code [H], so we now use [H] to represent all the pattern or color
  //   indices seen so far in the input stream.
  // - Or no, and therefore we append the code [G] to the `byte_stream_buffer_`,
  //   add the next available LZW code to the table to represent the pattern
  //   (sequence of indices mapped to [G])[K], and use [K] as the code that
  //   represents a new color indices pattern that starts at this pixel.
  //
  // Because fewer than `kMaxNumberOfLzwCodes` transitions are inserted before
  // the table is reset, reserving `kMaxNumberOfLzwCodes` upfront and erasing
  // all entries via `ResetCodeTable()` retains the backing allocation across
  // resets and frames without runtime heap allocations or rehashes.
  absl::flat_hash_map<CodeTableKey, LzwCode> code_table_;
};

}  // namespace recording

#endif  // CHROMEOS_ASH_SERVICES_RECORDING_LZW_PIXEL_COLOR_INDICES_WRITER_H_
