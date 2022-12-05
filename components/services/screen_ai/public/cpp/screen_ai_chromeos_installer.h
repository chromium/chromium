// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_SCREEN_AI_PUBLIC_CPP_SCREEN_AI_CHROMEOS_INSTALLER_H_
#define COMPONENTS_SERVICES_SCREEN_AI_PUBLIC_CPP_SCREEN_AI_CHROMEOS_INSTALLER_H_

#include "chromeos/ash/components/dbus/dlcservice/dlcservice_client.h"

class PrefService;

namespace screen_ai::chrome_os_installer {

// If ScreenAI library is needed, registers for installation of its DLC. If not,
// and after some delay, uninstalls the DLC.
void ManageInstallation(PrefService* local_state);

}  // namespace screen_ai::chrome_os_installer

#endif  // COMPONENTS_SERVICES_SCREEN_AI_PUBLIC_CPP_SCREEN_AI_CHROMEOS_INSTALLER_H_
