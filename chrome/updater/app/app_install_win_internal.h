// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_APP_APP_INSTALL_WIN_INTERNAL_H_
#define CHROME_UPDATER_APP_APP_INSTALL_WIN_INTERNAL_H_

#include "chrome/updater/app/app_install_progress.h"
#include "chrome/updater/update_service.h"

namespace updater {

[[nodiscard]] ObserverCompletionInfo HandleInstallResult(
    const UpdateService::UpdateState& update_state);

}  // namespace updater

#endif  // CHROME_UPDATER_APP_APP_INSTALL_WIN_INTERNAL_H_
