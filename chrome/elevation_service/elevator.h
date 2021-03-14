// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_ELEVATION_SERVICE_ELEVATOR_H_
#define CHROME_ELEVATION_SERVICE_ELEVATOR_H_

#include <windows.h>

#include <wrl/implements.h>
#include <wrl/module.h>

#include <string>

#include "base/macros.h"
#include "chrome/elevation_service/elevation_service_idl.h"

namespace elevation_service {

class Elevator
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IElevator,
          IElevatorChromium,
          IElevatorChrome,
          IElevatorChromeBeta,
          IElevatorChromeDev,
          IElevatorChromeCanary> {
 public:
  Elevator() = default;

  // Securely validates and runs the provided Chrome Recovery CRX elevated, by
  // first copying the CRX to a secure directory under %ProgramFiles% to
  // validate and unpack the CRX.
  IFACEMETHODIMP RunRecoveryCRXElevated(const wchar_t* crx_path,
                                        const wchar_t* browser_appid,
                                        const wchar_t* browser_version,
                                        const wchar_t* session_id,
                                        DWORD caller_proc_id,
                                        ULONG_PTR* proc_handle) override;

 private:
  ~Elevator() override = default;

  DISALLOW_COPY_AND_ASSIGN(Elevator);
};

}  // namespace elevation_service

#endif  // CHROME_ELEVATION_SERVICE_ELEVATOR_H_
