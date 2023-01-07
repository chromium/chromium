// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_ELEVATION_SERVICE_CALLER_VALIDATION_H_
#define CHROME_ELEVATION_SERVICE_CALLER_VALIDATION_H_

#include <string>

#include "chrome/elevation_service/elevation_service_idl.h"

namespace base {
class Process;
}

namespace elevation_service {

// Generates an opaque blob of validation data for the given `level` for the
// calling process `process`. Returns the validation data if it was successfully
// generated, or empty string otherwise. See elevation_service_idl.idl for the
// definition of the valid protection levels.
std::string GenerateValidationData(ProtectionLevel level,
                                   const base::Process& process);

// Validates `validation_data` validates for `process`, according to the
// validation policy for the level encoded in `validation_data` when it was
// generated. Returns true if the validation passed.
bool ValidateData(const base::Process& process,
                  const std::string& validation_data);

}  // namespace elevation_service

#endif  // CHROME_ELEVATION_SERVICE_CALLER_VALIDATION_H_
