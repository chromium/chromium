// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_PUBLIC_CPP_ASSISTANT_PREFS_H_
#define CHROMEOS_SERVICES_ASSISTANT_PUBLIC_CPP_ASSISTANT_PREFS_H_

class PrefRegistrySimple;

namespace chromeos {
namespace assistant {
namespace prefs {

// The status of the user's consent. The enum values cannot be changed because
// they are persisted on disk.
enum ConsentStatus {
  // The status is unknown.
  kUnknown = 0,

  // The user accepted activity control access.
  kActivityControlAccepted = 1,

  // The user is not authorized to give consent.
  kUnauthorized = 2,

  // The user's consent information is not found. This is typically the case
  // when consent from the user has never been requested.
  kNotFound = 3,
};

extern const char kAssistantConsentStatus[];
extern const char kAssistantContextEnabled[];
extern const char kAssistantDisabledByPolicy[];
extern const char kAssistantEnabled[];
extern const char kAssistantHotwordAlwaysOn[];
extern const char kAssistantHotwordEnabled[];
extern const char kAssistantLaunchWithMicOpen[];
extern const char kAssistantNotificationEnabled[];

// Registers Assistant specific profile preferences for browser prefs.
void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace prefs
}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_PUBLIC_CPP_ASSISTANT_PREFS_H_
