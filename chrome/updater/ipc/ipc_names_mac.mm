// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/ipc/ipc_names.h"

#include "base/strings/sys_string_conversions.h"
#import "chrome/updater/mac/xpc_service_names.h"
#include "chrome/updater/updater_scope.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"

namespace updater {

mojo::NamedPlatformChannel::ServerName GetUpdateServiceInternalServerName(
    UpdaterScope scope) {
  return base::SysNSStringToUTF8(GetUpdateServiceInternalMachName(scope));
}

}  // namespace updater
