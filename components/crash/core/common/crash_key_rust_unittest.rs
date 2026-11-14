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
        fn AnnotationValue(annotation: &Annotation) -> String;
        fn IsInAnnotationList(annotation: &Annotation) -> bool;
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

#[gtest(RustCrashKeyTest, SetAndClear)]
fn test_set_and_clear() {
    static KEY: CrashKeyString<32> = CrashKeyString::new(c"rust-set-and-clear");
    let annotation = as_annotation(&KEY);

    KEY.set("value");
    expect_true!(ffi::IsInAnnotationList(annotation));
    expect_eq!(ffi::AnnotationValueSize(annotation), 5);
    expect_eq!(ffi::AnnotationValue(annotation), "value");

    KEY.set("another value");
    expect_eq!(ffi::AnnotationValue(annotation), "another value");

    KEY.clear();
    expect_eq!(ffi::AnnotationValueSize(annotation), 0);
    // Cleared crash keys stay in the list, but aren't included in reports.
    expect_true!(ffi::IsInAnnotationList(annotation));

    KEY.set("again");
    expect_eq!(ffi::AnnotationValue(annotation), "again");
}

#[gtest(RustCrashKeyTest, Truncation)]
fn test_truncation() {
    static KEY: CrashKeyString<32> = CrashKeyString::new(c"rust-truncation");
    let annotation = as_annotation(&KEY);

    KEY.set("x".repeat(100));
    expect_eq!(ffi::AnnotationValue(annotation), "x".repeat(32));

    // Truncation doesn't split multi-byte characters: 16 two-byte characters
    // fit exactly, but a leading one-byte character leaves room for only 15.
    KEY.set("\u{e9}".repeat(20));
    expect_eq!(ffi::AnnotationValue(annotation), "\u{e9}".repeat(16));
    KEY.set(format!("x{}", "\u{e9}".repeat(20)));
    expect_eq!(ffi::AnnotationValue(annotation), format!("x{}", "\u{e9}".repeat(15)));
}

#[gtest(RustCrashKeyTest, ConcurrentWrites)]
fn test_concurrent_writes() {
    static KEY: CrashKeyString<32> = CrashKeyString::new(c"rust-concurrent-writes");
    let values = ["a".repeat(32), "b".repeat(32)];
    std::thread::scope(|scope| {
        for value in &values {
            scope.spawn(move || {
                for _ in 0..1000 {
                    KEY.set(value);
                }
            });
        }
    });
    // Each write is atomic with respect to other writes, so the value must be
    // one of the written values rather than a mix of them.
    let value = ffi::AnnotationValue(as_annotation(&KEY));
    expect_true!(values.contains(&value), "Unexpected value: {value}");
}

#[gtest(RustCrashKeyTest, SetDisplay)]
fn test_set_display() {
    static KEY: CrashKeyString<32> = CrashKeyString::new(c"rust-set-display");
    let annotation = as_annotation(&KEY);

    KEY.set(42);
    expect_eq!(ffi::AnnotationValue(annotation), "42");
    KEY.set(true);
    expect_eq!(ffi::AnnotationValue(annotation), "true");
    KEY.set(String::from("owned"));
    expect_eq!(ffi::AnnotationValue(annotation), "owned");
    let (id, name) = (7, "seven");
    KEY.set(format_args!("{id}:{name}"));
    expect_eq!(ffi::AnnotationValue(annotation), "7:seven");
}

#[gtest(RustCrashKeyTest, SetDisplayTruncation)]
fn test_set_display_truncation() {
    static KEY: CrashKeyString<32> = CrashKeyString::new(c"rust-set-display-truncation");
    let annotation = as_annotation(&KEY);

    let first = "x".repeat(20);
    let second = "\u{e9}".repeat(10);
    KEY.set(format_args!("{first}{second}"));
    expect_eq!(ffi::AnnotationValue(annotation), format!("{first}{}", "\u{e9}".repeat(6)));
}
