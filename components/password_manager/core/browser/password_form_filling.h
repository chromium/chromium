// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FORM_FILLING_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FORM_FILLING_H_

#include <optional>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "components/autofill/core/common/unique_ids.h"

namespace autofill {
struct PasswordFormFillData;
}  // namespace autofill

namespace url {
class Origin;
}  // namespace url

namespace password_manager {
class PasswordFormMetricsRecorder;
class PasswordManagerClient;
class PasswordManagerDriver;
struct PasswordForm;

// Enum detailing the browser process' best belief what kind of credential
// filling is used in the renderer for a given password form.
//
// NOTE: The renderer can still decide not to fill due to reasons that are only
// known to it, thus this enum contains only probable filling kinds. Due to the
// inherent inaccuracy DO NOT record this enum to UMA.
enum class LikelyFormFilling {
  // There are no credentials to fill.
  kNoFilling,
  // The form is rendered with the best matching credential filled in.
  kFillOnPageLoad,
  // The form requires an active selection of the username before passwords
  // are filled.
  kFillOnAccountSelect,
  // The form is rendered with initial account suggestions, but no credential
  // is filled in.
  kShowInitialAccountSuggestions,
};

LikelyFormFilling SendFillInformationToRenderer(
    PasswordManagerClient* client,
    PasswordManagerDriver* driver,
    const PasswordForm& observed_form,
    base::span<const PasswordForm> best_matches,
    base::span<const PasswordForm> federated_matches,
    const PasswordForm* preferred_match,
    PasswordFormMetricsRecorder* metrics_recorder,
    bool webauthn_suggestions_available,
    base::span<autofill::FieldRendererId> suggestion_banned_fields);

// Create a PasswordFormFillData structure in preparation for filling a form
// identified by |form_on_page|, with credentials from |preferred_match| and
// |matches|. |preferred_match| should equal to one of matches.
// If |wait_for_username| is true then fill on account select will be used.
autofill::PasswordFormFillData CreatePasswordFormFillData(
    const PasswordForm& form_on_page,
    base::span<const PasswordForm> best_matches,
    std::optional<PasswordForm> preferred_match,
    const url::Origin& main_frame_origin,
    bool wait_for_username,
    base::span<autofill::FieldRendererId> suggestion_banned_fields);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FORM_FILLING_H_
