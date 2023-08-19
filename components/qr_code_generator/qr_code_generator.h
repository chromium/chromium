// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_QR_CODE_GENERATOR_QR_CODE_GENERATOR_H_
#define COMPONENTS_QR_CODE_GENERATOR_QR_CODE_GENERATOR_H_

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/containers/span.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace qr_code_generator {

struct QRVersionInfo;

// QRCodeGenerator generates class M QR codes of various versions.
// References in the following comments refer to ISO 18004 (3rd edition).
// Supports versions up to 26 by adding constants.
class QRCodeGenerator {
 public:
  // Contains output data for Generate().
  // The default state contains no data.
  struct GeneratedCode {
   public:
    GeneratedCode();
    GeneratedCode(GeneratedCode&&);
    GeneratedCode& operator=(GeneratedCode&&);

    GeneratedCode(const GeneratedCode&) = delete;
    GeneratedCode& operator=(const GeneratedCode&) = delete;

    ~GeneratedCode();

    // Pixel data.  The least-significant bit of each byte is set if that
    // tile/module should be "black".
    //
    // Clients should ensure four tiles/modules of padding when rendering the
    // code.
    //
    // On error, will not be populated, and will contain an empty vector.
    std::vector<uint8_t> data;

    // Width and height (which are equal) of the generated data, in
    // tiles/modules.
    //
    // The following invariant holds: `qr_size * qr_size == data.size()`.
    //
    // On error, will not be populated, and will contain 0.
    int qr_size = 0;
  };

  // Static parameters for V5 QR codes.
  // These exist while migrating clients to dynamic sizes.
  struct V5 {
    static constexpr int kSize = 37;
    static constexpr int kTotalSize = kSize * kSize;
    static constexpr size_t kNumSegments = 2;
    static constexpr size_t kSegmentDataBytes = 43;
    static constexpr size_t kDataBytes = kSegmentDataBytes * kNumSegments;
    static constexpr size_t kInputBytes = kDataBytes - 2;
  };

  // VersionClass enumerates the two types of QR code supported: small and
  // large. These differ in the size of the lengths used.
  enum class VersionClass {
    SMALL,
    LARGE,
    // Micro QR codes and versions >= 27 would be their own class but are not
    // supported.
  };

  // SegmentType enumerates the different data segments that can be in a QR
  // code. See section 7.3.
  enum class SegmentType {
    DIGIT = 1,
    ALPHANUM = 2,
    BINARY = 3,
    // ECI and Kanji segments are not supported.
  };

  // A Segment is a run of input bytes encoded as a given segment type.
  struct Segment {
    SegmentType type;
    size_t length;
  };

  QRCodeGenerator();
  ~QRCodeGenerator();

  // Generates a QR code containing the given data.
  // The generator will attempt to choose a version that fits the data and which
  // is >= |min_version|, if given. The returned span's length is
  // input-dependent and not known at compile-time.
  absl::optional<GeneratedCode> Generate(
      base::span<const uint8_t> in,
      absl::optional<int> min_version = absl::nullopt);

  // kMaxInputSize is the maximum number of bytes that `Generate` will try to
  // process. Inputs larger than this will certainly fail, but could otherwise
  // take a long time to do so as the code tries to calculate an optimum
  // segmentation before it realises that it's hopeless.
  //
  // To calculate this value, consider the table at
  // https://www.qrcode.com/en/about/version.html. Take the largest capacity QR
  // format supported by the code, find the value for "numeric" encoding, and
  // round up a little.
  static constexpr size_t kMaxInputSize = 700;

 private:
  FRIEND_TEST_ALL_PREFIXES(QRCodeGenerator, Segmentation);
  FRIEND_TEST_ALL_PREFIXES(QRCodeGenerator, SegmentationValid);

  // PutFinder paints a finder symbol at the given coordinates.
  void PutFinder(int x, int y);

  // PutAlignment paints an alignment symbol centered at the given coordinates.
  void PutAlignment(int x, int y);

  // PutVerticalTiming paints the vertical timing signal.
  void PutVerticalTiming(int x);

  // PutVerticalTiming paints the horizontal timing signal.
  void PutHorizontalTiming(int y);

  // PutFormatBits paints the 15-bit, pre-encoded format metadata. See page 56
  // for the location of the format bits.
  void PutFormatBits(const uint16_t format);

  // PutVersionBlocks writes the two blocks of version information for QR
  // versions seven and above.
  void PutVersionBlocks(const uint32_t encoded_version);

  // PutBits writes the given data into the QR code in correct order, avoiding
  // structural elements that must have already been painted. See section 7.7.3
  // about the placement algorithm.
  void PutBits(const uint8_t* data,
               size_t data_len,
               uint8_t (*mask_func)(int, int));

  // at returns a reference to the given element of |d_|.
  uint8_t& at(int x, int y);

  // fillAt sets the |len| elements at (x, y) to |value|.
  void fillAt(int x, int y, size_t len, uint8_t value);

  // copyTo copies |len| elements from |data| to the elements at (x, y).
  void copyTo(int x, int y, const uint8_t* data, size_t len);

  // clipped returns a reference to the given element of |d_|, or to
  // |clip_dump_| if the coordinates are out of bounds.
  uint8_t& clipped(int x, int y);

  // GF28Mul returns the product of |a| and |b| (which must be field elements,
  // i.e. < 256) in the field GF(2^8) mod x^8 + x^4 + x^3 + x^2 + 1.
  static uint8_t GF28Mul(uint16_t a, uint16_t b);

  // AddErrorCorrection writes the Reed-Solomon expanded version of |in| to
  // |out|.
  // |out| should have length block_bytes for the code's version.
  // |in| should have length block_data_bytes for the code's version.
  // |block_bytes| and |block_ec_bytes| must be provided for the current
  // version/level/group.
  void AddErrorCorrection(base::span<uint8_t> out,
                          base::span<const uint8_t> in,
                          size_t block_bytes,
                          size_t block_ec_bytes);

  // CountPenaltyPoints sums the penalty points for the current, fully drawn,
  // code. See table 11.
  unsigned CountPenaltyPoints() const;

  // ClassifyByte returns the smallest |SegmentType| that can encode |byte|.
  static SegmentType ClassifyByte(uint8_t byte);

  // IsValidSegmentation returns true if |segments| precisely covers |input|
  // with segments that can encode the corresponding byte of |input|.
  static bool IsValidSegmentation(const std::vector<Segment>& segments,
                                  base::span<const uint8_t> input);

  // NoSuperfluousSegments returns true if no consecutive segments in |segments|
  // have the same type.
  static bool NoSuperfluousSegments(const std::vector<Segment>& segments);

  // InitialSegmentation returns a segmentation of |input| that puts each
  // byte into the smallest |SegmentType| that can encode it while merging
  // consecutive segments of the same type.
  static std::vector<Segment> InitialSegmentation(
      base::span<const uint8_t> input);

  // MergeSegments replaces the segments indexed by |start| and |end|
  // (inclusive) with |replacement|.
  static size_t MergeSegments(std::vector<Segment>* segments,
                              size_t start,
                              size_t end,
                              Segment replacement);

  // MaybeMerge merges the segments indexed by |start| and |end| (inclusive)
  // with a single segment of type |merged_type|, if that merge would take fewer
  // bits.
  static size_t MaybeMerge(VersionClass vclass,
                           std::vector<Segment>* segments,
                           size_t start,
                           size_t end,
                           SegmentType merged_type);

  // SegmentDigitAlphaSpan updates the segments indexed by |start| and |end|
  // (inclusive) to be a fairly optimal segmentation. The indicated segments
  // must all be |DIGIT| or |ALPHANUM| segments.
  static size_t SegmentDigitAlphaSpan(VersionClass vclass,
                                      std::vector<Segment>* segments,
                                      size_t start,
                                      size_t end);

  // SegmentInput returns a fairly optimal segmentation of |input|.
  static std::vector<Segment> SegmentInput(VersionClass vclass,
                                           base::span<const uint8_t> input);

  // Parameters for the currently-selected version of the QR code.
  // Generate() will pick a version that can contain enough data.
  // Unowned; nullptr until initialized in Generate().
  raw_ptr<const QRVersionInfo> version_info_ = nullptr;

  // d_ represents a QR code with one byte per pixel. Each byte is one pixel.
  // The LSB is set if the pixel is "black". The second bit is set if the pixel
  // is part of the structure of the QR code, i.e. finder or alignment symbols,
  // timing patterns, or format data.
  // Initialized and possibly reinitialized in Generate().
  std::vector<uint8_t> d_;

  // clip_dump_ is the target of paints that would otherwise fall outside of the
  // QR code.
  uint8_t clip_dump_;
};

}  // namespace qr_code_generator

#endif  // COMPONENTS_QR_CODE_GENERATOR_QR_CODE_GENERATOR_H_
