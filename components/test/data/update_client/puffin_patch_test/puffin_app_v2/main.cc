// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The Puffin patch test app is a do-nothing application "installer" that is
// embedded in an update CRX. This is version 2, version 1 is located at
// ../puffin_app_v1. If you apply the
// puffpatch puffin_app_v1_to_v2.puff to puffin_app_v1.crx3, it produces
// puffin_app_v2.crx3. See //third_party/puffin.
#include <iostream>

int main() {
  std::cout << "Hello, world! this is version 2 of puffin_app." << std::endl;
}
