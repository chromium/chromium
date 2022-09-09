// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A dummy source file that exports a single symbol. Built using compiler and
// linker flags very similar to those used for official builds as of March 2014.
//
// x86:
// cl /nologo -D_WIN32_WINNT=0x0602 -DWINVER=0x0602 -DWIN32 -D_WINDOWS
// -DNOMINMAX -DWIN32_LEAN_AND_MEAN -D_HAS_EXCEPTIONS=0 -D__STD_C
// -D_CRT_SECURE_NO_DEPRECATE -D_SCL_SECURE_NO_DEPRECATE
// -DNTDDI_VERSION=0x06020000 -D_USING_V110_SDK71_ -D__STDC_CONSTANT_MACROS
// -D__STDC_FORMAT_MACROS -DNDEBUG -D_UNICODE -DUNICODE /O1 /Ob2 /GF /GT /Oy-
// /Oi /Os /W4 /WX /Zi /GR- /Gy /GS /MT /we4389 /Oy- /FS /TP /FC
// module_with_exports.cc /link /nologo /DLL /OUT:module_with_exports_x86.dll
// /DEBUG /MACHINE:X86 /safeseh /largeaddressaware /SUBSYSTEM:CONSOLE,5.01
// /INCREMENTAL:NO /FIXED:NO /OPT:REF /OPT:ICF /LTCG /PROFILE /DYNAMICBASE
// /NXCOMPAT /MANIFEST /MANIFESTUAC:NO
//
// x64:
// cl /nologo -D_WIN32_WINNT=0x0602 -DWINVER=0x0602 -DWIN32 -D_WINDOWS
// -DNOMINMAX -DWIN32_LEAN_AND_MEAN -D_HAS_EXCEPTIONS=0 -D__STD_C
// -D_CRT_SECURE_NO_DEPRECATE -D_SCL_SECURE_NO_DEPRECATE
// -DNTDDI_VERSION=0x06020000 -D_USING_V110_SDK71_ -D__STDC_CONSTANT_MACROS
// -D__STDC_FORMAT_MACROS -DNDEBUG -D_UNICODE -DUNICODE /wd4351 /wd4355 /wd4396
// /wd4503 /wd4819 /wd4100 /wd4121 /wd4125 /wd4127 /wd4130 /wd4131 /wd4189
// /wd4201 /wd4238 /wd4244 /wd4245 /wd4310 /wd4428 /wd4481 /wd4505 /wd4510
// /wd4512 /wd4530 /wd4610 /wd4611 /wd4701 /wd4702 /wd4706 /O1 /Ob2 /GF /GT /Oy-
// /Oi /Os /W4 /WX /Zi /GR- /Gy /GS /MT /we4389 /Oy- /FS /TP /FC
// module_with_exports.cc /link /nologo /DLL /OUT:module_with_exports_x64.dll
// /DEBUG /MACHINE:X64 /SUBSYSTEM:CONSOLE /INCREMENTAL:NO /FIXED:NO /OPT:REF
// /OPT:ICF /LTCG /PROFILE /DYNAMICBASE /NXCOMPAT /MANIFEST /MANIFESTUAC:NO

#include <windows.h>

extern "C" __declspec(dllexport) void AnExport(void) {
}

extern "C" BOOL WINAPI DllMain(HINSTANCE, DWORD, void*) {
  return TRUE;
}
