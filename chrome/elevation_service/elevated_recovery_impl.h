// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_ELEVATION_SERVICE_ELEVATED_RECOVERY_IMPL_H_
#define CHROME_ELEVATION_SERVICE_ELEVATED_RECOVERY_IMPL_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/win/scoped_handle.h"

namespace base {

class CommandLine;
class FilePath;

}  // namespace base

namespace crx_file {
enum class VerifierFormat;
}

namespace elevation_service {

// Delete stale files within the Chrome Recovery directory left over from any
// previous calls to RunChromeRecoveryCRX.
HRESULT CleanupChromeRecoveryDirectory();

// Verifies the CRX and then runs ChromeRecovery.exe embedded within the
// provided |crx_path|. The returned |proc_handle| is a process handle that is
// valid for the |caller_proc_id| process, or the current process if
// |caller_proc_id| is 0. Please read the doc comment in
// elevation_service_idl.idl for the other parameters.
HRESULT RunChromeRecoveryCRX(const base::FilePath& crx_path,
                             const std::wstring& browser_appid,
                             const std::wstring& browser_version,
                             const std::wstring& session_id,
                             uint32_t caller_proc_id,
                             base::win::ScopedHandle* proc_handle);

// Verifies the CRX and then runs |exe_filename| embedded within the provided
// |crx_path|. The returned |proc_handle| is a process handle that is valid for
// the |caller_proc_id| process, or the current process if |caller_proc_id| is
// 0. |unpacked_under_path| is expected to be eventually deleted by the caller.
HRESULT RunCRX(const base::FilePath& crx_path,
               const base::CommandLine& args,
               const crx_file::VerifierFormat& crx_format,
               const std::vector<uint8_t>& crx_hash,
               const base::FilePath& unpack_under_path,
               const base::FilePath& exe_filename,
               uint32_t caller_proc_id,
               base::win::ScopedHandle* proc_handle);

}  // namespace elevation_service

#endif  // CHROME_ELEVATION_SERVICE_ELEVATED_RECOVERY_IMPL_H_
