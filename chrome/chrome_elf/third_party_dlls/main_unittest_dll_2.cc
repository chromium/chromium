// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

extern "C" {
// Have a dummy export so that the module gets an export table entry.
void DummyExport() {}
}  // extern "C"

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved) {
  return TRUE;
}
