// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use rust_gtest_interop::prelude::*;

chromium::import! {
    "//codelabs/rust/solutions/exercise_2_testing:library";
}

#[gtest(MyRustTestSuite, MyAdditionTest)]
fn test() {
    expect_eq!(library::add_numbers(2, 2), 4);
}
