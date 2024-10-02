// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/unique_position.h"

#include <algorithm>
#include <array>
#include <limits>
#include <string>

#include "base/base64.h"
#include "base/containers/span.h"
#include "base/hash/sha1.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/protocol/unique_position.pb.h"
#include "third_party/zlib/zlib.h"

namespace syncer {

namespace {

UniquePosition::Suffix StringToSuffix(std::string_view str) {
  CHECK_EQ(str.length(), UniquePosition::kSuffixLength);
  UniquePosition::Suffix suffix;
  base::ranges::copy(str, suffix.begin());
  return suffix;
}

std::string SuffixToString(const UniquePosition::Suffix& suffix) {
  return std::string(suffix.begin(), suffix.end());
}

}  // namespace

constexpr size_t UniquePosition::kSuffixLength;
constexpr size_t UniquePosition::kCompressBytesThreshold;

// static.
bool UniquePosition::IsValidSuffix(const Suffix& suffix) {
  return suffix.back() != 0;
}

// static.
bool UniquePosition::IsValidBytes(const std::string& bytes) {
  // The first condition ensures that our suffix uniqueness is sufficient to
  // guarantee position uniqueness.  Otherwise, it's possible the end of some
  // prefix + some short suffix == some long suffix.
  // The second condition ensures that FindSmallerWithSuffix can always return a
  // result.
  return bytes.length() >= kSuffixLength && bytes[bytes.length() - 1] != 0;
}

// static.
UniquePosition::Suffix UniquePosition::RandomSuffix() {
  // Users random data for all but the last byte. The last byte must not be
  // zero. Set it arbitrarily to 0x7f.
  Suffix suffix;
  base::RandBytes(base::make_span(suffix));
  suffix.back() = 0x7f;
  return suffix;
}

// static.
UniquePosition::Suffix UniquePosition::GenerateSuffix(
    const ClientTagHash& client_tag_hash) {
  std::string result = base::Base64Encode(
      base::SHA1Hash(base::as_byte_span(client_tag_hash.value())));
  UniquePosition::Suffix suffix = StringToSuffix(result);
  CHECK(IsValidSuffix(suffix));
  return suffix;
}

// static.
UniquePosition UniquePosition::FromProto(const sync_pb::UniquePosition& proto) {
  if (proto.has_custom_compressed_v1()) {
    return UniquePosition(proto.custom_compressed_v1());
  } else if (proto.has_value() && !proto.value().empty()) {
    return UniquePosition(Compress(proto.value()));
  } else if (proto.has_compressed_value() && proto.has_uncompressed_length()) {
    uLongf uncompressed_len = proto.uncompressed_length();
    std::string un_gzipped;

    un_gzipped.resize(uncompressed_len);
    int result = uncompress(
        reinterpret_cast<Bytef*>(std::data(un_gzipped)), &uncompressed_len,
        reinterpret_cast<const Bytef*>(proto.compressed_value().data()),
        proto.compressed_value().size());
    if (result != Z_OK) {
      DLOG(ERROR) << "Unzip failed " << result;
      return UniquePosition();
    }
    if (uncompressed_len != proto.uncompressed_length()) {
      DLOG(ERROR) << "Uncompressed length " << uncompressed_len
                  << " did not match specified length "
                  << proto.uncompressed_length();
      return UniquePosition();
    }
    return UniquePosition(Compress(un_gzipped));
  } else {
    return UniquePosition();
  }
}

// static.
UniquePosition UniquePosition::FromInt64(int64_t x, const Suffix& suffix) {
  uint64_t y = static_cast<uint64_t>(x);
  y ^= 0x8000000000000000ULL;  // Make it non-negative.
  std::string bytes(8, 0);
  for (int i = 7; i >= 0; --i) {
    bytes[i] = static_cast<uint8_t>(y);
    y >>= 8;
  }
  return UniquePosition(bytes + SuffixToString(suffix), suffix);
}

// static.
UniquePosition UniquePosition::InitialPosition(const Suffix& suffix) {
  DCHECK(IsValidSuffix(suffix));
  return UniquePosition(SuffixToString(suffix), suffix);
}

// static.
UniquePosition UniquePosition::Before(const UniquePosition& x,
                                      const Suffix& suffix) {
  DCHECK(IsValidSuffix(suffix));
  DCHECK(x.IsValid());
  const std::string& before =
      FindSmallerWithSuffix(Uncompress(x.compressed_), suffix);
  return UniquePosition(before + SuffixToString(suffix), suffix);
}

// static.
UniquePosition UniquePosition::After(const UniquePosition& x,
                                     const Suffix& suffix) {
  DCHECK(IsValidSuffix(suffix));
  DCHECK(x.IsValid());
  const std::string& after =
      FindGreaterWithSuffix(Uncompress(x.compressed_), suffix);
  return UniquePosition(after + SuffixToString(suffix), suffix);
}

// static.
UniquePosition UniquePosition::Between(const UniquePosition& before,
                                       const UniquePosition& after,
                                       const Suffix& suffix) {
  DCHECK(before.IsValid());
  DCHECK(after.IsValid());
  DCHECK(before.LessThan(after));
  DCHECK(IsValidSuffix(suffix));
  const std::string& mid = FindBetweenWithSuffix(
      Uncompress(before.compressed_), Uncompress(after.compressed_), suffix);
  return UniquePosition(mid + SuffixToString(suffix), suffix);
}

UniquePosition::UniquePosition() = default;

bool UniquePosition::LessThan(const UniquePosition& other) const {
  DCHECK(this->IsValid());
  DCHECK(other.IsValid());

  return compressed_ < other.compressed_;
}

bool UniquePosition::Equals(const UniquePosition& other) const {
  if (!this->IsValid() && !other.IsValid()) {
    return true;
  }

  return compressed_ == other.compressed_;
}

sync_pb::UniquePosition UniquePosition::ToProto() const {
  sync_pb::UniquePosition proto;

  // This is the current preferred foramt.
  proto.set_custom_compressed_v1(compressed_);
  return proto;

  // Older clients used to write other formats.  We don't bother doing that
  // anymore because that form of backwards compatibility is expensive.  We no
  // longer want to pay that price just too support clients that have been
  // obsolete for a long time.  See the proto definition for details.
}

void UniquePosition::SerializeToString(std::string* blob) const {
  DCHECK(blob);
  ToProto().SerializeToString(blob);
}

bool UniquePosition::IsValid() const {
  return !compressed_.empty();
}

std::string UniquePosition::ToDebugString() const {
  const std::string bytes = Uncompress(compressed_);
  if (bytes.empty()) {
    return std::string("INVALID[]");
  }

  std::string debug_string = base::HexEncode(bytes);
  if (!IsValid()) {
    debug_string = "INVALID[" + debug_string + "]";
  }

  std::string compressed_string = base::HexEncode(compressed_);
  debug_string.append(", compressed: " + compressed_string);
  return debug_string;
}

UniquePosition::Suffix UniquePosition::GetSuffixForTest() const {
  const std::string bytes = Uncompress(compressed_);
  const size_t prefix_len = bytes.length() - kSuffixLength;
  return StringToSuffix(bytes.substr(prefix_len));
}

std::string UniquePosition::FindSmallerWithSuffix(const std::string& reference,
                                                  const Suffix& suffix) {
  size_t ref_zeroes = reference.find_first_not_of('\0');
  std::string suffix_str = SuffixToString(suffix);
  size_t suffix_zeroes = suffix_str.find_first_not_of('\0');

  // Neither of our inputs are allowed to have trailing zeroes, so the following
  // must be true.
  DCHECK_NE(ref_zeroes, std::string::npos);
  DCHECK_NE(suffix_zeroes, std::string::npos);

  if (suffix_zeroes > ref_zeroes) {
    // Implies suffix < ref.
    return std::string();
  }

  if (suffix_str.substr(suffix_zeroes) < reference.substr(ref_zeroes)) {
    // Prepend zeroes so the result has as many zero digits as |reference|.
    return std::string(ref_zeroes - suffix_zeroes, '\0');
  } else if (suffix_zeroes > 1) {
    // Prepend zeroes so the result has one more zero digit than |reference|.
    // We could also take the "else" branch below, but taking this branch will
    // give us a smaller result.
    return std::string(ref_zeroes - suffix_zeroes + 1, '\0');
  } else {
    // Prepend zeroes to match those in the |reference|, then something smaller
    // than the first non-zero digit in |reference|.
    char lt_digit = static_cast<uint8_t>(reference[ref_zeroes]) / 2;
    return std::string(ref_zeroes, '\0') + lt_digit;
  }
}

// static
std::string UniquePosition::FindGreaterWithSuffix(const std::string& reference,
                                                  const Suffix& suffix) {
  size_t ref_FFs =
      reference.find_first_not_of(std::numeric_limits<uint8_t>::max());
  std::string suffix_str = SuffixToString(suffix);
  size_t suffix_FFs =
      suffix_str.find_first_not_of(std::numeric_limits<uint8_t>::max());

  if (ref_FFs == std::string::npos) {
    ref_FFs = reference.length();
  }
  if (suffix_FFs == std::string::npos) {
    suffix_FFs = suffix_str.length();
  }

  if (suffix_FFs > ref_FFs) {
    // Implies suffix > reference.
    return std::string();
  }

  if (suffix_str.substr(suffix_FFs) > reference.substr(ref_FFs)) {
    // Prepend FF digits to match those in |reference|.
    return std::string(ref_FFs - suffix_FFs,
                       std::numeric_limits<uint8_t>::max());
  } else if (suffix_FFs > 1) {
    // Prepend enough leading FF digits so result has one more of them than
    // |reference| does.  We could also take the "else" branch below, but this
    // gives us a smaller result.
    return std::string(ref_FFs - suffix_FFs + 1,
                       std::numeric_limits<uint8_t>::max());
  } else {
    // Prepend FF digits to match those in |reference|, then something larger
    // than the first non-FF digit in |reference|.
    char gt_digit = static_cast<uint8_t>(reference[ref_FFs]) +
                    (std::numeric_limits<uint8_t>::max() -
                     static_cast<uint8_t>(reference[ref_FFs]) + 1) /
                        2;
    return std::string(ref_FFs, std::numeric_limits<uint8_t>::max()) + gt_digit;
  }
}

// static
std::string UniquePosition::FindBetweenWithSuffix(const std::string& before,
                                                  const std::string& after,
                                                  const Suffix& suffix) {
  DCHECK(IsValidSuffix(suffix));
  DCHECK_NE(before, after);
  DCHECK_LT(before, after);

  std::string suffix_str = SuffixToString(suffix);
  std::string mid;

  // Sometimes our suffix puts us where we want to be.
  if (before < suffix_str && suffix_str < after) {
    return std::string();
  }

  size_t i = 0;
  for (; i < std::min(before.length(), after.length()); ++i) {
    uint8_t a_digit = before[i];
    uint8_t b_digit = after[i];

    if (b_digit - a_digit >= 2) {
      mid.push_back(a_digit + (b_digit - a_digit) / 2);
      return mid;
    } else if (a_digit == b_digit) {
      mid.push_back(a_digit);

      // Both strings are equal so far.  Will appending the suffix at this point
      // give us the comparison we're looking for?
      if (before.substr(i + 1) < suffix_str &&
          suffix_str < after.substr(i + 1)) {
        return mid;
      }
    } else {
      DCHECK_EQ(b_digit - a_digit, 1);  // Implied by above if branches.

      // The two options are off by one digit.  The choice of whether to round
      // up or down here will have consequences on what we do with the remaining
      // digits.  Exploring both options is an optimization and is not required
      // for the correctness of this algorithm.

      // Option A: Round down the current digit.  This makes our |mid| <
      // |after|, no matter what we append afterwards.  We then focus on
      // appending digits until |mid| > |before|.
      std::string mid_a = mid;
      mid_a.push_back(a_digit);
      mid_a.append(FindGreaterWithSuffix(before.substr(i + 1), suffix));

      // Option B: Round up the current digit.  This makes our |mid| > |before|,
      // no matter what we append afterwards.  We then focus on appending digits
      // until |mid| < |after|.  Note that this option may not be viable if the
      // current digit is the last one in |after|, so we skip the option in that
      // case.
      if (after.length() > i + 1) {
        std::string mid_b = mid;
        mid_b.push_back(b_digit);
        mid_b.append(FindSmallerWithSuffix(after.substr(i + 1), suffix));

        // Does this give us a shorter position value?  If so, use it.
        if (mid_b.length() < mid_a.length()) {
          return mid_b;
        }
      }
      return mid_a;
    }
  }

  // If we haven't found a midpoint yet, the following must be true:
  DCHECK_EQ(before.substr(0, i), after.substr(0, i));
  DCHECK_EQ(before, mid);
  DCHECK_LT(before.length(), after.length());

  // We know that we'll need to append at least one more byte to |mid| in the
  // process of making it < |after|.  Appending any digit, regardless of the
  // value, will make |before| < |mid|.  Therefore, the following will get us a
  // valid position.

  mid.append(FindSmallerWithSuffix(after.substr(i), suffix));
  return mid;
}

UniquePosition::UniquePosition(const std::string& compressed)
    : compressed_(IsValidBytes(Uncompress(compressed)) ? compressed
                                                       : std::string()) {}

UniquePosition::UniquePosition(const std::string& uncompressed,
                               const Suffix& suffix)
    : UniquePosition(Compress(uncompressed)) {
  DCHECK(uncompressed.rfind(SuffixToString(suffix)) + kSuffixLength ==
         uncompressed.length());
  DCHECK(IsValidSuffix(suffix));
  DCHECK(IsValid());
}

// On custom compression:
//
// Let C(x) be the compression function and U(x) be the uncompression function.
//
// This compression scheme has a few special properties.  For one, it is
// order-preserving.  For any two valid position strings x and y:
//   x < y <=> C(x) < C(y)
// This allows us keep the position strings compressed as we sort them.
//
// The compressed format and the decode algorithm:
//
// The compressed string is a series of blocks, almost all of which are 8 bytes
// in length.  The only exception is the last block in the compressed string,
// which may be a remainder block, which has length no greater than 7.  The
// full-length blocks are either repeated character blocks or plain data blocks.
// All blocks are entirely self-contained.  Their decoded values are independent
// from that of their neighbours.
//
// A repeated character block is encoded into eight bytes and represents between
// 4 and 2^31 repeated instances of a given character in the unencoded stream.
// The encoding consists of a single character repeated four times, followed by
// an encoded count.  The encoded count is stored as a big-endian 32 bit
// integer.  There are 2^31 possible count values, and two encodings for each.
// The high encoding is 'enc = kuint32max - count'; the low encoding is 'enc =
// count'.  At compression time, the algorithm will choose between the two
// encodings based on which of the two will maintain the appropriate sort
// ordering (by a process which will be described below).  The decompression
// algorithm need not concern itself with which encoding was used; it needs only
// to decode it.  The decoded value of this block is "count" instances of the
// character that was repeated four times in the first half of this block.
//
// A plain data block is encoded into eight bytes and represents exactly eight
// bytes of data in the unencoded stream.  The plain data block must not begin
// with the same character repeated four times.  It is allowed to contain such a
// four-character sequence, just not at the start of the block.  The decoded
// value of a plain data block is identical to its encoded value.
//
// A remainder block has length of at most seven.  It is a shorter version of
// the plain data block.  It occurs only at the end of the encoded stream and
// represents exactly as many bytes of unencoded data as its own length.  Like a
// plain data block, the remainder block never begins with the same character
// repeated four times.  The decoded value of this block is identical to its
// encoded value.
//
// The encode algorithm:
//
// From the above description, it can be seen that there may be more than one
// way to encode a given input string.  The encoder must be careful to choose
// the encoding that guarantees sort ordering.
//
// The rules for the encoder are as follows:
// 1. Iterate through the input string and produce output blocks one at a time.
// 2. Where possible (ie. where the next four bytes of input consist of the
//    same character repeated four times), produce a repeated data block of
//    maximum possible length.
// 3. If there is at least 8 bytes of data remaining and it is not possible
//    to produce a repeated character block, produce a plain data block.
// 4. If there are less than 8 bytes of data remaining and it is not possible
//    to produce a repeated character block, produce a remainder block.
// 5. When producing a repeated character block, the count encoding must be
//    chosen in such a way that the sort ordering is maintained.  The choice is
//    best illustrated by way of example:
//
//      When comparing two strings, the first of which begins with of 8
//      instances of the letter 'B' and the second with 10 instances of the
//      letter 'B', which of the two should compare lower?  The result depends
//      on the 9th character of the first string, since it will be compared
//      against the 9th 'B' in the second string.  If that character is an 'A',
//      then the first string will compare lower.  If it is a 'C', then the
//      first string will compare higher.
//
//    The key insight is that the comparison value of a repeated character block
//    depends on the value of the character that follows it.  If the character
//    follows the repeated character has a value greater than the repeated
//    character itself, then a shorter run length should translate to a higher
//    comparison value.  Therefore, we encode its count using the low encoding.
//    Similarly, if the following character is lower, we use the high encoding.

namespace {

// Appends an encoded run length to |output_str|.
static void WriteEncodedRunLength(uint32_t length,
                                  bool high_encoding,
                                  std::string* output_str) {
  CHECK_GE(length, 4U);
  CHECK_LT(length, 0x80000000);

  // Step 1: Invert the count, if necessary, to account for the following digit.
  uint32_t encoded_length;
  if (high_encoding) {
    encoded_length = 0xffffffff - length;
  } else {
    encoded_length = length;
  }

  // Step 2: Write it as big-endian so it compares correctly with memcmp(3).
  output_str->append(1, 0xff & (encoded_length >> 24U));
  output_str->append(1, 0xff & (encoded_length >> 16U));
  output_str->append(1, 0xff & (encoded_length >> 8U));
  output_str->append(1, 0xff & (encoded_length >> 0U));
}

// Reads an encoded run length for |str| at position |i|.
static uint32_t ReadEncodedRunLength(const std::string& str, size_t i) {
  DCHECK_LE(i + 4, str.length());

  // Step 1: Extract the big-endian count.
  uint32_t encoded_length = (static_cast<uint8_t>(str[i + 3]) << 0) |
                            (static_cast<uint8_t>(str[i + 2]) << 8) |
                            (static_cast<uint8_t>(str[i + 1]) << 16) |
                            (static_cast<uint8_t>(str[i + 0]) << 24);

  // Step 2: If this was an inverted count, un-invert it.
  uint32_t length;
  if (encoded_length & 0x80000000) {
    length = 0xffffffff - encoded_length;
  } else {
    length = encoded_length;
  }

  return length;
}

// A series of four identical chars at the beginning of a block indicates
// the beginning of a repeated character block.
static bool IsRepeatedCharPrefix(const std::string& chars, size_t start_index) {
  return chars[start_index] == chars[start_index + 1] &&
         chars[start_index] == chars[start_index + 2] &&
         chars[start_index] == chars[start_index + 3];
}

}  // namespace

// static
// Wraps the CompressImpl function with a bunch of DCHECKs.
std::string UniquePosition::Compress(const std::string& str) {
  DCHECK(IsValidBytes(str));
  std::string compressed = CompressImpl(str);
  DCHECK(IsValidCompressed(compressed));
  DCHECK_EQ(str, Uncompress(compressed));
  return compressed;
}

// static
// Performs the order preserving run length compression of a given input string.
std::string UniquePosition::CompressImpl(const std::string& str) {
  std::string output;

  // The compressed length will usually be at least as long as the suffix (28),
  // since the suffix bytes are mostly random.  Most are a few bytes longer; a
  // small few are tens of bytes longer.  Some early tests indicated that
  // roughly 99% had length 40 or smaller.  We guess that pre-sizing for 48 is a
  // good trade-off, but that has not been confirmed through profiling.
  output.reserve(48);

  // Each loop iteration will consume 8, or N bytes, where N >= 4 and is the
  // length of a string of identical digits starting at i.
  for (size_t i = 0; i < str.length();) {
    if (i + 4 <= str.length() && IsRepeatedCharPrefix(str, i)) {
      // Four identical bytes in a row at this position means that we must start
      // a repeated character block.  Begin by outputting those four bytes.
      output.append(str, i, 4);

      // Determine the size of the run.
      const char rep_digit = str[i];
      const size_t runs_until = str.find_first_not_of(rep_digit, i + 4);

      // Handle the 'runs until end' special case specially.
      size_t run_length;
      bool encode_high;  // True if the next byte is greater than |rep_digit|.
      if (runs_until == std::string::npos) {
        run_length = str.length() - i;
        encode_high = false;
      } else {
        run_length = runs_until - i;
        encode_high = static_cast<uint8_t>(str[runs_until]) >
                      static_cast<uint8_t>(rep_digit);
      }
      DCHECK_LT(run_length,
                static_cast<size_t>(std::numeric_limits<int32_t>::max()))
          << "This implementation can't encode run-lengths greater than 2^31.";

      WriteEncodedRunLength(run_length, encode_high, &output);
      i += run_length;  // Jump forward by the size of the run length.
    } else {
      // Output up to eight bytes without any encoding.
      const size_t len = std::min(static_cast<size_t>(8), str.length() - i);
      output.append(str, i, len);
      i += len;  // Jump forward by the amount of input consumed (usually 8).
    }
  }

  return output;
}

// static
// Uncompresses strings that were compresed with UniquePosition::Compress.
std::string UniquePosition::Uncompress(const std::string& str) {
  std::string output;
  size_t i = 0;
  // Iterate through the compressed string one block at a time.
  for (i = 0; i + 8 <= str.length(); i += 8) {
    if (IsRepeatedCharPrefix(str, i)) {
      // Found a repeated character block.  Expand it.
      const char rep_digit = str[i];
      uint32_t length = ReadEncodedRunLength(str, i + 4);
      output.append(length, rep_digit);
    } else {
      // Found a regular block.  Copy it.
      output.append(str, i, 8);
    }
  }
  // Copy the remaining bytes that were too small to form a block.
  output.append(str, i, std::string::npos);
  return output;
}

bool UniquePosition::IsValidCompressed(const std::string& str) {
  for (size_t i = 0; i + 8 <= str.length(); i += 8) {
    if (IsRepeatedCharPrefix(str, i)) {
      uint32_t count = ReadEncodedRunLength(str, i + 4);
      if (count < 4) {
        // A repeated character block should at least represent the four
        // characters that started it.
        return false;
      }
      if (str[i] == str[i + 4]) {
        // Does the next digit after a count match the repeated character?  Then
        // this is not the highest possible count.
        return false;
      }
    }
  }
  // We don't bother looking for the existence or checking the validity of
  // any partial blocks.  There's no way they could be invalid anyway.
  return true;
}

size_t UniquePosition::EstimateMemoryUsage() const {
  using base::trace_event::EstimateMemoryUsage;
  return EstimateMemoryUsage(compressed_);
}

}  // namespace syncer
