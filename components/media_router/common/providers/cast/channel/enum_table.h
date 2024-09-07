// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef COMPONENTS_MEDIA_ROUTER_COMMON_PROVIDERS_CAST_CHANNEL_ENUM_TABLE_H_
#define COMPONENTS_MEDIA_ROUTER_COMMON_PROVIDERS_CAST_CHANNEL_ENUM_TABLE_H_

#include <cstdint>
#include <cstring>
#include <new>
#include <optional>
#include <ostream>
#include <string_view>

#include "base/check_op.h"
#include "base/notreached.h"
#include "build/build_config.h"

// TODO(crbug.com/1291730): Move this file to a more appropriate directory.
//
//
// A bidirectional mapping between enum values and strings.
//
// Typical Usage
// -------------
//
// The typical way to use this class is to define a specializtion of
// EnumTable<E>::instance for a specific enum type, which is initialized with
// a table of enum values and their corresponding strings:
//
//     // In .h file:
//     enum class MyEnum { kFoo, kBar, ..., kOther, kMaxValue = kOther };
//
//     // In .cc file:
//
//     template <>
//     constexpr EnumTable<MyEnum> EnumTable<MyEnum>::instance({
//         {MyEnum::kFoo, "FOO"},
//         {MyEnum::kBar, "BAR"},
//         ...
//         {MyEnum::kOther},  // Not all values need strings.
//       },
//       MyEnum::kMaxValue);  // Needed to detect missing table entries.
//
// The functions EnumToString() and StringToEnum() are used to look up values
// in the table.  In some cases, it may be useful to wrap these functions for
// special handling of unknown values, etc.:
//
//     const char* MyEnumToString(MyEnum value) {
//       return EnumToString(value).value_or(nullptr).data();
//     }
//
//     MyEnum StringToMyEnum(const std::string& name) {
//       return StringToEnum<MyEnum>(name).value_or(MyEnum::kOther);
//     }
//
// NOTE: Because it's a template specialization, the definition of
// EnumTable<E>::instance has to be in the cast_util namespace.  Alternatively,
// you can declare your own table with any name you want and call its members
// functions directly.
//
// To get a the string for an enum value that is known at compile time, there is
// a special zero-argument form of EnumToString(), (and a corresponding member
// function in EnumTable):
//
//     // Compiles to std::string_view("FOO").
//     EnumToString<MyEnum, MyEnum::kFoo>();
//
// The syntax is a little awkward, but it saves you from having to duplicate
// string literals or define each enum string as a named constant.
//
//
// Consecutive Enum Tables
// -----------------------
//
// When using an EnumTable, it's generally best for the following conditions
// to be true:
//
// - The enum is defined with "enum class" syntax.
//
// - The members have the default values assigned by the compiler.
//
// - There is an extra member named kMaxValue which is set equal to the highest
//   ordinary value.  (The Chromium style checker will verify that kMaxValue
//   really is the maximum value.)
//
// - The values in the EnumTable constructor appear in sorted order.
//
// - Every member of the enum (other than kMaxValue) is included in the table.
//
// When these conditions are met, the enum's |kMaxValue| member should be passed
// as the second argument of the EnumTable.  This will create a table where enum
// values can be converted to strings in constant time.  It will also reliably
// detect an incomplete enum table during startup of a debug build.
//
//
// Non-Consecutive Enum Tables
// ---------------------------
//
// When the conditions in the previous section cannot be satisfied, the second
// argument of the EnumTable constructor should be the special constant
// |NonConsecutiveEnumTable|.  Doing so has some unfortunate side-effects: there
// is no automatic way to detect when an enum value is missing from the table,
// and looking up a non-constant enum value requires a linear search.
//
//
// Why use an EnumTable?
// ---------------------
//
// If you're only doing one-way conversions from an enum to a string, using an
// EnumTable is almost identical to a switch statement.  Typically a switch
// statement that converts an enum value to a string will compile to a simple
// constant-time lookup in a static array, and a call to an EnumTable's
// GetString() method will compile to almost exactly the same code (with one
// additional shlq instruction compared to the switch statement).
//
// If your enum values are non-consecutive, the GetString() method will perform
// a linear search instead of an array lookup.  The search should be competitve
// in performance with a switch statement in this case.  To avoid getting this
// sub-optimal behavior by accident, you must explicitly request it with an
// extra argument to EnumTable's constructor.  If you don't do this, failure to
// put the values in the correct order will cause your unit tests to fail on
// startup with a decriptive error message.
//
// EnumTable really shines when you need to convert strings to enum values.  A
// typical implementation without EnumTable looks like this:
//
//     constexpr char kMyEnumFooString[] = "FOO";
//     constexpr char kMyEnumBarString[] = "BAR";
//     ...
//
//     MyEnum StringToMyEnum(const std::string& str) {
//       if (str == kMyEnumFooString) return MyEnum::kFoo;
//       if (str == kMyEnumBarString) return MyEnum::kBar;
//       ...
//       return kOther;
//     }
//
// If you roll your own solution, you can't do much better than this without
// jumping through some hoops.  Obvious improvements, like storing the data in a
// global base::flat_map, are off-limits because Chromium requires all global
// variables to have trivial destructors.  A simple chain of "if" statements
// works fine, but it has a number of drawbacks compared to an EnumTable:
//
// - You have to write and maintain a function separate from the one that
//   converts enum values to strings.  With an EnumTable, a single declaration
//   lets you convert in both directions.
//
// - Since you will almost certainly be using the strings in more than one
//   place, you have to either define a named constant for each one, or live
//   with the fact that the string literals are duplicated, causing maintenance
//   headaches.  With an EnumTable, each string naturally appears in only one
//   place, so there's no need for supplementary named constants.  When you need
//   to refer to the string for a particular enum value, you can get it directly
//   from the table at compile time using the zero-argument EnumToString()
//   function or the zero-argument EnumTable<E>::ToString() method.
//
// - If you want to follow best practices, you'll need to write unit tests for
//   your functions that convert between strings and enums.  When you use an
//   EnumTable, you don't write any functions, so there's nothing to test; all
//   the necessary testing is done by EnumTable's own unit tests.
//
// - The conversion function is essentially a fully-unrolled linear search
//   through all the possible string values.  This bloats your code and causes
//   unnecessary churn in the instruction cache.  Using an EnumTable, this
//   search is compiled into a single loop shared by all enums, which the
//   compiler can unroll, or not, as appropriate.  On 64-bit platforms, the main
//   table uses only 16 bytes per entry, so memory locality is excellent and
//   impact on the data cache is minimal.
//
// - The hand-written function performs a function call for each candidate
//   string.  EnumTable's search loop compares the lengths of the strings before
//   doing anything else, so most iterations don't call any functions and don't
//   access any memory aside from the lookup table itself.
//
// - If you accidentally duplicate any lines in the function, you're on your
//   own.  EnumTable detects duplicate strings on startup (in debug builds), so
//   you'll never accidentally try to map a string to two different enum values.

