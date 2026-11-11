// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_GPM_PIN_AVAILABILITY_H_
#define CHROME_BROWSER_WEBAUTHN_GPM_PIN_AVAILABILITY_H_

namespace webauthn {

// LINT.IfChange(GpmPinAvailability)
//
// An enum that expresses whether a GPM PIN is set on an account.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class GpmPinAvailability {
  // The PIN is set but not usable (there are 0 remaining attempts for
  // entering the pin).
  kGpmPinSetButNotUsable = 0,
  // The PIN is set and usable (there are > 0 remaining attempts for entering
  // the pin).
  kGpmPinSetAndUsable = 1,
  // The PIN is unset.
  kGpmPinUnset = 2,
  kMaxValue = kGpmPinUnset,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/webauthn/enums.xml:GpmPinAvailability)

}  // namespace webauthn

#endif  // CHROME_BROWSER_WEBAUTHN_GPM_PIN_AVAILABILITY_H_
