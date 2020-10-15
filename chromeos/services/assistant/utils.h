// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_UTILS_H_
#define CHROMEOS_SERVICES_ASSISTANT_UTILS_H_

#include <string>

#include "base/macros.h"
#include "base/optional.h"

namespace base {
class FilePath;
}  // namespace base

namespace chromeos {
namespace assistant {

// Returns the root path of all user specific files.
base::FilePath GetRootPath();

// Returns the path where all downloaded LibAssistant resources are stored.
base::FilePath GetBaseAssistantDir();

// Creates the configuration for libassistant.
std::string CreateLibAssistantConfig(
    base::Optional<std::string> s3_server_uri_override,
    base::Optional<std::string> device_id_override);

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_UTILS_H_