namespace cast_util {

class
#ifdef ARCH_CPU_64_BITS
    alignas(16)
#endif
        GenericEnumTableEntry {
 public:
  inline constexpr GenericEnumTableEntry(int32_t value);
  inline constexpr GenericEnumTableEntry(int32_t value, std::string_view str);

  GenericEnumTableEntry(const GenericEnumTableEntry&) = delete;
  GenericEnumTableEntry& operator=(const GenericEnumTableEntry&) = delete;

 private:
  static const GenericEnumTableEntry* FindByString(
      const GenericEnumTableEntry data[],
      std::size_t size,
      std::string_view str);
  static std::optional<std::string_view>
  FindByValue(const GenericEnumTableEntry data[], std::size_t size, int value);

  constexpr std::string_view str() const {
    DCHECK(has_str());
    return {chars, length};
  }

  constexpr bool has_str() const { return chars != nullptr; }

  // Instead of storing a std::string_view, it's broken apart and stored as a
  // pointer and an unsigned int (rather than a std::size_t) so that table
  // entries fit in 16 bytes with no padding on 64-bit platforms.
  const char* chars;
  uint32_t length;

  int32_t value;

  template <typename E>
  friend class EnumTable;
};

// Yes, these constructors really needs to be inlined.  Even though they look
// "complex" to the style checker, everything is executed at compile time, so an
// EnumTable instance can be fully initialized without executing any code.

inline constexpr GenericEnumTableEntry::GenericEnumTableEntry(int32_t value)
    : chars(nullptr), length(UINT32_MAX), value(value) {}

inline constexpr GenericEnumTableEntry::GenericEnumTableEntry(
    int32_t value,
    std::string_view str)
    : chars(str.data()),
      length(static_cast<uint32_t>(str.length())),
      value(value) {}

struct NonConsecutiveEnumTable_t {};
constexpr NonConsecutiveEnumTable_t NonConsecutiveEnumTable;

// A table for associating enum values with string literals.  This class is
// designed for use as an initialized global variable, so it has a trivial
// destructor and a simple constant-time constructor.
template <typename E>
class EnumTable {
 public:
  // DO NOT add any data members to this class, or it will break the
  // reinterpret_casts below.  If necessary, add new members to
  // GenericEnumTableEntry instead.
  class Entry : public GenericEnumTableEntry {
   public:
    // Constructor for placeholder entries with no string value.
    constexpr Entry(E value)
        : GenericEnumTableEntry(static_cast<int32_t>(value)) {}

