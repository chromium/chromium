// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a minimal implementation of ChromeMain used to link the test helper
// executable for verifying early library execution order.

__attribute__((visibility("default"))) int ChromeMain(int argc, char* argv[]) {
  return 0;
}
