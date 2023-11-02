// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_PUBLIC_SHARED_UTILS_H_
#define CHROMEOS_SERVICES_ASSISTANT_PUBLIC_SHARED_UTILS_H_

#include <string>

#include "base/component_export.h"

namespace chromeos {
namespace assistant {

// Models an Interaction.
struct COMPONENT_EXPORT(ASSISTANT_SERVICE_SHARED) InteractionInfo {
  const int interaction_id;
  const std::string user_id;
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_PUBLIC_SHARED_UTILS_H_
