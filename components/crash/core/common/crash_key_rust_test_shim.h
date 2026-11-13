// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRASH_CORE_COMMON_CRASH_KEY_RUST_TEST_SHIM_H_
#define COMPONENTS_CRASH_CORE_COMMON_CRASH_KEY_RUST_TEST_SHIM_H_

#include <stddef.h>
#include <stdint.h>

#include "third_party/crashpad/crashpad/client/annotation.h"
#include "third_party/rust/cxx/v1/cxx.h"

// Test-only helpers that inspect Rust crash keys through Crashpad's C++ API,
// to verify that Rust's view of `crashpad::Annotation` matches C++'s.
namespace crash_reporter::rust::test {

size_t AnnotationSize();

::rust::Str AnnotationName(const crashpad::Annotation& annotation);
uint32_t AnnotationValueSize(const crashpad::Annotation& annotation);
uint16_t AnnotationType(const crashpad::Annotation& annotation);
bool AnnotationIsGuarded(const crashpad::Annotation& annotation);

}  // namespace crash_reporter::rust::test

#endif  // COMPONENTS_CRASH_CORE_COMMON_CRASH_KEY_RUST_TEST_SHIM_H_
