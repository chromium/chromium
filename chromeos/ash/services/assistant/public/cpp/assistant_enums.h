// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_ASSISTANT_PUBLIC_CPP_ASSISTANT_ENUMS_H_
#define CHROMEOS_ASH_SERVICES_ASSISTANT_PUBLIC_CPP_ASSISTANT_ENUMS_H_

namespace ash::assistant {
// The initial state is NOT_READY, after Assistant service started it becomes
// READY. When Assistant UI shows up the state becomes VISIBLE.
enum AssistantStatus {
  // The Assistant service is not ready yet.
  NOT_READY = 0,
  // The Assistant service is ready.
  READY,
};

enum AssistantAllowedState {
  // Assistant feature is allowed.
  ALLOWED = 0,
  // Disallowed because search and assistant is disabled by policy.
  DISALLOWED_BY_POLICY = 1,
  // Disallowed because user's locale is not compatible.
  DISALLOWED_BY_LOCALE = 2,
  // Disallowed because current user is not primary user.
  DISALLOWED_BY_NONPRIMARY_USER = 3,
  // DISALLOWED_BY_SUPERVISED_USER = 4, // Deprecated.
  // Disallowed because incognito mode.
  DISALLOWED_BY_INCOGNITO = 5,
  // Disallowed because the device is in demo mode.
  DISALLOWED_BY_DEMO_MODE = 6,
  // Disallowed because the device is in public session.
  DISALLOWED_BY_PUBLIC_SESSION = 7,
  // Disallowed because the user's account type is currently not supported.
  DISALLOWED_BY_ACCOUNT_TYPE = 8,
  // Disallowed because the device is in Kiosk mode.
  DISALLOWED_BY_KIOSK_MODE = 9,
  // Disallowed because no libassistant binary available.
  DISALLOWED_BY_NO_BINARY = 10,
  // Disallowed because new entry point.
  DISALLOWED_BY_NEW_ENTRY_POINT = 11,

  MAX_VALUE = DISALLOWED_BY_NEW_ENTRY_POINT,
};

}  // namespace ash::assistant

#endif  // CHROMEOS_ASH_SERVICES_ASSISTANT_PUBLIC_CPP_ASSISTANT_ENUMS_H_
