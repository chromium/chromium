// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/core/common/crash_key_rust_test_shim.h"

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

}  // namespace crash_reporter::rust::test
