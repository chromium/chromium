// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_ASSISTANT_PUBLIC_CPP_SWITCHES_H_
#define CHROMEOS_ASH_SERVICES_ASSISTANT_PUBLIC_CPP_SWITCHES_H_

#include "base/component_export.h"

namespace ash::assistant::switches {

// NOTE: Switches are reserved for developer-facing options. End-user facing
// features should use base::Feature. See features.h.

// Forces the assistant first-run onboarding view, even if the user has seen it
// before. Useful when working on UI layout.
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const char kForceAssistantOnboarding[];

// Redirects libassistant logging to /var/log/chrome/. This is mainly used to
// help collect logs when running tests.
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const char kRedirectLibassistantLogging[];

// Redirects libassistant logging to stdout. This is mainly used to help test
// locally.
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const char kDisableLibAssistantLogfile[];

}  // namespace ash::assistant::switches

// TODO(b/258750971): remove when internal assistant codes are migrated to
// namespace ash.
namespace chromeos::assistant {
namespace switches = ::ash::assistant::switches;
}

#endif  // CHROMEOS_ASH_SERVICES_ASSISTANT_PUBLIC_CPP_SWITCHES_H_
