// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/ipc/ipc_names.h"

#include "base/strings/strcat.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/updater_version.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"

namespace updater {

mojo::NamedPlatformChannel::ServerName GetUpdateServiceInternalServerName(
    UpdaterScope scope) {
  return base::StrCat({MAC_BUNDLE_IDENTIFIER_STRING ".update-internal.",
                       kUpdaterVersion,
                       scope == UpdaterScope::kUser ? "" : ".system"});
}

mojo::NamedPlatformChannel::ServerName GetUpdateServiceServerName(
    UpdaterScope scope) {
  return base::StrCat({MAC_BUNDLE_IDENTIFIER_STRING ".update",
                       scope == UpdaterScope::kUser ? "" : ".system"});
}

}  // namespace updater
