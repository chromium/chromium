// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CBOR_READER_H_
#define COMPONENTS_CBOR_READER_H_

#include <stddef.h>

#include <optional>

#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_span.h"
#include "components/cbor/cbor_buildflags.h"
#include "components/cbor/cbor_export.h"
#include "components/cbor/values.h"

// TODO(crbug.com/536539387): Once Crubit fixes -Wnullability-completeness, we
// can include cbor_rust.h in reader.h instead of reader.cc and assign the
// values of DecoderError directly from the Rust Tag enum.
#if BUILDFLAG(USE_CBOR_RUST)
// Included in reader.cc instead: #include "components/cbor/rust/cbor_rust.h"
#endif

// Concise Binary Object Representation (CBOR) decoder as defined by
// https://tools.ietf.org/html/rfc7049. This decoder only accepts canonical CBOR
// as defined by section 3.9.
//
// This implementation supports the following major types:
//  - 0: Unsigned integers, up to 64-bit values*.
//  - 1: Signed integers, up to 64-bit values*.
//  - 2: Byte strings.
//  - 3: UTF-8 strings.
//  - 4: Definite-length arrays.
//  - 5: Definite-length maps.
//  - 7: Simple values.
//
//  * Note: For simplicity, this implementation represents both signed and
//    unsigned integers with signed int64_t. This reduces the effective range
//    of unsigned integers.
//
// Requirements for canonical CBOR representation:
//  - Duplicate keys in maps are not allowed.
//  - Keys for maps must be sorted first by length and then by byte-wise
//    lexical order, as defined in Section 3.9.
//
// Known limitations and interpretations of the RFC (and the reasons):
//  - Does not support indefinite-length data streams or semantic tags (major
//    type 6). (Simplicity; security)
//  - Does not support the floating point and BREAK stop code value types in
//    major type 7. (Simplicity)
//  - Does not support non-character codepoints in major type 3. (Security)
//  - Treats incomplete CBOR data items as syntax errors. (Security)
//  - Treats trailing data bytes as errors. (Security)
//  - Treats unknown additional information formats as syntax errors.
//    (Simplicity; security)
//  - Limits CBOR value inputs to at most 16 layers of nesting. Callers can
//    enforce more shallow nesting by setting |max_nesting_level|. (Efficiency;
//    security)
//  - Only supports CBOR maps with integer or string type keys, due to the
//    cost of serialization when sorting map keys. (Efficiency; simplicity)
//  - Does not support simple values that are unassigned/reserved as per RFC
//    7049, and treats them as errors. (Security)

namespace cbor {

CBOR_EXPORT BASE_DECLARE_FEATURE(kUseRustCborParser);

// TODO(crbug.com/535682335): Remove `#if BUILDFLAG(USE_CBOR_RUST)` macros and
// unconditionally use rust types once Cronet supports Crubit dependencies.
#if BUILDFLAG(USE_CBOR_RUST)
namespace rust {
struct Value;
}
#endif

class CBOR_EXPORT Reader {
 public:
  // TODO(crbug.com/536539387): Once Crubit fixes -Wnullability-completeness, we
  // can include cbor_rust.h in reader.h and assign the values of DecoderError
  // directly from the Rust Tag enum.
  enum class DecoderError {
    // LINT.IfChange(DecoderError)
    CBOR_NO_ERROR = 0,
    UNSUPPORTED_MAJOR_TYPE = 1,
    UNKNOWN_ADDITIONAL_INFO = 2,
    INCOMPLETE_CBOR_DATA = 3,
    INCORRECT_MAP_KEY_TYPE = 4,
    TOO_MUCH_NESTING = 5,
    INVALID_UTF8 = 6,
    EXTRANEOUS_DATA = 7,
    OUT_OF_ORDER_KEY = 8,
    NON_MINIMAL_CBOR_ENCODING = 9,
    UNSUPPORTED_SIMPLE_VALUE = 10,
    UNSUPPORTED_FLOATING_POINT_VALUE = 11,
    OUT_OF_RANGE_INTEGER_VALUE = 12,
    DUPLICATE_KEY = 13,
    UNKNOWN_ERROR = 14,
    // LINT.ThenChange(//components/cbor/rust/reader.rs:ErrorCode,//components/cbor/reader.cc:DecoderErrorAsserts)
  };

  // CBOR nested depth sufficient for most use cases.
  static const int kCBORMaxDepth = 16;

  // Config contains configuration for a CBOR parsing operation.
  struct CBOR_EXPORT Config {
    Config();

    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;

    ~Config();

