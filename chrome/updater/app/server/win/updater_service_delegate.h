// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_APP_SERVER_WIN_UPDATER_SERVICE_DELEGATE_H_
#define CHROME_UPDATER_APP_SERVER_WIN_UPDATER_SERVICE_DELEGATE_H_

#include "base/functional/callback_forward.h"
#include "base/win/windows_types.h"
#include "chrome/windows_services/service_program/service_delegate.h"

namespace updater {

class UpdaterServiceDelegate : public ServiceDelegate {
 public:
  // This function is the main entry point for the service. The return value can
  // be either a Win32 error code or an HRESULT, depending on the API function
  // that failed.
  static int RunWindowsService();

  UpdaterServiceDelegate(const UpdaterServiceDelegate&) = delete;
  UpdaterServiceDelegate& operator=(const UpdaterServiceDelegate&) = delete;

 private:
  UpdaterServiceDelegate();
  ~UpdaterServiceDelegate() override;

  bool PreRun() override;

  // Runs the main logic of the service.
  HRESULT Run(const base::CommandLine& command_line) override;
  void OnServiceControlStop() override;

  // Handles COM object registration, message loop, and unregistration. Returns
  // when all COM objects are released.
  HRESULT RunCOMServer();
};

}  // namespace updater

#endif  // CHROME_UPDATER_APP_SERVER_WIN_UPDATER_SERVICE_DELEGATE_H_
