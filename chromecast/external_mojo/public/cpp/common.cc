// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/external_mojo/public/cpp/common.h"

#include <string_view>

#include "base/check.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "chromecast/base/chromecast_switches.h"

namespace chromecast {
namespace external_mojo {

namespace {

#if !BUILDFLAG(IS_ANDROID)
// Default path for Unix domain socket used by external Mojo services to connect
// to Mojo services within cast_shell.
constexpr std::string_view kDefaultBrokerPath("/tmp/cast_mojo_broker");
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace

std::string GetBrokerPath() {
  std::string broker_path;
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kCastMojoBrokerPath)) {
    broker_path =
        command_line->GetSwitchValueASCII(switches::kCastMojoBrokerPath);
  } else {
#if BUILDFLAG(IS_ANDROID)
    // Android apps don't have access to `/tmp` folder. Use app's data folder
    // instead to store the socket file.
    base::FilePath socket_path;
    CHECK(base::PathService::Get(base::DIR_ANDROID_APP_DATA, &socket_path));
    socket_path = socket_path.AppendASCII(FILE_PATH_LITERAL("cast_mojo_broker"));
    broker_path = socket_path.MaybeAsASCII();
#else
    broker_path = std::string(kDefaultBrokerPath);
#endif  // BUILDFLAG(IS_ANDROID)
  }
  return broker_path;
}

}  // namespace external_mojo
}  // namespace chromecast
