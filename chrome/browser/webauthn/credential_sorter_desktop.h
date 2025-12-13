// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_CREDENTIAL_SORTER_DESKTOP_H_
#define CHROME_BROWSER_WEBAUTHN_CREDENTIAL_SORTER_DESKTOP_H_

#include <vector>

#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "chrome/browser/webauthn/credential_sorter.h"

namespace webauthn::sorting {

// Processes a list of mechanisms by deduplicating and sorting them.
// - Deduplication: For mechanisms with the same `mechanism.name`,
//   selects the "best" one based on rules (e.g., recency).
// - Sorting: Orders the resulting mechanisms based on preference rules
//   (e.g. recency, passkeys preferred, alphabetical).
//
// This logic is applied when ui_presentation is
// `UIPresentation::kModalImmediate`. For other presentations, it may apply a
// default behavior or pass through the mechanisms unmodified.
std::vector<AuthenticatorRequestDialogModel::Mechanism> SortMechanisms(
    std::vector<AuthenticatorRequestDialogModel::Mechanism> mechanisms,
    UIPresentation ui_presentation);

}  // namespace webauthn::sorting

#endif  // CHROME_BROWSER_WEBAUTHN_CREDENTIAL_SORTER_DESKTOP_H_
