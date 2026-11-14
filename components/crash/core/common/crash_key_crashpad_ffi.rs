// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! C++ functions used by `crash_key.rs` on platforms that use Crashpad
//! annotations.

#[cxx::bridge(namespace = "crash_reporter::rust")]
mod ffi {
    unsafe extern "C++" {
        include!("components/crash/core/common/crash_key_rust_shim.h");

        #[namespace = "crashpad"]
        type Annotation;

        /// Adds `annotation` to Crashpad's list of annotations, so that it is
        /// included in crash reports. Does nothing if it was already added.
        ///
        /// # Safety
        ///
        /// `annotation` must be a valid `crashpad::Annotation` that is never
        /// destroyed.
        unsafe fn RegisterAnnotation(annotation: *mut Annotation);
    }
}

pub use ffi::{Annotation, RegisterAnnotation};
