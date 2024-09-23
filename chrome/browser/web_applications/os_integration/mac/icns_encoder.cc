// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/mac/icns_encoder.h"

#include <algorithm>

#include "base/files/file.h"
#include "base/notreached.h"
#include "base/numerics/byte_conversions.h"
#include "base/numerics/checked_math.h"
#include "base/ranges/algorithm.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image.h"

namespace web_app {

namespace {

// Mapping of image size to the type identifiers used in the .icns file format
// for the png representation of the image as well as the RGB and Alpha channel
// representations.
struct IcnsBlockTypes {
  int size;
  uint32_t png_type;
  uint32_t image_type = 0;
  uint32_t mask_type = 0;
};

constexpr IcnsBlockTypes kIcnsBlockTypes[] = {
    {16, 'icp4', 'is32', 's8mk'},
    {32, 'icp5', 'il32', 'l8mk'},
    {48, 'icp6', 'ih32', 'h8mk'},
    {128, 'ic07'},
    {256, 'ic08'},
    {512, 'ic09'},
};

std::vector<uint8_t> CreateBlockHeader(uint32_t type, size_t data_length) {
  std::vector<uint8_t> result(8u);
  auto [first, second] = base::span(result).split_at<4u>();
  first.copy_from(base::U32ToBigEndian(type));
  second.copy_from(
      base::U32ToBigEndian(base::checked_cast<uint32_t>(data_length + 8u)));
  return result;
}

// Struct containing the red, green, blue and alpha channels extracted from an
// image as four separate vectors.
struct ImageBytes {
  std::vector<uint8_t> r, g, b, a;
};

// Extracts the red, green, blue and alpha channels from `bitmap` as four
// separate vectors. The red, green and blue channels will contain the
// unpremultiplied values, as that is how data is stored in an .icns file.
ImageBytes ExtractImageBytes(const SkBitmap& bitmap) {
  ImageBytes result;
  const size_t pixel_count = bitmap.height() * bitmap.width();
  result.r.reserve(pixel_count);
  result.g.reserve(pixel_count);
  result.b.reserve(pixel_count);
  result.a.reserve(pixel_count);
  for (int y = 0; y < bitmap.height(); ++y) {
    for (int x = 0; x < bitmap.width(); ++x) {
      SkColor c = bitmap.getColor(x, y);
      result.r.push_back(SkColorGetR(c));
      result.g.push_back(SkColorGetG(c));
      result.b.push_back(SkColorGetB(c));
      result.a.push_back(SkColorGetA(c));
    }
  }
  return result;
}

}  // namespace

IcnsEncoder::Block::Block(uint32_t type, std::vector<uint8_t> data)
    : type(type), data(std::move(data)) {}
IcnsEncoder::Block::~Block() = default;
IcnsEncoder::Block::Block(Block&&) = default;
IcnsEncoder::Block& IcnsEncoder::Block::operator=(Block&&) = default;

IcnsEncoder::IcnsEncoder() = default;
IcnsEncoder::~IcnsEncoder() = default;

bool IcnsEncoder::AddImage(const gfx::Image& image) {
  if (image.IsEmpty())
    return false;

  SkBitmap bitmap = image.AsBitmap();
  if (bitmap.colorType() != kN32_SkColorType ||
      bitmap.width() != bitmap.height())
    return false;

  const IcnsBlockTypes* block_types = base::ranges::find(
      kIcnsBlockTypes, bitmap.width(), &IcnsBlockTypes::size);
  if (block_types == std::end(kIcnsBlockTypes))
    return false;

  if (block_types->image_type != 0) {
    // If there is a legacy image type for this size we should use that rather
    // than the png format, as many places in Mac OS do not properly support png
    // icons for sizes that also support a legacy format.
    DCHECK(block_types->mask_type != 0);
    ImageBytes bytes = ExtractImageBytes(bitmap);
    std::vector<uint8_t> image_data;
    AppendRLEImageData(bytes.r, &image_data);
    AppendRLEImageData(bytes.g, &image_data);
    AppendRLEImageData(bytes.b, &image_data);
    AppendBlock(block_types->image_type, std::move(image_data));
    AppendBlock(block_types->mask_type, std::move(bytes.a));
  } else {
    DCHECK(block_types->png_type != 0);
    std::vector<uint8_t> png_data;
    if (!gfx::PNGCodec::EncodeBGRASkBitmap(
            bitmap, /*discard_transparancy=*/false, &png_data)) {
      return false;
    }
    AppendBlock(block_types->png_type, std::move(png_data));
  }
  return true;
}

bool IcnsEncoder::WriteToFile(const base::FilePath& path) const {
  // Build the Table of Contents, which is simply the headers of all the blocks
  // concatenated.
  Block toc('TOC ');
  toc.data.reserve(8 * blocks_.size());
  for (const auto& block : blocks_) {
    auto header = CreateBlockHeader(block.type, block.data.size());
    toc.data.insert(toc.data.end(), header.begin(), header.end());
  }

  size_t total_data_size =
      total_block_size_ + toc.data.size() + kBlockHeaderSize;

  base::File output(path, base::File::Flags::FLAG_CREATE_ALWAYS |
                              base::File::Flags::FLAG_WRITE);
  if (!output.IsValid())
    return false;

  if (!output.WriteAtCurrentPosAndCheck(
          ::web_app::CreateBlockHeader('icns', total_data_size))) {
    return false;
  }
  if (!WriteBlockToFile(output, toc))
    return false;
  for (const auto& block : blocks_) {
    if (!WriteBlockToFile(output, block))
      return false;
  }

  return true;
}

// static
void IcnsEncoder::AppendRLEImageData(base::span<const uint8_t> data,
                                     std::vector<uint8_t>* rle_data) {
  // The packing loop is done with two pieces of state:
  //   - data: at any point in the loop this only contains the bytes that have
  //           not yet been written to the block
  //   - search_offset: this is the offset within |data| used to search for
  //                    byte runs
  //
  // The code scours through the data, looking for runs of length greater than 3
  // (since only runs of 3 or longer can be compressed). As soon as a run is
  // found, all the data up to `search_offset` is dumped as literal data,
  // `data` is updated to only point at the remaining data, then the run is
  // dumped (and `data` updated again), and then the search continues.

  size_t search_offset = 0;

  // Search for runs through the block of data, byte by byte.
  while (search_offset < data.size()) {
    uint8_t current_byte = data[search_offset];
    size_t run_length = 1;
    while (search_offset + run_length < data.size() && run_length < 130 &&
           data[search_offset + run_length] == current_byte) {
      ++run_length;
    }
    if (run_length >= 3) {
      // A long-enough run was found. First, dump all the data before the run
      // into the output block.
      while (search_offset > 0) {
        // Because uncompressed data runs max out at 128 bytes of data, cap the
        // uncompressed run at 128 bytes.
        base::span<const uint8_t> uncompressed_chunk =
            data.first(std::min<size_t>(search_offset, 128));
        // Key byte values of 0..127 mean 1..128 bytes of uncompressed data.
        uint8_t key_byte = uncompressed_chunk.size() - 1;
        rle_data->push_back(key_byte);
        rle_data->insert(rle_data->end(), uncompressed_chunk.begin(),
                         uncompressed_chunk.end());
        data = data.subspan(uncompressed_chunk.size());
        search_offset -= uncompressed_chunk.size();
      }
      // Now that the output block is caught up, put the run that was just found
      // into it. Key byte values of 128..255 mean 3..130 copies of the
      // following byte, thus the addition of 125 to the run length.
      uint8_t key_byte = run_length + 125;
      rle_data->push_back(key_byte);
      rle_data->push_back(current_byte);
      data = data.subspan(run_length);
    } else {
      // The run is too small, so keep looking.
      search_offset += run_length;
    }
  }
  // At this point, there are no more runs, so pack the rest of the data into
  // the output block.
  while (search_offset > 0) {
    // Because uncompressed data runs max out at 128 bytes of data, cap the
    // uncompressed run at 128 bytes.
    base::span<const uint8_t> uncompressed_chunk =
        data.first(std::min<size_t>(search_offset, 128));
    // Key byte values of 0..127 mean 1..128 bytes of uncompressed data.
    uint8_t key_byte = uncompressed_chunk.size() - 1;
    rle_data->push_back(key_byte);
    rle_data->insert(rle_data->end(), uncompressed_chunk.begin(),
                     uncompressed_chunk.end());
    data = data.subspan(uncompressed_chunk.size());
    search_offset -= uncompressed_chunk.size();
  }
}

void IcnsEncoder::AppendBlock(uint32_t type, std::vector<uint8_t> data) {
  total_block_size_ += data.size() + kBlockHeaderSize;
  blocks_.emplace_back(type, std::move(data));
}

// static
bool IcnsEncoder::WriteBlockToFile(base::File& file, const Block& block) {
  if (!file.WriteAtCurrentPosAndCheck(
          CreateBlockHeader(block.type, block.data.size())))
    return false;
  if (!file.WriteAtCurrentPosAndCheck(block.data))
    return false;
  return true;
}

}  // namespace web_app
