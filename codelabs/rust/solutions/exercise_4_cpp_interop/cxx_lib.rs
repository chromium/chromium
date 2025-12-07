// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#[cxx::bridge]
mod ffi {
    extern "Rust" {
        fn hello_from_rust();
    }

    unsafe extern "C++" {
        include!("codelabs/rust/solutions/exercise_4_cpp_interop/callbacks_from_rust.h");
        fn hello_from_cpp();
    }
}

pub fn hello_from_rust() {
    println!("Hello from Rust!");
    ffi::hello_from_cpp();
}