    // Used to report the number of bytes of input consumed. This suppresses the
    // |EXTRANEOUS_DATA| error case. May be nullptr.
    raw_ptr<size_t> num_bytes_consumed = nullptr;

    // Used to report the specific error in the case that parsing fails. May be
    // nullptr;
    raw_ptr<DecoderError> error_code_out = nullptr;

    // Controls the maximum depth of CBOR nesting that will be permitted. This
    // exists to control stack consumption during parsing.
    size_t max_nesting_level = kCBORMaxDepth;

    // Causes strings that are not valid UTF-8 to be accepted and suppresses the
    // |INVALID_UTF8| error, unless such strings are map keys. Invalid strings
    // will result in Values of type |INVALID_UTF8| rather than |STRING|. Users
    // of this feature should ensure that every invalid string is accounted for
    // in the resulting structure.
    //
    // (Map keys are not allowed to be invalid because it was not necessary for
    // the motivating case and because it adds complexity to handle the ordering
    // correctly.)
    bool allow_invalid_utf8 = false;

    // Uses the rust parser instead of the C++ parser.
    bool use_rust;
  };

  Reader(const Reader&) = delete;
  Reader& operator=(const Reader&) = delete;

  ~Reader();

  // Reads and parses |input_data| into a Value. Returns an empty Optional
  // if the input violates any one of the syntax requirements (including unknown
  // additional info and incomplete CBOR data).
  //
  // The caller can optionally provide |error_code_out| to obtain additional
  // information about decoding failures.
  //
  // If the caller provides it, |max_nesting_level| cannot exceed
  // |kCBORMaxDepth|.
  //
  // Returns an empty Optional if not all the data was consumed, and sets
  // |error_code_out| to EXTRANEOUS_DATA in this case.
  static std::optional<Value> Read(base::span<const uint8_t> input_data,
                                   DecoderError* error_code_out = nullptr,
                                   int max_nesting_level = kCBORMaxDepth);

  // A version of |Read|, above, that takes a |Config| structure to allow
  // additional controls.
  static std::optional<Value> Read(base::span<const uint8_t> input_data,
                                   const Config& config);

  // A version of |Read| that takes some fields of |Config| as parameters to
  // avoid having to construct a |Config| object explicitly.
  static std::optional<Value> Read(base::span<const uint8_t> input_data,
                                   size_t* num_bytes_consumed,
                                   DecoderError* error_code_out = nullptr,
                                   int max_nesting_level = kCBORMaxDepth);

  // Translates errors to human-readable error messages.
  static const char* ErrorCodeToString(DecoderError error_code);

 private:
  explicit Reader(base::span<const uint8_t> data);

  // Encapsulates information extracted from the header of a CBOR data item,
  // which consists of the initial byte, and a variable-length-encoded integer
  // (if any).
  struct DataItemHeader {
    // The major type decoded from the initial byte.
    Value::Type type;

    // The raw 5-bit additional information from the initial byte.
    uint8_t additional_info;

    // The integer |value| decoded from the |additional_info| and the
    // variable-length-encoded integer, if any.
    uint64_t value;
  };

  std::optional<DataItemHeader> DecodeDataItemHeader();
  std::optional<Value> DecodeCompleteDataItem(const Config& config,
                                              int max_nesting_level);
  std::optional<Value> DecodeValueToNegative(uint64_t value);
  std::optional<Value> DecodeValueToUnsigned(uint64_t value);
  std::optional<Value> DecodeToSimpleValue(const DataItemHeader& header);
  std::optional<uint64_t> ReadVariadicLengthInteger(uint8_t additional_info);
  std::optional<Value> ReadByteStringContent(const DataItemHeader& header);
  std::optional<Value> ReadStringContent(const DataItemHeader& header,
                                         const Config& config);
  std::optional<Value> ReadArrayContent(const DataItemHeader& header,
                                        const Config& config,
                                        int max_nesting_level);
  std::optional<Value> ReadMapContent(const DataItemHeader& header,
                                      const Config& config,
                                      int max_nesting_level);
  std::optional<uint8_t> ReadByte();
  std::optional<base::span<const uint8_t>> ReadBytes(uint64_t num_bytes);
  bool IsEncodingMinimal(uint8_t additional_bytes, uint64_t uint_data);

  DecoderError GetErrorCode() { return error_code_; }

#if BUILDFLAG(USE_CBOR_RUST)
  static Value ConvertRustValueToCpp(const cbor::rust::Value& rust_val);
#endif

  size_t num_bytes_remaining() const { return rest_.size(); }

  base::raw_span<const uint8_t> rest_;
  DecoderError error_code_;
};

}  // namespace cbor

#endif  // COMPONENTS_CBOR_READER_H_
