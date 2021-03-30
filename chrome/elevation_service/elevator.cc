// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/elevation_service/elevator.h"

#include <stdint.h>

#include "base/files/file_path.h"
#include "base/win/win_util.h"
#include "chrome/elevation_service/elevated_recovery_impl.h"

namespace elevation_service {

HRESULT Elevator::RunRecoveryCRXElevated(const wchar_t* crx_path,
                                         const wchar_t* browser_appid,
                                         const wchar_t* browser_version,
                                         const wchar_t* session_id,
                                         DWORD caller_proc_id,
                                         ULONG_PTR* proc_handle) {
  base::win::ScopedHandle scoped_proc_handle;
  HRESULT hr = RunChromeRecoveryCRX(base::FilePath(crx_path), browser_appid,
                                    browser_version, session_id, caller_proc_id,
                                    &scoped_proc_handle);
  *proc_handle = base::win::HandleToUint32(scoped_proc_handle.Take());
  return hr;
}

}  // namespace elevation_service
