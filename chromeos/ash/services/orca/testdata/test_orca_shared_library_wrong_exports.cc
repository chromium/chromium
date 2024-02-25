// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

extern "C" {

// Does not export functions expected by `OrcaLibrary`.

void __attribute__((visibility("default"))) TestFunction(int) {}
}
