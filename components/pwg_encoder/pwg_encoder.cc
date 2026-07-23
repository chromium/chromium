// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pwg_encoder/pwg_encoder.h"

#include <limits.h>
#include <string.h>

#include <algorithm>
#include <memory>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/numerics/byte_conversions.h"
#include "components/pwg_encoder/bitmap_image.h"

namespace pwg_encoder {

namespace {

const uint32_t kBitsPerColor = 8;
const uint32_t kColorOrder = 0;  // chunky.

// Coefficients used to convert from RGB to monochrome.
const uint32_t kRedCoefficient = 2125;
const uint32_t kGreenCoefficient = 7154;
const uint32_t kBlueCoefficient = 721;
const uint32_t kColorCoefficientDenominator = 10000;

const char kPwgKeyword[] = "RaS2";

const uint32_t kHeaderSize = 1796;
const uint32_t kHeaderCupsDuplex = 272;
const uint32_t kHeaderCupsHwResolutionHorizontal = 276;
const uint32_t kHeaderCupsHwResolutionVertical = 280;
const uint32_t kHeaderCupsTumble = 368;
const uint32_t kHeaderCupsWidth = 372;
const uint32_t kHeaderCupsHeight = 376;
const uint32_t kHeaderCupsBitsPerColor = 384;
const uint32_t kHeaderCupsBitsPerPixel = 388;
const uint32_t kHeaderCupsBytesPerLine = 392;
const uint32_t kHeaderCupsColorOrder = 396;
const uint32_t kHeaderCupsColorSpace = 400;
const uint32_t kHeaderCupsNumColors = 420;
const uint32_t kHeaderPwgTotalPageCount = 452;
const uint32_t kHeaderPwgCrossFeedTransform = 456;
const uint32_t kHeaderPwgFeedTransform = 460;

const int kPwgMaxPackedRows = 256;

const int kPwgMaxPackedPixels = 128;

void EncodePixelToRGB(uint32_t pixel, std::string& output) {
  auto [b, g, r, a] = base::U32ToNativeEndian(pixel);
  output.push_back(static_cast<char>(r));
  output.push_back(static_cast<char>(g));
  output.push_back(static_cast<char>(b));
}

void EncodePixelToMonochrome(uint32_t pixel, std::string& output) {
  auto [b, g, r, a] = base::U32ToNativeEndian(pixel);
  output.push_back(static_cast<char>((r * kRedCoefficient +    //
                                      g * kGreenCoefficient +  //
                                      b * kBlueCoefficient)    //
                                     / kColorCoefficientDenominator));
}

std::string EncodePageHeader(const BitmapImage& image,
                             const PwgHeaderInfo& pwg_header_info) {
  uint8_t header_buf[kHeaderSize] = {};
  auto header = base::span(header_buf);

  uint32_t num_colors =
      pwg_header_info.color_space == PwgHeaderInfo::SGRAY ? 1 : 3;
  uint32_t bits_per_pixel = num_colors * kBitsPerColor;

  header.subspan<kHeaderCupsDuplex, 4u>().copy_from(
      base::U32ToBigEndian(pwg_header_info.duplex ? 1 : 0));
  header.subspan<kHeaderCupsHwResolutionHorizontal, 4u>().copy_from(
      base::U32ToBigEndian(pwg_header_info.dpi.width()));
  header.subspan<kHeaderCupsHwResolutionVertical, 4u>().copy_from(
      base::U32ToBigEndian(pwg_header_info.dpi.height()));
  header.subspan<kHeaderCupsTumble, 4u>().copy_from(
      base::U32ToBigEndian(pwg_header_info.tumble ? 1 : 0));
  header.subspan<kHeaderCupsWidth, 4u>().copy_from(
      base::U32ToBigEndian(image.size().width()));
  header.subspan<kHeaderCupsHeight, 4u>().copy_from(
      base::U32ToBigEndian(image.size().height()));
  header.subspan<kHeaderCupsBitsPerColor, 4u>().copy_from(
      base::U32ToBigEndian(kBitsPerColor));
  header.subspan<kHeaderCupsBitsPerPixel, 4u>().copy_from(
      base::U32ToBigEndian(bits_per_pixel));
  header.subspan<kHeaderCupsBytesPerLine, 4u>().copy_from(
      base::U32ToBigEndian((bits_per_pixel * image.size().width() + 7) / 8));
  header.subspan<kHeaderCupsColorOrder, 4u>().copy_from(
      base::U32ToBigEndian(kColorOrder));
  header.subspan<kHeaderCupsColorSpace, 4u>().copy_from(
      base::U32ToBigEndian(pwg_header_info.color_space));
  header.subspan<kHeaderCupsNumColors, 4u>().copy_from(
      base::U32ToBigEndian(num_colors));
  header.subspan<kHeaderPwgCrossFeedTransform, 4u>().copy_from(
      base::U32ToBigEndian(pwg_header_info.flipx ? -1 : 1));
  header.subspan<kHeaderPwgFeedTransform, 4u>().copy_from(
      base::U32ToBigEndian(pwg_header_info.flipy ? -1 : 1));
  header.subspan<kHeaderPwgTotalPageCount, 4u>().copy_from(
      base::U32ToBigEndian(pwg_header_info.total_pages));
  return std::string(header.begin(), header.end());
}

template <class RandomAccessIterator>
void EncodeRow(RandomAccessIterator pos,
               RandomAccessIterator row_end,
               bool monochrome,
               std::string& output) {
  // According to PWG-raster, a sequence of N identical pixels (up to 128)
  // can be encoded by a byte N-1, followed by the information on
  // that pixel. Any generic sequence of N pixels (up to 129) can be encoded
  // with (signed) byte 1-N, followed by the information on the N pixels.
  // Notice that for sequences of 1 pixel there is no difference between
  // the two encodings.

  // We encode every largest sequence of identical pixels together because it
  // usually saves the most space. Every other pixel should be encoded in the
  // smallest number of generic sequences.
  // NOTE: the algorithm is not optimal especially in case of monochrome.
  while (pos != row_end) {
    RandomAccessIterator it = pos + 1;
    RandomAccessIterator end = (row_end - pos > kPwgMaxPackedPixels)
                                   ? pos + kPwgMaxPackedPixels
                                   : row_end;

    // Counts how many identical pixels (up to 128).
    while (it != end && *pos == *it) {
      ++it;
    }
    if (it != pos + 1) {  // More than one pixel
      output.push_back(static_cast<char>((it - pos) - 1));
      if (monochrome) {
        EncodePixelToMonochrome(*pos, output);
      } else {
        EncodePixelToRGB(*pos, output);
      }
      pos = it;
    } else {
      // Finds how many pixels there are each different from the previous one.
      // IMPORTANT: even if sequences of different pixels can contain as many
      // as 129 pixels, we restrict to 128 because some decoders don't manage
      // it correctly. So iterating until it != end is correct.
      while (it != end && *it != *(it - 1)) {
        ++it;
      }
      // Optimization: ignores the last pixel of the sequence if it is followed
      // by an identical pixel, as it is more convenient for it to be the start
      // of a new sequence of identical pixels. Notice that we don't compare
      // to end, but row_end.
      if (it != row_end && *it == *(it - 1)) {
        --it;
      }
      output.push_back(static_cast<char>(1 - (it - pos)));
      while (pos != it) {
        if (monochrome) {
          EncodePixelToMonochrome(*pos, output);
        } else {
          EncodePixelToRGB(*pos, output);
        }
        ++pos;
      }
    }
  }
}

}  // namespace

// static
std::string PwgEncoder::EncodePageFromBGRAColorspace(
    const BitmapImage& image,
    const PwgHeaderInfo& pwg_header_info) {
  bool monochrome = pwg_header_info.color_space == PwgHeaderInfo::SGRAY;
  std::string output = EncodePageHeader(image, pwg_header_info);

  // Ensure no integer overflow.
  CHECK(image.size().width() < INT_MAX / image.channels());

  int row_number = 0;
  while (row_number < image.size().height()) {
    base::span<const uint32_t> current_row =
        image.GetRow(row_number++, pwg_header_info.flipy);
    int num_identical_rows = 1;
    // We count how many times the current row is repeated.
    while (num_identical_rows < kPwgMaxPackedRows &&
           row_number < image.size().height() &&
           current_row == image.GetRow(row_number, pwg_header_info.flipy)) {
      num_identical_rows++;
      row_number++;
    }
    output.push_back(static_cast<char>(num_identical_rows - 1));

    // The BGRA colorspace has a 32-bit pixels information.
    // Management of the bytes of the pixel is done by pixel_encoder function
    // on the original array to avoid endian problems.
    if (!pwg_header_info.flipx) {
      EncodeRow(current_row.begin(), current_row.end(), monochrome, output);
    } else {
      // We reverse the iterators.
      EncodeRow(current_row.rbegin(), current_row.rend(), monochrome, output);
    }
  }
  return output;
}

// static
std::string PwgEncoder::GetDocumentHeader() {
  std::string output;
  output.append(kPwgKeyword, 4);
  return output;
}

}  // namespace pwg_encoder
