// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/core/common/crash_key_base_support.h"

#include <memory>
#include <ostream>
#include <string_view>

#include "base/check_op.h"
#include "base/debug/crash_logging.h"
#include "components/crash/core/common/crash_key.h"

#if BUILDFLAG(USE_CRASHPAD_ANNOTATION)
#include "third_party/crashpad/crashpad/client/annotation_list.h"  // nogncheck
#endif

namespace crash_reporter {

namespace {

// This stores the value for a crash key allocated through the //base API.
template <uint32_t ValueSize>
struct BaseCrashKeyString : public base::debug::CrashKeyString {
  BaseCrashKeyString(const char name[], base::debug::CrashKeySize size)
      : base::debug::CrashKeyString(name, size), impl(name) {
    DCHECK_EQ(static_cast<uint32_t>(size), ValueSize);
  }
  crash_reporter::CrashKeyString<ValueSize> impl;
};

#define SIZE_CLASS_OPERATION(size_class, operation_prefix, operation_suffix) \
  switch (size_class) {                                                      \
    case base::debug::CrashKeySize::Size32:                                  \
      operation_prefix BaseCrashKeyString<32> operation_suffix;              \
      break;                                                                 \
    case base::debug::CrashKeySize::Size64:                                  \
      operation_prefix BaseCrashKeyString<64> operation_suffix;              \
      break;                                                                 \
    case base::debug::CrashKeySize::Size256:                                 \
      operation_prefix BaseCrashKeyString<256> operation_suffix;             \
      break;                                                                 \
    case base::debug::CrashKeySize::Size1024:                                \
      operation_prefix BaseCrashKeyString<1024> operation_suffix;            \
      break;                                                                 \
  }

class CrashKeyBaseSupport : public base::debug::CrashKeyImplementation {
 public:
  CrashKeyBaseSupport() = default;

  CrashKeyBaseSupport(const CrashKeyBaseSupport&) = delete;
  CrashKeyBaseSupport& operator=(const CrashKeyBaseSupport&) = delete;

  ~CrashKeyBaseSupport() override = default;

  base::debug::CrashKeyString* Allocate(
      const char name[],
      base::debug::CrashKeySize size) override {
    SIZE_CLASS_OPERATION(size, return new, (name, size));
    return nullptr;
  }

  void Set(base::debug::CrashKeyString* crash_key,
           std::string_view value) override {
    SIZE_CLASS_OPERATION(crash_key->size,
                         reinterpret_cast<, *>(crash_key)->impl.Set(value));
  }

  void Clear(base::debug::CrashKeyString* crash_key) override {
    SIZE_CLASS_OPERATION(crash_key->size,
                         reinterpret_cast<, *>(crash_key)->impl.Clear());
  }

  void OutputCrashKeysToStream(std::ostream& out) override {
#if defined(OFFICIAL_BUILD)
    out << "Crash key dumping is inherently thread-unsafe so it's disabled in "
           "official builds.";
#elif BUILDFLAG(USE_CRASHPAD_ANNOTATION)
    // TODO(lukasza): If phasing out breakpad takes a long time, then consider
    // a better way to abstract away difference between crashpad and breakpad.
    // For example, maybe the code below should be moved into
    // third_party/crashpad/crashpad/client and exposed (in an abstract,
    // implementation-agnostic way) via CrashKeyString.  This would allow
    // avoiding using the BUILDFLAG(...) macros here.

    // Note that iterating over this list while other threads are executing is
    // inherently racy.
    auto* annotations = crashpad::AnnotationList::Get();
    if (!annotations || annotations->begin() == annotations->end())
      return;

    out << "Crash keys:\n";
    for (const crashpad::Annotation* annotation : *annotations) {
      if (!annotation->is_set())
        continue;

      if (annotation->type() != crashpad::Annotation::Type::kString)
        continue;
      std::string_view value(static_cast<const char*>(annotation->value()),
                             annotation->size());

      out << "  \"" << annotation->name() << "\" = \"" << value << "\"\n";
    }
#endif
  }
};

#undef SIZE_CLASS_OPERATION

}  // namespace

void InitializeCrashKeyBaseSupport() {
  base::debug::SetCrashKeyImplementation(
      std::make_unique<CrashKeyBaseSupport>());
}

}  // namespace crash_reporter
