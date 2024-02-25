// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines all the command-line switches used by Chrome.

#ifndef COMPONENTS_NACL_COMMON_NACL_SWITCHES_H_
#define COMPONENTS_NACL_COMMON_NACL_SWITCHES_H_

namespace switches {

// All switches in alphabetical order. The switches should be documented
// alongside the definition of their values in the .cc file.
extern const char kDisablePnaclCrashThrottling[];
extern const char kEnableNaClDebug[];
extern const char kForcePNaClSubzero[];
extern const char kNaClDebugMask[];
extern const char kNaClGdbScript[];
extern const char kNaClGdb[];
extern const char kNaClLoaderProcess[];

extern const char kVerboseLoggingInNacl[];
extern const char kVerboseLoggingInNaclChoiceDisabled[];
extern const char kVerboseLoggingInNaclChoiceLow[];
extern const char kVerboseLoggingInNaclChoiceMedium[];
extern const char kVerboseLoggingInNaclChoiceHigh[];
extern const char kVerboseLoggingInNaclChoiceHighest[];

}  // namespace switches

#endif  // COMPONENTS_NACL_COMMON_NACL_SWITCHES_H_