    // Constructor for regular entries.
    constexpr Entry(E value, std::string_view str)
        : GenericEnumTableEntry(static_cast<int32_t>(value), str) {}

    Entry(const Entry&) = delete;
    Entry& operator=(const Entry&) = delete;
  };

  static_assert(sizeof(E) <= sizeof(int32_t),
                "Enum must not use more than 32 bits of storage.");
  static_assert(sizeof(Entry) == sizeof(GenericEnumTableEntry),
                "EnumTable<E>::Entry must not have its own member variables.");

  // Creates an EnumTable where data[i].value == i for all i.  When a table is
  // created with this constructor, the GetString() method is a simple array
  // lookup that runs in constant time.
  //
  // If |max_value| is specified, all enum values in |data| must be less than or
  // equal to |max_value|.  This feature is intended to help catch errors cause
  // by a new value being added to an enum without the new value being added to
  // the corresponding table.  For best results, use an enum class and create a
  // constant named kMaxValue.  For more details, see
  // https://www.chromium.org/developers/coding-style/chromium-style-checker-errors#TOC-Enumerator-max-values
  constexpr EnumTable(std::initializer_list<Entry> data, E max_value)
      : EnumTable(data, true) {
#ifndef NDEBUG
    // NOTE(jrw): This is compiled out when NDEBUG is defined, even if DCHECKS
    // are normally enabled because DCHECK_ALWAYS_ON is defined.  The reason for
    // this is that if DCHECKs are enabled, global EnumTable instances will have
    // a nontrivial constructor, which is flagged as a style violation by the
    // linux-rel trybot.
    auto found = FindNonConsecutiveEntry(data);
    DCHECK(!found) << "Entries' numerical values must be consecutive "
                   << "integers starting from 0; found problem at index "
                   << *found;

    const auto int_max_value = static_cast<int32_t>(max_value);
    DCHECK(data.end()[-1].value == int_max_value)
        << "Missing entry for enum value " << int_max_value;
#endif  // NDEBUG
  }

  // Creates an EnumTable where data[i].value != i for some values of i.  When
  // a table is created with this constructor, the GetString() method must
  // perform a linear search to find the correct string.
  constexpr EnumTable(std::initializer_list<Entry> data,
                      NonConsecutiveEnumTable_t)
      : EnumTable(data, false) {
#ifndef NDEBUG
    DCHECK(FindNonConsecutiveEntry(data))
        << "Don't use this constructor for sorted entries.";
#endif  // NDEBUG
  }

  EnumTable(const EnumTable&) = delete;
  EnumTable& operator=(const EnumTable&) = delete;

  // Gets the string associated with the given enum value.  When the argument
  // is a constant, prefer the zero-argument form below.
  inline std::optional<std::string_view> GetString(E value) const {
    if (is_sorted_) {
      const std::size_t index = static_cast<std::size_t>(value);
      if (ANALYZER_ASSUME_TRUE(index < data_.size())) {
        const auto& entry = data_.begin()[index];
        if (ANALYZER_ASSUME_TRUE(entry.has_str()))
          return entry.str();
      }
      return std::nullopt;
    }
    return GenericEnumTableEntry::FindByValue(
        reinterpret_cast<const GenericEnumTableEntry*>(data_.begin()),
        data_.size(), static_cast<int32_t>(value));
  }

