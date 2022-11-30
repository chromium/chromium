// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/common/nacl_paths.h"

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "components/nacl/common/nacl_switches.h"

namespace {

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
// File name of the nacl_helper and nacl_helper_bootstrap, Linux only.
const base::FilePath::CharType kInternalNaClHelperFileName[] =
    FILE_PATH_LITERAL("nacl_helper");
const base::FilePath::CharType kInternalNaClHelperBootstrapFileName[] =
    FILE_PATH_LITERAL("nacl_helper_bootstrap");

bool GetNaClHelperPath(const base::FilePath::CharType* filename,
                       base::FilePath* output) {
  if (!base::PathService::Get(base::DIR_MODULE, output))
    return false;
  *output = output->Append(filename);
  return true;
}

#endif

}  // namespace

namespace nacl {

bool PathProvider(int key, base::FilePath* result) {
  switch (key) {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
    case FILE_NACL_HELPER:
      return GetNaClHelperPath(kInternalNaClHelperFileName, result);
    case FILE_NACL_HELPER_BOOTSTRAP:
      return GetNaClHelperPath(kInternalNaClHelperBootstrapFileName, result);
#endif
    default:
      return false;
  }
}

// This cannot be done as a static initializer sadly since Visual Studio will
// eliminate this object file if there is no direct entry point into it.
void RegisterPathProvider() {
  base::PathService::RegisterProvider(PathProvider, PATH_START, PATH_END);
}

}  // namespace nacl
