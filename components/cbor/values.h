// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CBOR_VALUES_H_
#define COMPONENTS_CBOR_VALUES_H_

#include <stdint.h>

#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/notreached.h"
#include "components/cbor/cbor_export.h"

namespace cbor {

// A class for Concise Binary Object Representation (CBOR) values.
// This does not support indefinite-length encodings.
class CBOR_EXPORT Value {
 public:
  struct Less {
    // Comparison predicate to order keys in a dictionary as required by the
    // canonical CBOR order defined in
    // https://tools.ietf.org/html/rfc7049#section-3.9
    // TODO(crbug.com/40560917): Clarify where this stands.
    bool operator()(const Value& a, const Value& b) const {
      // The current implementation only supports integer, text string, byte
      // string and invalid UTF8 keys.
      DCHECK((a.is_integer() || a.is_string() || a.is_bytestring() ||
              a.is_invalid_utf8()) &&
             (b.is_integer() || b.is_string() || b.is_bytestring() ||
              b.is_invalid_utf8()));

      // Below text from https://tools.ietf.org/html/rfc7049 errata 4409:
      // *  If the major types are different, the one with the lower value
      //    in numerical order sorts earlier.
      if (a.type() != b.type())
        return a.type() < b.type();

      // *  If two keys have different lengths, the shorter one sorts
      //    earlier;
      // *  If two keys have the same length, the one with the lower value
      //    in (byte-wise) lexical order sorts earlier.
      switch (a.type()) {
        case Type::UNSIGNED:
          // For unsigned integers, the smaller value has shorter length,
          // and (byte-wise) lexical representation.
          return a.GetInteger() < b.GetInteger();
        case Type::NEGATIVE:
          // For negative integers, the value closer to zero has shorter length,
          // and (byte-wise) lexical representation.
          return a.GetInteger() > b.GetInteger();
        case Type::STRING: {
          const auto& a_str = a.GetString();
          const size_t a_length = a_str.size();
          const auto& b_str = b.GetString();
          const size_t b_length = b_str.size();
          return std::tie(a_length, a_str) < std::tie(b_length, b_str);
        }
        case Type::BYTE_STRING: {
          const auto& a_str = a.GetBytestring();
          const size_t a_length = a_str.size();
          const auto& b_str = b.GetBytestring();
          const size_t b_length = b_str.size();
          return std::tie(a_length, a_str) < std::tie(b_length, b_str);
        }
        case Type::INVALID_UTF8: {
          const auto& a_str = a.GetInvalidUTF8();
          const size_t a_length = a_str.size();
          const auto& b_str = b.GetInvalidUTF8();
          const size_t b_length = b_str.size();
          return std::tie(a_length, a_str) < std::tie(b_length, b_str);
        }
        default:
          break;
      }

      NOTREACHED_IN_MIGRATION();
      return false;
    }

    using is_transparent = void;
  };

  using BinaryValue = std::vector<uint8_t>;
  using ArrayValue = std::vector<Value>;
  using MapValue = base::flat_map<Value, Value, Less>;

  enum class Type {
    UNSIGNED = 0,
    NEGATIVE = 1,
    BYTE_STRING = 2,
    STRING = 3,
    ARRAY = 4,
    MAP = 5,
    TAG = 6,
    SIMPLE_VALUE = 7,
    // In CBOR floating types also have major type 7, but we separate them here
    // for simplicity.
    FLOAT_VALUE = 70,
    NONE = -1,
    INVALID_UTF8 = -2,
  };

  enum class SimpleValue {
    FALSE_VALUE = 20,
    TRUE_VALUE = 21,
    NULL_VALUE = 22,
    UNDEFINED = 23,
  };

  // Returns a Value with Type::INVALID_UTF8. This factory method lets tests
  // encode such a value as a CBOR string. It should never be used outside of
  // tests since encoding may yield invalid CBOR data.
  static Value InvalidUTF8StringValueForTesting(std::string_view in_string);

  Value(Value&& that) noexcept;
  Value() noexcept;  // A NONE value.

  explicit Value(Type type);

  explicit Value(SimpleValue in_simple);
  explicit Value(bool boolean_value);
  explicit Value(double in_float);

  explicit Value(int integer_value);
  explicit Value(int64_t integer_value);
  explicit Value(uint64_t integer_value) = delete;

  explicit Value(base::span<const uint8_t> in_bytes);
  explicit Value(BinaryValue&& in_bytes) noexcept;

  explicit Value(const char* in_string, Type type = Type::STRING);
  explicit Value(std::string&& in_string, Type type = Type::STRING) noexcept;
  explicit Value(std::string_view in_string, Type type = Type::STRING);

  explicit Value(const ArrayValue& in_array);
  explicit Value(ArrayValue&& in_array) noexcept;

  explicit Value(const MapValue& in_map);
  explicit Value(MapValue&& in_map) noexcept;

  Value& operator=(Value&& that) noexcept;

  Value(const Value&) = delete;
  Value& operator=(const Value&) = delete;

  ~Value();

  // Value's copy constructor and copy assignment operator are deleted.
  // Use this to obtain a deep copy explicitly.
  Value Clone() const;

  // Returns the type of the value stored by the current Value object.
  Type type() const { return type_; }

  // Returns true if the current object represents a given type.
  bool is_type(Type type) const { return type == type_; }
  bool is_none() const { return type() == Type::NONE; }
  bool is_invalid_utf8() const { return type() == Type::INVALID_UTF8; }
  bool is_simple() const { return type() == Type::SIMPLE_VALUE; }
  bool is_bool() const {
    return is_simple() && (simple_value_ == SimpleValue::TRUE_VALUE ||
                           simple_value_ == SimpleValue::FALSE_VALUE);
  }
  bool is_double() const { return type() == Type::FLOAT_VALUE; }
  bool is_unsigned() const { return type() == Type::UNSIGNED; }
  bool is_negative() const { return type() == Type::NEGATIVE; }
  bool is_integer() const { return is_unsigned() || is_negative(); }
  bool is_bytestring() const { return type() == Type::BYTE_STRING; }
  bool is_string() const { return type() == Type::STRING; }
  bool is_array() const { return type() == Type::ARRAY; }
  bool is_map() const { return type() == Type::MAP; }

  // These will all fatally assert if the type doesn't match.
  SimpleValue GetSimpleValue() const;
  bool GetBool() const;
  double GetDouble() const;
  const int64_t& GetInteger() const;
  const int64_t& GetUnsigned() const;
  const int64_t& GetNegative() const;
  const BinaryValue& GetBytestring() const;
  std::string_view GetBytestringAsString() const;
  // Returned string may contain NUL characters.
  const std::string& GetString() const;
  const ArrayValue& GetArray() const;
  const MapValue& GetMap() const;
  const BinaryValue& GetInvalidUTF8() const;

 private:
  friend class Reader;
  // This constructor allows INVALID_UTF8 values to be created, which only
  // |Reader| and InvalidUTF8StringValueForTesting() may do.
  Value(base::span<const uint8_t> in_bytes, Type type);

  Type type_;

  union {
    SimpleValue simple_value_;
    int64_t integer_value_;
    double float_value_;
    BinaryValue bytestring_value_;
    std::string string_value_;
    ArrayValue array_value_;
    MapValue map_value_;
  };

  void InternalMoveConstructFrom(Value&& that);
  void InternalCleanup();
};

}  // namespace cbor

#endif  // COMPONENTS_CBOR_VALUES_H_
