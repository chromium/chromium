// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CBOR_VALUES_H_
#define COMPONENTS_CBOR_VALUES_H_

#include <stdint.h>

#include <iosfwd>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "components/cbor/cbor_export.h"

namespace cbor {

// A class for Concise Binary Object Representation (CBOR) values.
// This does not support indefinite-length encodings.
class CBOR_EXPORT Value {
 public:
  struct CBOR_EXPORT Less {
    // Comparison predicate to order keys in a dictionary as required by the
    // CTAP2 canonical CBOR encoding form:
    // https://fidoalliance.org/specs/fido-v2.0-ps-20190130/fido-client-to-authenticator-protocol-v2.0-ps-20190130.html#ctap2-canonical-cbor-encoding-form
    bool operator()(const Value& a, const Value& b) const;

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
    // TAG = 6, but not actually supported.
    SIMPLE_VALUE = 7,
    INVALID_UTF8 = -2,
  };

  enum class SimpleValue {
    FALSE_VALUE = 20,
    TRUE_VALUE = 21,
    NULL_VALUE = 22,
    UNDEFINED = 23,

    kMinValue = FALSE_VALUE,
    kMaxValue = UNDEFINED,
  };

  struct Null final {
    constexpr explicit Null() = default;

    friend bool operator==(const Null&, const Null&) { return true; }
  };
  static constexpr Null null;

  struct Undefined final {
    constexpr explicit Undefined() = default;

    friend bool operator==(const Undefined&, const Undefined&) { return true; }
  };
  static constexpr Undefined undefined;

  struct InvalidUTF8 final {
    BinaryValue bytes;

    friend bool operator==(const InvalidUTF8&, const InvalidUTF8&) = default;
  };

  // Returns a Value with Type::INVALID_UTF8. This factory method lets tests
  // encode such a value as a CBOR string. It should never be used outside of
  // tests since encoding may yield invalid CBOR data.
  static Value InvalidUTF8StringValueForTesting(std::string_view in_string);

  // Use std::optional<Value> to represent an absent value.
  Value() = delete;

  explicit Value(bool boolean_value) noexcept;
  explicit Value(Null) noexcept;
  explicit Value(Undefined) noexcept;

  explicit Value(float float_value) = delete;
  explicit Value(double float_value) = delete;

  explicit Value(int integer_value);
  explicit Value(int64_t integer_value) noexcept;
  explicit Value(uint64_t integer_value) = delete;

  // Constructors for `Type::BYTE_STRING`.
  explicit Value(base::span<const uint8_t> in_bytes);
  explicit Value(BinaryValue&& in_bytes) noexcept;

  // Constructors for `Type::STRING`.
  explicit Value(const char* in_string);
  explicit Value(std::string&& in_string) noexcept;
  explicit Value(std::string_view in_string);

  explicit Value(const ArrayValue& in_array);
  explicit Value(ArrayValue&& in_array) noexcept;

  explicit Value(const MapValue& in_map);
  explicit Value(MapValue&& in_map) noexcept;

  // Prevent pointers from implicitly converting to `bool`.
  template <typename T>
  explicit Value(const T*) = delete;

  Value(Value&&) noexcept;
  Value& operator=(Value&&) noexcept;

  Value(const Value&) = delete;
  Value& operator=(const Value&) = delete;

  ~Value();

  // Value's copy constructor and copy assignment operator are deleted.
  // Use this to obtain a deep copy explicitly.
  Value Clone() const;

  // Returns the type of the value stored by the current Value object.
  Type type() const;

  bool is_invalid_utf8() const {
    return std::holds_alternative<InvalidUTF8>(data_);
  }
  bool is_simple() const { return is_bool() || is_null() || is_undefined(); }
  bool is_bool() const { return std::holds_alternative<bool>(data_); }
  bool is_unsigned() const { return is_integer() && GetInteger() >= 0; }
  bool is_negative() const { return is_integer() && GetInteger() < 0; }
  bool is_integer() const { return std::holds_alternative<int64_t>(data_); }
  bool is_bytestring() const {
    return std::holds_alternative<BinaryValue>(data_);
  }
  bool is_string() const { return std::holds_alternative<std::string>(data_); }
  bool is_array() const { return std::holds_alternative<ArrayValue>(data_); }
  bool is_map() const { return std::holds_alternative<MapValue>(data_); }
  bool is_null() const { return std::holds_alternative<Null>(data_); }
  bool is_undefined() const { return std::holds_alternative<Undefined>(data_); }

  // These will all fatally assert if the type doesn't match.
  // Deprecated: Use `GetBool()` or `is_null()` or `is_undefined()`.
  SimpleValue GetSimpleValue() const;
  bool GetBool() const;
  int64_t GetInteger() const;
  int64_t GetUnsigned() const;
  int64_t GetNegative() const;
  const BinaryValue& GetBytestring() const LIFETIME_BOUND;
  std::string_view GetBytestringAsString() const LIFETIME_BOUND;
  // Returned string may contain NUL characters.
  const std::string& GetString() const LIFETIME_BOUND;
  const ArrayValue& GetArray() const LIFETIME_BOUND;
  ArrayValue& GetArray() LIFETIME_BOUND;
  const MapValue& GetMap() const LIFETIME_BOUND;
  MapValue& GetMap() LIFETIME_BOUND;
  const BinaryValue& GetInvalidUTF8() const LIFETIME_BOUND;

  template <typename Visitor>
  auto Visit(Visitor&& visitor) const {
    return std::visit(std::forward<Visitor>(visitor), data_);
  }

  CBOR_EXPORT friend bool operator==(const Value&, const Value&);

 private:
  friend class Reader;

  struct invalid_utf8_t final {
    constexpr explicit invalid_utf8_t() = default;
  };
  static constexpr invalid_utf8_t invalid_utf8;

  // This constructor creates `INVALID_UTF8` values, which only
  // `Reader` and `InvalidUTF8StringValueForTesting()` may do.
  Value(invalid_utf8_t, base::span<const uint8_t> in_bytes);

  using Storage = std::variant<Null,
                               Undefined,
                               bool,
                               int64_t,
                               BinaryValue,
                               std::string,
                               ArrayValue,
                               MapValue,
                               InvalidUTF8>;

  Storage data_;
};

// Stream operators so for pretty-printing in tests.
CBOR_EXPORT std::ostream& operator<<(std::ostream&, const Value&);
CBOR_EXPORT std::ostream& operator<<(std::ostream&, Value::Type);

}  // namespace cbor

#endif  // COMPONENTS_CBOR_VALUES_H_
