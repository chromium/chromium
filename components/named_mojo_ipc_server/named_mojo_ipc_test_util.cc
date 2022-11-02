// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/named_mojo_ipc_server/named_mojo_ipc_test_util.h"

#include <inttypes.h>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"

namespace named_mojo_ipc_server {
namespace test {

mojo::NamedPlatformChannel::ServerName GenerateRandomServerName() {
  std::string temp_path;
#if BUILDFLAG(IS_POSIX)
  // Posix server names should start with the temp directory path. Otherwise the
  // socket file will be created under the current working directory.
  base::FilePath temp_file_path;
  if (base::GetTempDir(&temp_file_path)) {
    temp_path = temp_file_path.AsEndingWithSeparator().value();
  } else {
    LOG(ERROR) << "Failed to retrieve temporary directory.";
  }
#endif
  return mojo::NamedPlatformChannel::ServerNameFromUTF8(
      base::StringPrintf("%sremoting_mojo_test_server.%" PRIu64,
                         temp_path.c_str(), base::RandUint64()));
}

}  // namespace test
}  // namespace named_mojo_ipc_server
