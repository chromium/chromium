// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use std::env;
use std::io::Write;
use std::path::Path;
use std::process::Command;
use std::str::{self, FromStr};

fn main() {
    println!("cargo:rustc-cfg=build_script_ran");

    let target = env::var("TARGET").unwrap();

    if target.contains("android") {
        println!("cargo:rustc-cfg=is_android");
    }
    let target_arch = env::var("CARGO_CFG_TARGET_ARCH").unwrap();
    println!("cargo:rustc-cfg={}", target_arch.as_str());
}
