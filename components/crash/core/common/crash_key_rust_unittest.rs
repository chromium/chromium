// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Tests for `//components/crash/core/common:crash_key_rust`.

chromium::import! {
    "//components/crash/core/common:crash_key_rust_for_testing" as crash_key_rust;
}

use crash_key_rust::CrashKeyString;
use rust_gtest_interop::prelude::*;

#[cxx::bridge(namespace = "crash_reporter::rust::test")]
mod ffi {
    unsafe extern "C++" {
        include!("components/crash/core/common/crash_key_rust_test_shim.h");

        #[namespace = "crashpad"]
        type Annotation;

        fn AnnotationSize() -> usize;
        fn AnnotationName(annotation: &Annotation) -> &str;
        fn AnnotationValueSize(annotation: &Annotation) -> u32;
        fn AnnotationType(annotation: &Annotation) -> u16;
        fn AnnotationIsGuarded(annotation: &Annotation) -> bool;
    }
}

/// Returns `crash_key` as a `crashpad::Annotation`, to inspect it via
/// Crashpad's C++ API.
fn as_annotation<const N: usize>(
    crash_key: &'static CrashKeyString<N>,
) -> &'static ffi::Annotation {
    // SAFETY: `CrashKeyString` is layout-compatible with `crashpad::Annotation`
    // (which is what these tests verify), and `crash_key` is `'static`.
    unsafe { &*crash_key.as_annotation_ptr_for_testing().cast::<ffi::Annotation>() }
}

/// The size of each of the pointer fields at the start of
/// `crashpad::Annotation`. `usize` is defined to be pointer-sized.
const ANNOTATION_POINTER_FIELD_SIZE: usize = size_of::<usize>();

/// Size of `crashpad::Annotation`, without the inline value: three pointer
/// fields, followed by 8 bytes of other fields.
const ANNOTATION_HEADER_SIZE: usize = 3 * ANNOTATION_POINTER_FIELD_SIZE + 8;

#[gtest(RustCrashKeyTest, LayoutMatchesCrashpad)]
fn test_layout_matches_crashpad() {
    static KEY: CrashKeyString<32> = CrashKeyString::new(c"rust-layout");
    expect_eq!(ffi::AnnotationSize(), ANNOTATION_HEADER_SIZE);

    let annotation = as_annotation(&KEY);
    expect_eq!(ffi::AnnotationName(annotation), "rust-layout");
    expect_eq!(ffi::AnnotationValueSize(annotation), 0);
    expect_eq!(ffi::AnnotationType(annotation), 1);
    expect_false!(ffi::AnnotationIsGuarded(annotation));
}

#[gtest(RustCrashKeyTest, Name)]
fn test_name() {
    static KEY: CrashKeyString<64> = CrashKeyString::new(c"rust-name");
    expect_eq!(KEY.name(), c"rust-name");
}
