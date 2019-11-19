// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/chrome_elf/hook_util/test/hook_util_test_dll.h"

namespace {

int g_export_call_count = 0;

}  // namespace

extern "C" __declspec(dllexport) void ExportedApi() {
  g_export_call_count++;
}

extern "C" __declspec(dllexport) int ExportedApiCallCount() {
  return g_export_call_count;
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved) {
  return TRUE;
}
