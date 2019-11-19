// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRASH_CORE_COMMON_CRASH_KEY_H_
#define COMPONENTS_CRASH_CORE_COMMON_CRASH_KEY_H_

#include <stdint.h>

#include <string>

#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "build/build_config.h"
#include "components/crash/core/common/crash_buildflags.h"
#include "components/crash/core/common/crash_export.h"

// The crash key interface exposed by this file is the same as the Crashpad
// Annotation interface. Because not all platforms use Crashpad yet, a
// source-compatible interface is provided on top of the older Breakpad
// storage mechanism.
#if BUILDFLAG(USE_CRASHPAD_ANNOTATION) || BUILDFLAG(USE_COMBINED_ANNOTATIONS)
#include "third_party/crashpad/crashpad/client/annotation.h"  // nogncheck
#endif

namespace base {
namespace debug {
class StackTrace;
}  // namespace debug
}  // namespace base

namespace crash_reporter {

class CrashKeyBreakpadTest;

// A CrashKeyString stores a name-value pair that will be recorded within a
// crash report.
//
// The crash key name must be a constant string expression, and the value
// should be unique and identifying. The maximum size for the value is
// specified as the template argument, and values greater than this are
// truncated. When specifying a value size, space should be left for the
// `NUL` byte. Crash keys should be declared with static storage duration.
//
// Examples:
// \code
//    // This crash key is only set in one function:
//    void DidNavigate(const GURL& gurl) {
//      static crash_reporter::CrashKeyString<256> url_key("url");
//      url_key.Set(gurl.ToString());
//    }
//
//    // This crash key can be set/cleared across different functions:
//    namespace {
//    crash_reporter::CrashKeyString<32> g_operation_id("operation-req-id");
//    }
//
//    void OnStartingOperation(const std::string& request_id) {
//      g_operation_id.Set(request_id);
//    }
//
//    void OnEndingOperation() {
//      g_operation_id.Clear()
//    }
// \endcode
#if BUILDFLAG(USE_CRASHPAD_ANNOTATION)

template <crashpad::Annotation::ValueSizeType MaxLength>
using CrashKeyString = crashpad::StringAnnotation<MaxLength>;

#else  // Crashpad-compatible crash key interface:

namespace internal {

constexpr size_t kCrashKeyStorageNumEntries = 200;
constexpr size_t kCrashKeyStorageValueSize = 128;

// Base implementation of a CrashKeyString for non-Crashpad clients. A separate
// base class is used to avoid inlining complex logic into the public template
// API.
class CRASH_KEY_EXPORT CrashKeyStringImpl {
 public:
  constexpr explicit CrashKeyStringImpl(const char name[],
                                        size_t* index_array,
                                        size_t index_array_count)
      : name_(name),
        index_array_(index_array),
        index_array_count_(index_array_count) {}

  void Set(base::StringPiece value);
  void Clear();

  bool is_set() const;

 private:
  friend class crash_reporter::CrashKeyBreakpadTest;

  // The name of the crash key.
  const char* const name_;

  // If the crash key is set, this is the index into the storage that can be
  // used to set/clear the key without requiring a linear scan of the storage
  // table. This will be |num_entries| if unset.
  size_t* index_array_;
  size_t index_array_count_;

  DISALLOW_COPY_AND_ASSIGN(CrashKeyStringImpl);
};

// This type creates a C array that is initialized with a specific default
// value, rather than the standard zero-initialized default.
template <typename T,
          size_t TotalSize,
          T DefaultValue,
          size_t Count,
          T... Values>
struct InitializedArrayImpl {
  using Type = typename InitializedArrayImpl<T,
                                             TotalSize,
                                             DefaultValue,
                                             Count - 1,
                                             DefaultValue,
                                             Values...>::Type;
};

template <typename T, size_t TotalSize, T DefaultValue, T... Values>
struct InitializedArrayImpl<T, TotalSize, DefaultValue, 0, Values...> {
  using Type = InitializedArrayImpl<T, TotalSize, DefaultValue, 0, Values...>;
  T data[TotalSize]{Values...};
};

template <typename T, size_t ArraySize, T DefaultValue>
using InitializedArray =
    typename InitializedArrayImpl<T, ArraySize, DefaultValue, ArraySize>::Type;

}  // namespace internal

template <uint32_t MaxLength>
class CrashKeyStringBreakpad : public internal::CrashKeyStringImpl {
 public:
  constexpr static size_t chunk_count =
      (MaxLength / internal::kCrashKeyStorageValueSize) + 1;

  // A constructor tag that can be used to initialize a C array of crash keys.
  enum class Tag { kArray };

  constexpr explicit CrashKeyStringBreakpad(const char name[])
      : internal::CrashKeyStringImpl(name, indexes_.data, chunk_count) {}

  constexpr CrashKeyStringBreakpad(const char name[], Tag tag)
      : CrashKeyStringBreakpad(name) {}

 private:
  // Indexes into the TransitionalCrashKeyStorage for when a value is set.
  // See the comment in CrashKeyStringImpl for details.
  // An unset index in the storage is represented by a sentinel value, which
  // is the total number of entries. This will initialize the array with
  // that sentinel value as a compile-time expression.
  internal::InitializedArray<size_t,
                             chunk_count,
                             internal::kCrashKeyStorageNumEntries>
      indexes_;

