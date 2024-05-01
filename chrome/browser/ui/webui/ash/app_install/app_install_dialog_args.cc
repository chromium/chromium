// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/app_install/app_install_dialog_args.h"

namespace ash::app_install {

AppInfoArgs::AppInfoArgs() = default;
AppInfoArgs::AppInfoArgs(AppInfoArgs&&) = default;
AppInfoArgs::~AppInfoArgs() = default;
AppInfoArgs& AppInfoArgs::operator=(AppInfoArgs&&) = default;

ConnectionErrorArgs::ConnectionErrorArgs() = default;
ConnectionErrorArgs::ConnectionErrorArgs(ConnectionErrorArgs&&) = default;
ConnectionErrorArgs::~ConnectionErrorArgs() = default;
ConnectionErrorArgs& ConnectionErrorArgs::operator=(ConnectionErrorArgs&&) =
    default;

}  // namespace ash::app_install
