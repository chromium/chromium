// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CBOR_READER_H_
#define COMPONENTS_CBOR_READER_H_

#include <stddef.h>

#include <map>
#include <optional>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_span.h"
#include "components/cbor/cbor_export.h"
#include "components/cbor/values.h"

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
//  - 7: Simple values or floating point values.
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

class CBOR_EXPORT Reader {
 public:
  enum class DecoderError {
    CBOR_NO_ERROR = 0,
    UNSUPPORTED_MAJOR_TYPE,
    UNKNOWN_ADDITIONAL_INFO,
    INCOMPLETE_CBOR_DATA,
    INCORRECT_MAP_KEY_TYPE,
    TOO_MUCH_NESTING,
    INVALID_UTF8,
    EXTRANEOUS_DATA,
    OUT_OF_ORDER_KEY,
    NON_MINIMAL_CBOR_ENCODING,
    UNSUPPORTED_SIMPLE_VALUE,
    UNSUPPORTED_FLOATING_POINT_VALUE,
    OUT_OF_RANGE_INTEGER_VALUE,
    DUPLICATE_KEY,
    UNKNOWN_ERROR,
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
    raw_ptr<DecoderError, DanglingUntriaged> error_code_out = nullptr;

    // Controls the maximum depth of CBOR nesting that will be permitted. This
    // exists to control stack consumption during parsing.
    int max_nesting_level = kCBORMaxDepth;

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

    // Causes an input to be accepted even if it contains one or more maps with
    // keys that are not in the canonical ordering as defined in Section 3.9,
    // and suppresses the OUT_OF_ORDER_KEY error. The original ordering of keys
    // will _not_ be preserved, but instead, in the returned cbor::Value, all
    // maps are re-sorted so that their keys are in canonical order. By
    // definition, enabling this option may result in loss of information (i.e.
    // the original key ordering).
    //
    // Enabling this option will still not allow duplicate keys, in case of
    // which the DUPLICATE_KEY error will be emitted.
    bool allow_and_canonicalize_out_of_order_keys = false;

    // Causes floating point in CBOR to be decoded. This is an option as
    // several users of this library do not want to accept floats in CBOR. When
    // this option is set to `false` any floating point values encountered
    // during decoding will set raise the `UNSUPPORTED_FLOATING_POINT_VALUE`
    // error.
    bool allow_floating_point = false;
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
  std::optional<Value> DecodeToSimpleValueOrFloat(const DataItemHeader& header,
                                                  const Config& config);
  std::optional<uint64_t> ReadVariadicLengthInteger(Value::Type type,
                                                    uint8_t additional_info);
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
  bool IsKeyInOrder(const Value& new_key,
                    const std::map<Value, Value, Value::Less>& map);
  // Check if `new_key` is a duplicate of a key that already exists in the
  // `map`.
  bool IsDuplicateKey(const Value& new_key,
                      const std::map<Value, Value, Value::Less>& map);
  bool IsEncodingMinimal(uint8_t additional_bytes, uint64_t uint_data);

  DecoderError GetErrorCode() { return error_code_; }

  size_t num_bytes_remaining() const { return rest_.size(); }

  base::raw_span<const uint8_t> rest_;
  DecoderError error_code_;
};

}  // namespace cbor

#endif  // COMPONENTS_CBOR_READER_H_
