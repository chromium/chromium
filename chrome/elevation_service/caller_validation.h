// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_ELEVATION_SERVICE_CALLER_VALIDATION_H_
#define CHROME_ELEVATION_SERVICE_CALLER_VALIDATION_H_

#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/types/expected.h"
#include "base/win/windows_types.h"
#include "chrome/elevation_service/elevation_service_idl.h"

namespace base {
class FilePath;
class Process;
}

namespace elevation_service {

// Generates an opaque blob of validation data for the given `level` for the
// calling process `process`. Returns the validation data if it was successfully
// generated, or an error code - either a system HRESULT or a custom one defined
// in elevator.h. See elevation_service_idl.idl for the definition of the valid
// protection levels.
base::expected<std::vector<uint8_t>, HRESULT> GenerateValidationData(
    ProtectionLevel level,
    const base::Process& process);

// Validates `validation_data` validates for `process`, according to the
// validation policy for the level encoded in `validation_data` when it was
// generated. The returned HRESULT determines whether or not the validation
// passed. If validation failed and `log_message` is specified, then an extended
// log might be returned.
HRESULT ValidateData(const base::Process& process,
                     base::span<const uint8_t> validation_data,
                     std::string* log_message = nullptr);

// This internal function is exposed to tests, for testing. See documentation on
// `MaybeTrimProcessPath`.
base::FilePath MaybeTrimProcessPathForTesting(const base::FilePath& full_path);

}  // namespace elevation_service

#endif  // CHROME_ELEVATION_SERVICE_CALLER_VALIDATION_H_
