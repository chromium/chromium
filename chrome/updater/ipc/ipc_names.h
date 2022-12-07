// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_IPC_IPC_NAMES_H_
#define CHROME_UPDATER_IPC_IPC_NAMES_H_

#include "chrome/updater/updater_scope.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"

namespace updater {

mojo::NamedPlatformChannel::ServerName GetUpdateServiceInternalServerName(
    UpdaterScope scope);

#if BUILDFLAG(IS_LINUX)
mojo::NamedPlatformChannel::ServerName GetUpdateServiceServerName(
    UpdaterScope scope);
#endif  // BUILDFLAG(IS_LINUX)

}  // namespace updater

#endif  // CHROME_UPDATER_IPC_NAMES_H_
