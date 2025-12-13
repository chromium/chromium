# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
def is_rust_build_script(script: str) -> bool:
  return script == "//build/rust/gni_impl/run_build_script.py"