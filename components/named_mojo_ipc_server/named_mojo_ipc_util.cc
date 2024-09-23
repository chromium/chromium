// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/named_mojo_ipc_server/named_mojo_ipc_util.h"

#include <string>
#include <string_view>

#include "build/build_config.h"

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC)
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#endif

namespace named_mojo_ipc_server {

mojo::NamedPlatformChannel::ServerName
WorkingDirectoryIndependentServerNameFromUTF8(std::string_view name) {
#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC)
  // The channel name on non-mac POSIX (basically Linux) is the path to a unix
  // domain socket, so it needs to be an absolute path to allow the IPC binary
  // to be executed from any working directory.
  base::FilePath temp_dir_path;
  if (base::GetTempDir(&temp_dir_path)) {
    return mojo::NamedPlatformChannel::ServerNameFromUTF8(
        temp_dir_path.Append(name).value());
  }
  LOG(ERROR) << "Failed to retrieve temporary directory.";
  // Fallback to just using |name|.
#endif

  // ServerName on other platforms (i.e. Windows and Mac) is globally unique.
  return mojo::NamedPlatformChannel::ServerNameFromUTF8(name);
}

}  // namespace named_mojo_ipc_server