  // This overload of GetString is designed for cases where the argument is a
  // literal value.  It will be evaluated at compile time in non-debug builds.
  //
  // If |Value| has no string defined in the table, you'll get an error at
  // runtime in debug builds, but in release builds this function will just
  // return a string holding an error message.  Unfortunately it doesn't seem
  // possible to report an error in evaluating a constexpr function at compile
  // time without using exceptions.
  template <E Value>
  constexpr std::string_view GetString() const {
    for (const auto& entry : data_) {
      if (entry.value == static_cast<int32_t>(Value) && entry.has_str())
        return entry.str();
    }

    NOTREACHED_IN_MIGRATION()
        << "No string for enum value: " << static_cast<int32_t>(Value);
    return "[invalid enum value]";
  }

  // Gets the enum value associated with the given string.  Unlike
  // GetString(), this method is not defined as a constexpr, because it should
  // never be called with a literal string; it's simpler to just refer to the
  // enum value directly.
  std::optional<E> GetEnum(std::string_view str) const {
    auto* entry = GenericEnumTableEntry::FindByString(
        reinterpret_cast<const GenericEnumTableEntry*>(data_.begin()),
        data_.size(), str);
    return entry ? static_cast<E>(entry->value) : std::optional<E>();
  }

  // The default instance of this class.  There should normally only be one
  // instance of this class for a given enum type.  Users of this class are
  // responsible for providing a suitable definition for each enum type if the
  // EnumToString() or StringToEnum() functions are used.
  static const EnumTable& GetInstance();

 private:
#ifdef ARCH_CPU_64_BITS
  alignas(std::hardware_destructive_interference_size)
#endif
      std::initializer_list<Entry> data_;
  bool is_sorted_;

  constexpr EnumTable(std::initializer_list<Entry> data, bool is_sorted)
      : data_(data), is_sorted_(is_sorted) {
#ifndef NDEBUG
    // If the table is too large, it's probably time to update this class to do
    // something smarter than a linear search.
    DCHECK(data.size() <= 32) << "Table too large.";

    for (std::size_t i = 0; i < data.size(); i++) {
      for (std::size_t j = i + 1; j < data.size(); j++) {
        const Entry& ei = data.begin()[i];
        const Entry& ej = data.begin()[j];
        DCHECK(ei.value != ej.value)
            << "Found duplicate enum values at indices " << i << " and " << j;
        DCHECK(!(ei.has_str() && ej.has_str() && ei.str() == ej.str()))
            << "Found duplicate strings at indices " << i << " and " << j;
      }
    }
#endif  // NDEBUG
  }

#ifndef NDEBUG
  // Finds and returns the first i for which data[i].value != i;
  constexpr static std::optional<std::size_t> FindNonConsecutiveEntry(
      std::initializer_list<Entry> data) {
    int32_t counter = 0;
    for (const auto& entry : data) {
      if (entry.value != counter) {
        return counter;
      }
      ++counter;
    }
    return {};
  }
#endif  // NDEBUG
};

// Converts an enum value to a string using the default table
// (EnumTable<E>::instance) for the given enum type.
template <typename E>
inline std::optional<std::string_view> EnumToString(E value) {
  return EnumTable<E>::GetInstance().GetString(value);
}

// Converts a literal enum value to a string at compile time using the default
// table (EnumTable<E>::GetInstance()) for the given enum type.
//
// TODO(crbug.com/1291730): Once C++17 features are allowed, change this
// function to have only one template parameter:
//
//   template <auto Value>
//   inline std::string_view EnumToString() {
//     return EnumTable<decltype(Value)
//         >::GetInstance().template GetString<Value>();
//   }
//
template <typename E, E Value>
inline std::string_view EnumToString() {
  return EnumTable<E>::GetInstance().template GetString<Value>();
}

// Converts a string to an enum value using the default table
// (EnumTable<E>::instance) for the given enum type.
template <typename E>
inline std::optional<E> StringToEnum(std::string_view str) {
  return EnumTable<E>::GetInstance().GetEnum(str);
}

}  // namespace cast_util

#endif  // COMPONENTS_MEDIA_ROUTER_COMMON_PROVIDERS_CAST_CHANNEL_ENUM_TABLE_H_
