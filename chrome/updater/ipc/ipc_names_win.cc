// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/ipc/ipc_names.h"

#include <optional>
#include <string>

#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/updater_version.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"

namespace updater {

namespace {

std::wstring GetMojoServerName(UpdaterScope scope,
                               bool is_internal_service,
                               const std::wstring& version = {}) {
  std::wstring server_name =
      base::StrCat({base::UTF8ToWide(PRODUCT_FULLNAME_STRING),
                    IsSystemInstall(scope) ? L"System" : L"User",
                    is_internal_service ? L"Internal" : L"", version});
  std::erase_if(server_name, base::IsAsciiWhitespace<wchar_t>);
  return server_name;
}

}  // namespace

mojo::NamedPlatformChannel::ServerName GetUpdateServiceInternalServerName(
    UpdaterScope scope) {
  return GetMojoServerName(scope, true, kUpdaterVersionUtf16);
}

mojo::NamedPlatformChannel::ServerName GetUpdateServiceServerName(
    UpdaterScope scope) {
  return GetMojoServerName(scope, false);
}

}  // namespace updater
