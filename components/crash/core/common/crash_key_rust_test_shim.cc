// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/core/common/crash_key_rust_test_shim.h"

#include <string_view>

#include "third_party/crashpad/crashpad/client/annotation_list.h"

namespace crash_reporter::rust::test {

size_t AnnotationSize() {
  return sizeof(crashpad::Annotation);
}

::rust::Str AnnotationName(const crashpad::Annotation& annotation) {
  return ::rust::Str(annotation.name());
}

uint32_t AnnotationValueSize(const crashpad::Annotation& annotation) {
  return annotation.size();
}

uint16_t AnnotationType(const crashpad::Annotation& annotation) {
  return static_cast<uint16_t>(annotation.type());
}

bool AnnotationIsGuarded(const crashpad::Annotation& annotation) {
  return annotation.concurrent_access_guard_mode() ==
         crashpad::Annotation::ConcurrentAccessGuardMode::kScopedSpinGuard;
}

::rust::String AnnotationValue(const crashpad::Annotation& annotation) {
  return ::rust::String(static_cast<const char*>(annotation.value()),
                        annotation.size());
}

bool IsInAnnotationList(const crashpad::Annotation& annotation) {
  crashpad::AnnotationList* list = crashpad::AnnotationList::Get();
  if (!list) {
    return false;
  }
  for (const crashpad::Annotation* entry : *list) {
    if (entry == &annotation) {
      return true;
    }
  }
  return false;
}

::rust::String GetAnnotationValueByName(::rust::Str name) {
  crashpad::AnnotationList* list = crashpad::AnnotationList::Get();
  if (!list) {
    return ::rust::String();
  }
  for (const crashpad::Annotation* annotation : *list) {
    if (annotation->is_set() &&
        std::string_view(annotation->name()) == std::string_view(name)) {
      return AnnotationValue(*annotation);
    }
  }
  return ::rust::String();
}

}  // namespace crash_reporter::rust::test
