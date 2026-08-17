// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_USER_DATA_PROCESSING_CONSENT_STATES_H_
#define COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_USER_DATA_PROCESSING_CONSENT_STATES_H_

namespace one_time_tokens {

enum class ConsentState {
  kUndefined = 0,
  kUnknown = 1,
  kEnabled = 2,
  kDisabled = 3,
};

struct UserDataProcessingConsentStates {
  ConsentState comms_apps = ConsentState::kUndefined;
  ConsentState google_apps = ConsentState::kUndefined;

  bool operator==(const UserDataProcessingConsentStates& other) const = default;
};

}  // namespace one_time_tokens

#endif  // COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_USER_DATA_PROCESSING_CONSENT_STATES_H_
