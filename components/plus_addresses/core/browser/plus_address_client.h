// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_CORE_BROWSER_PLUS_ADDRESS_CLIENT_H_
#define COMPONENTS_PLUS_ADDRESSES_CORE_BROWSER_PLUS_ADDRESS_CLIENT_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "components/autofill/core/common/plus_address_survey_type.h"

namespace url {
class Origin;
}

namespace plus_addresses {

// An interface for embedder-specific plus address actions, e.g. Chrome on
// Desktop.
class PlusAddressClient {
 public:
  enum class PlusAddressErrorDialogType {
    kGenericError,
    // The quota for plus address creation is exhausted (account-wide or
    // site-specific).
    kQuotaExhausted,
    // The network request timed out.
    kTimeout,
  };

  // Callback to be run with the created plus address.
  using PlusAddressCreationCallback =
      base::OnceCallback<void(const std::string&)>;

  // Callback to run when the user decides to undo the plus address full form
  // fulling. If the user never undoes the operation, the callback is never
  // triggered.
  using EmailOverrideUndoCallback = base::OnceClosure;

  virtual ~PlusAddressClient() = default;

  // Orchestrates UI for enterprise plus address creation; no-op
  // except on supported platforms.
  virtual void OfferPlusAddressCreation(
      const url::Origin& main_frame_origin,
      bool is_manual_fallback,
      PlusAddressCreationCallback callback) = 0;

  // Notifies the user via a patform specific UI that full form filling for plus
  // addresses has occurred (i.e. the filled email address was overridden by the
  // plus address). The UI provides the user with the option to undo the
  // filling operation back to back to `original_email`, in which case the
  // `email_override_undo_callback` is triggered.
  virtual void ShowPlusAddressEmailOverrideNotification(
      const std::string& original_email,
      EmailOverrideUndoCallback email_override_undo_callback) = 0;

  // Shows UI to inform the user about a plus address error (apart from
  // affiliation errors).
  virtual void ShowPlusAddressError(
      PlusAddressErrorDialogType error_dialog_type,
      base::OnceClosure on_accepted) = 0;

  // Shows UI to inform the user about a plus address affiliation error.
  virtual void ShowPlusAddressAffiliationError(
      const std::u16string& affiliated_domain,
      const std::u16string& affiliated_plus_address,
      base::OnceClosure on_accepted) = 0;

  // Triggers the HaTS survey of the `survey_type`.
  virtual void TriggerPlusAddressUserPerceptionSurvey(
      hats::SurveyType survey_type) = 0;
};

}  // namespace plus_addresses

#endif  // COMPONENTS_PLUS_ADDRESSES_CORE_BROWSER_PLUS_ADDRESS_CLIENT_H_
