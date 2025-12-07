// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_IPC_UPDATE_SERVICE_DIALER_H_
#define CHROME_UPDATER_IPC_UPDATE_SERVICE_DIALER_H_

#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/updater/ipc/update_service_dialer_win.h"
#else  // BUILDFLAG(IS_WIN)
#include "chrome/updater/ipc/update_service_dialer_posix.h"
#endif  // BUILDFLAG(IS_WIN)

#endif  // CHROME_UPDATER_IPC_UPDATE_SERVICE_DIALER_H_
