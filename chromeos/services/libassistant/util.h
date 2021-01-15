// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_LIBASSISTANT_UTIL_H_
#define CHROMEOS_SERVICES_LIBASSISTANT_UTIL_H_

#include <string>
#include "base/optional.h"

namespace base {
class FilePath;
}  // namespace base

namespace chromeos {
namespace libassistant {

// Creates the configuration for libassistant.
std::string CreateLibAssistantConfig(
    base::Optional<std::string> s3_server_uri_override,
    base::Optional<std::string> device_id_override,
    bool log_in_home_dir);

// Returns the path where all downloaded LibAssistant resources are stored.
base::FilePath GetBaseAssistantDir();

}  // namespace libassistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_LIBASSISTANT_UTIL_H_
