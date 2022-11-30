// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NACL_COMMON_NACL_PATHS_H_
#define COMPONENTS_NACL_COMMON_NACL_PATHS_H_

#include "build/build_config.h"

// This file declares path keys for the chrome module.  These can be used with
// the PathService to access various special directories and files.

namespace nacl {

enum {
  PATH_START = 9000,

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  FILE_NACL_HELPER = PATH_START,  // Full path to Linux nacl_helper executable.
  FILE_NACL_HELPER_BOOTSTRAP,     // ... and nacl_helper_bootstrap executable.
#endif

  PATH_END
};

// Call once to register the provider for the path keys defined above.
void RegisterPathProvider();

}  // namespace nacl

#endif  // COMPONENTS_NACL_COMMON_NACL_PATHS_H_