  DISALLOW_COPY_AND_ASSIGN(CrashKeyStringBreakpad);
};

#if BUILDFLAG(USE_COMBINED_ANNOTATIONS)

namespace internal {

class CrashKeyStringCombinedImpl {
 public:
  constexpr CrashKeyStringCombinedImpl(CrashKeyStringImpl* breakpad_key,
                                       crashpad::Annotation* crashpad_key)
      : breakpad_key_(breakpad_key), crashpad_key_(crashpad_key) {}

  void Clear() {
    breakpad_key_->Clear();
    crashpad_key_->Clear();
  }

  bool is_set() const { return breakpad_key_->is_set(); }

 private:
  CrashKeyStringImpl* breakpad_key_;
  crashpad::Annotation* crashpad_key_;

  DISALLOW_COPY_AND_ASSIGN(CrashKeyStringCombinedImpl);
};

}  // namespace internal

template <uint32_t MaxLength>
class CrashKeyStringCombined : public internal::CrashKeyStringCombinedImpl {
 public:
  enum class Tag { kArray };

  constexpr explicit CrashKeyStringCombined(const char name[])
      : internal::CrashKeyStringCombinedImpl(&breakpad_key_, &crashpad_key_),
        breakpad_key_(name),
        crashpad_key_(name) {}

  constexpr CrashKeyStringCombined(const char name[], Tag tag)
      : CrashKeyStringCombined(name) {}

  void Set(base::StringPiece value) {
    breakpad_key_.Set(value);
    crashpad_key_.Set(value);
  }

 private:
  CrashKeyStringBreakpad<MaxLength> breakpad_key_;
  crashpad::StringAnnotation<MaxLength> crashpad_key_;

  DISALLOW_COPY_AND_ASSIGN(CrashKeyStringCombined);
};

template <uint32_t MaxLength>
using CrashKeyString = CrashKeyStringCombined<MaxLength>;
#else
template <uint32_t MaxLength>
using CrashKeyString = CrashKeyStringBreakpad<MaxLength>;
#endif  // BUILDFLAG(USE_COMBINED_ANNOTATIONS)

#endif  // BUILDFLAG(USE_CRASHPAD_ANNOTATION)

// This scoper clears the specified annotation's value when it goes out of
// scope.
//
// Example:
//    void DoSomething(const std::string& data) {
//      static crash_reporter::CrashKeyString<32> crash_key("DoSomething-data");
//      crash_reporter::ScopedCrashKeyString auto_clear(&crash_key, data);
//
//      DoSomethignImpl(data);
//    }
class ScopedCrashKeyString {
 public:
#if BUILDFLAG(USE_CRASHPAD_ANNOTATION)
  using CrashKeyType = crashpad::Annotation;
#elif BUILDFLAG(USE_COMBINED_ANNOTATIONS)
  using CrashKeyType = internal::CrashKeyStringCombinedImpl;
#else
  using CrashKeyType = internal::CrashKeyStringImpl;
#endif

  template <class T>
  ScopedCrashKeyString(T* crash_key, base::StringPiece value)
      : crash_key_(crash_key) {
    crash_key->Set(value);
  }

  ~ScopedCrashKeyString() { crash_key_->Clear(); }

 private:
  CrashKeyType* const crash_key_;
  DISALLOW_COPY_AND_ASSIGN(ScopedCrashKeyString);
};

namespace internal {
// Formats a stack trace into a string whose length will not exceed
// |max_length|. This function ensures no addresses are truncated when
// being formatted.
CRASH_KEY_EXPORT std::string FormatStackTrace(
    const base::debug::StackTrace& trace,
    size_t max_length);
}  // namespace internal

// Formats a base::debug::StackTrace as a string of space-separated hexadecimal
// numbers and stores it in a CrashKeyString.
// TODO(rsesek): When all clients use Crashpad, traces should become a first-
// class Annotation type rather than being forced through string conversion.
template <uint32_t Size>
void SetCrashKeyStringToStackTrace(CrashKeyString<Size>* key,
                                   const base::debug::StackTrace& trace) {
  std::string trace_string = internal::FormatStackTrace(trace, Size);
  key->Set(trace_string);
}

// Initializes the crash key subsystem if it is required.
CRASH_KEY_EXPORT void InitializeCrashKeys();

#if defined(UNIT_TEST) || defined(CRASH_CORE_COMMON_IMPLEMENTATION)
// Returns a value for the crash key named |key_name|. For Crashpad-based
// clients, this returns the first instance found of the name.
// Note: In a component build, this will only retrieve crash keys for the
// current component.
CRASH_KEY_EXPORT std::string GetCrashKeyValue(const std::string& key_name);

// Initializes the crash key subsystem with testing configuration if it is
// required.
CRASH_KEY_EXPORT void InitializeCrashKeysForTesting();

// Resets crash key state and, depending on the platform, de-initializes
// the system.
CRASH_KEY_EXPORT void ResetCrashKeysForTesting();
#endif

}  // namespace crash_reporter

#endif  // COMPONENTS_CRASH_CORE_COMMON_CRASH_KEY_H_
