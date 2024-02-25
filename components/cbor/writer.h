// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CBOR_WRITER_H_
#define COMPONENTS_CBOR_WRITER_H_

#include <stddef.h>
#include <stdint.h>

#include <optional>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/cbor/cbor_export.h"
#include "components/cbor/float_conversions.h"
#include "components/cbor/values.h"

// A basic Concise Binary Object Representation (CBOR) encoder as defined by
// https://tools.ietf.org/html/rfc7049. This is a generic encoder that supplies
// canonical, well-formed CBOR values but does not guarantee their validity
// (see https://tools.ietf.org/html/rfc7049#section-3.2).
// Supported:
//  * Major types:
//     * 0: Unsigned integers, up to INT64_MAX.
//     * 1: Negative integers, to INT64_MIN.
//     * 2: Byte strings.
//     * 3: UTF-8 strings.
//     * 4: Arrays, with the number of elements known at the start.
//     * 5: Maps, with the number of elements known at the start
//              of the container.
//     * 7: Simple values.
//
// Unsupported:
//  * Indefinite-length encodings.
//  * Parsing.
//
// Requirements for canonical CBOR as suggested by RFC 7049 are:
//  1) All major data types for the CBOR values must be as short as possible.
//      * Unsigned integer between 0 to 23 must be expressed in same byte as
//            the major type.
//      * 24 to 255 must be expressed only with an additional uint8_t.
//      * 256 to 65535 must be expressed only with an additional uint16_t.
//      * 65536 to 4294967295 must be expressed only with an additional
//            uint32_t. * The rules for expression of length in major types
//            2 to 5 follow the above rule for integers.
//  2) Keys in every map must be sorted (first by major type, then by key
//         length, then by value in byte-wise lexical order).
//  3) Indefinite length items must be converted to definite length items.
//  4) All maps must not have duplicate keys.
//
// Current implementation of Writer encoder meets all the requirements of
// canonical CBOR.

namespace cbor {

class CBOR_EXPORT Writer {
 public:
  // Default that should be sufficiently large for most use cases.
  static constexpr size_t kDefaultMaxNestingDepth = 16;

  struct CBOR_EXPORT Config {
    // Controls the maximum depth of CBOR nesting that will be permitted in a
    // Value. Nesting depth is defined as the number of arrays/maps that have to
    // be traversed to reach the most nested contained Value. Primitive values
    // and empty containers have nesting depths of 0.
    int max_nesting_level = kDefaultMaxNestingDepth;

    // Controls whether the Writer allows writing string values of type
    // Value::Type::INVALID_UTF8. Regular CBOR strings must be valid UTF-8.
    // Writers with this setting will produce invalid CBOR, so it may only be
    // enabled in tests.
    bool allow_invalid_utf8_for_testing = false;
  };

  Writer(const Writer&) = delete;
  Writer& operator=(const Writer&) = delete;

  ~Writer();

  // Returns the CBOR byte string representation of |node|, unless its nesting
  // depth is greater than |max_nesting_level|, in which case an empty optional
  // value is returned.
  static std::optional<std::vector<uint8_t>> Write(
      const Value& node,
      size_t max_nesting_level = kDefaultMaxNestingDepth);

  // A version of |Write| above that takes a Config.
  static std::optional<std::vector<uint8_t>> Write(const Value& node,
                                                   const Config& config);

 private:
  explicit Writer(std::vector<uint8_t>* cbor);

  // Called recursively to build the CBOR bytestring. When completed,
  // |encoded_cbor_| will contain the CBOR.
  bool EncodeCBOR(const Value& node,
                  int max_nesting_level,
                  bool allow_invalid_utf8);

  // Encodes the type and size of the data being added.
  void StartItem(Value::Type type, uint64_t size);

  // Encodes the additional information for the data.
  void SetAdditionalInformation(uint8_t additional_information);

  // Encodes an unsigned integer value. This is used to both write
  // unsigned integers and to encode the lengths of other major types.
  void SetUint(uint64_t value);

  // Returns the number of bytes needed to store the unsigned integer.
  size_t GetNumUintBytes(uint64_t value);

  // Holds the encoded CBOR data.
  raw_ptr<std::vector<uint8_t>> encoded_cbor_;
};

}  // namespace cbor

#endif  // COMPONENTS_CBOR_WRITER_H_
