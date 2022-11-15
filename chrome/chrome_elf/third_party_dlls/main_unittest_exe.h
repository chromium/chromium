// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_ELF_THIRD_PARTY_DLLS_MAIN_UNITTEST_EXE_H_
#define CHROME_CHROME_ELF_THIRD_PARTY_DLLS_MAIN_UNITTEST_EXE_H_

namespace third_party_dlls {

enum ExitCode {
  kDllLoadSuccess = 0,
  kDllLoadFailed = 1,

  // Unexpected failures are negative ints:
  kBadCommandLine = -1,
  kThirdPartyAlreadyInitialized = -2,
  kThirdPartyInitFailure = -3,
  kMissingArgument = -4,
  kBadBlocklistPath = -5,
  kBadArgument = -6,
  kUnsupportedTestId = -7,
  kEmptyLog = -8,
  kUnexpectedLog = -9,
  kUnexpectedSectionPath = -10,
  kBadLogEntrySize = -11,
};

enum TestId {
  kTestOnlyInitialization = 1,  // Just initialization.
  kTestSingleDllLoad = 2,       // Single DLL load.
  kTestLogPath = 3,             // Single DLL load with log path scrutiny.
};

}  // namespace third_party_dlls

#endif  // CHROME_CHROME_ELF_THIRD_PARTY_DLLS_MAIN_UNITTEST_EXE_H_
