// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/common/nacl_switches.h"

namespace switches {

// Disables crash throttling for Portable Native Client.
const char kDisablePnaclCrashThrottling[]   = "disable-pnacl-crash-throttling";

// Enables debugging via RSP over a socket.
const char kEnableNaClDebug[]               = "enable-nacl-debug";

// Force use of the Subzero as the PNaCl translator instead of LLC.
const char kForcePNaClSubzero[] = "force-pnacl-subzero";

// Uses NaCl manifest URL to choose whether NaCl program will be debugged by
// debug stub.
// Switch value format: [!]pattern1,pattern2,...,patternN. Each pattern uses
// the same syntax as patterns in Chrome extension manifest. The only difference
// is that * scheme matches all schemes instead of matching only http and https.
// If the value doesn't start with !, a program will be debugged if manifest URL
// matches any pattern. If the value starts with !, a program will be debugged
// if manifest URL does not match any pattern.
const char kNaClDebugMask[]                 = "nacl-debug-mask";

// GDB script to pass to the nacl-gdb debugger at startup.
const char kNaClGdbScript[]                 = "nacl-gdb-script";

// Native Client GDB debugger that will be launched automatically when needed.
const char kNaClGdb[]                       = "nacl-gdb";

// Value for --type that causes the process to run as a NativeClient loader
// for SFI mode.
const char kNaClLoaderProcess[]             = "nacl-loader";

// Sets NACLVERBOSITY to enable verbose logging.
// This should match the string used in chrome/browser/about_flags.cc
const char kVerboseLoggingInNacl[] = "verbose-logging-in-nacl";

const char kVerboseLoggingInNaclChoiceLow[] = "1";
const char kVerboseLoggingInNaclChoiceMedium[] = "2";
const char kVerboseLoggingInNaclChoiceHigh[] = "4";
const char kVerboseLoggingInNaclChoiceHighest[] = "7";
const char kVerboseLoggingInNaclChoiceDisabled[] = "0";
}  // namespace switches
