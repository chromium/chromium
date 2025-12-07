// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/enterprise_companion/enterprise_companion.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

#if ENTERPRISE_COMPANION_TEST_ONLY
// This symbol is normally declared in //base:test_support and defined in
// //base. Adding a dependency on //base:test_support forces the executable to
// take a dependency on libtest_trace_processor.dylib with an rpath relative to
// the executable's path. However, during integration tests this executable will
// be installed elsewhere on the filesystem (i.e. /Library/Application Support/)
// causing it to crash due to dylib resolution failure. Because the dependency
// on //base/test_support is only needed for the declaration of
// AllowCheckIsTestForTesting, it is simpler to declare it ourselves than add
// test-only code paths to the installer to ensure that the dylib is copied.
namespace base::test {
void AllowCheckIsTestForTesting();  // IN-TEST
}
#endif

void MaybeAllowCheckIsTest() {
#if ENTERPRISE_COMPANION_TEST_ONLY
  // Allow test-only code paths to be reached for enterprise_companion_test.
  // This is useful for integration testing. For instance, this allows the
  // pinned policy signing key for CloudPolicyClient to be overridden via the
  // kPolicyVerificationKey flag.
  base::test::AllowCheckIsTestForTesting();  // IN-TEST
#endif
}

#if BUILDFLAG(IS_POSIX)
int main(int argc, const char* argv[]) {
  MaybeAllowCheckIsTest();
  return enterprise_companion::EnterpriseCompanionMain(argc, argv);
}
#elif BUILDFLAG(IS_WIN)
int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE prev, wchar_t*, int) {
  MaybeAllowCheckIsTest();
  // `argc` and `argv` are ignored by `base::CommandLine` for Windows. Instead,
  // the implementation parses `GetCommandLineW()` directly.
  return enterprise_companion::EnterpriseCompanionMain(0, nullptr);
}
#endif
