// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <type_traits>

#include "third_party/crashpad/crashpad/client/annotation.h"

namespace crash_reporter::rust {

// The Rust crash key API (crash_key.rs) defines its own storage that mirrors
// the layout of `crashpad::Annotation`. These assertions, together with the
// matching `const` assertions in crash_key.rs, ensure that a change to
// `crashpad::Annotation` breaks the build rather than corrupting crash reports.
// Field offsets can't be checked here because the fields are private; they are
// checked by crash_key_rust_unittest.rs instead.
//
// LINT.IfChange(AnnotationLayout)
static_assert(std::is_standard_layout_v<crashpad::Annotation>);
static_assert(alignof(crashpad::Annotation) == alignof(void*));
static_assert(sizeof(crashpad::Annotation) == 3 * sizeof(void*) + 8);
static_assert(sizeof(crashpad::Annotation::ValueSizeType) == 4);
static_assert(sizeof(crashpad::Annotation::Type) == 2);
static_assert(sizeof(crashpad::Annotation::ConcurrentAccessGuardMode) == 1);
static_assert(static_cast<uint16_t>(crashpad::Annotation::Type::kString) == 1);
// LINT.ThenChange(crash_key.rs:AnnotationLayout)

}  // namespace crash_reporter::rust
