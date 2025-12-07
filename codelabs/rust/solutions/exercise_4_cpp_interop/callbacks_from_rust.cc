// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/logging.h>
#include <codelabs/rust/solutions/exercise_4_cpp_interop/callbacks_from_rust.h>

void hello_from_cpp() {
  LOG(INFO) << "Callback from Rust into C++";
}
