// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NACL_LOADER_NACL_HELPER_LINUX_H_
#define COMPONENTS_NACL_LOADER_NACL_HELPER_LINUX_H_

namespace nacl {

// A mini-zygote specifically for Native Client. This file defines
// constants used to implement communication between the nacl_helper
// process and the Chrome zygote.

#define kNaClMaxIPCMessageLength 2048

// Used by Helper to tell Zygote it has started successfully.
#define kNaClHelperStartupAck "NACLHELPER_OK"

enum NaClZygoteIPCCommand {
  kNaClForkRequest,
  kNaClGetTerminationStatusRequest,
};

// The next set of constants define global Linux file descriptors.
// For communications between NaCl loader and browser.
// See also content/common/zygote_main_linux.cc and
// https://chromium.googlesource.com/chromium/src/+/main/docs/linux/zygote.md

// For communications between NaCl loader and zygote.
#define kNaClZygoteDescriptor 3

} // namespace nacl

#endif  // COMPONENTS_NACL_LOADER_NACL_HELPER_LINUX_H_
