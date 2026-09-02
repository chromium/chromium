// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTEXTUAL_SEARCH_FOOTPRINTS_PUBLIC_DRIVE_DISCLAIMER_CONTROLLER_H_
#define COMPONENTS_CONTEXTUAL_SEARCH_FOOTPRINTS_PUBLIC_DRIVE_DISCLAIMER_CONTROLLER_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/contextual_search/footprints/public/constants.h"
#include "components/contextual_search/footprints/public/fpop_service.h"
#include "components/contextual_search/footprints/public/proto/footprints_oneplatform.pb.h"

namespace drive_picker {

// Manages the verification of the Google Drive disclaimer status.
// It queries the Footprints Access Control Service (FACS) backend via
// `FpopService` to check whether the user has accepted the disclaimer.
class DriveDisclaimerController {
 public:
  enum class DisclaimerStatus {
    kAccepted,
    kNotAccepted,

    // The setting is restricted by the FACS backend. This occurs when the
    // setting cannot be enabled due to account policy or restrictions (e.g.,
    // parental control / supervised accounts, minor / under-age restrictions,
    // managed Enterprise/EDU accounts, geographic/location limits, or other
    // backend configurations). Callers should handle this by avoiding prompting
    // the user, as they cannot modify this setting.
    kRestricted,
  };

  // Represents the raw eligibility status returned by the FPOP API.
  // These values mirror the internal `ConsentEligibilityStatus` proto enum.
  enum class ConsentEligibilityStatus {
    kUnknown = 0,
    kCanConsent = 1,
    kCannotConsent = 2,
    kAlreadyConsented = 3,
  };

  // Converts a DisclaimerStatus to its string representation.
  static std::string DisclaimerStatusToString(DisclaimerStatus status);

  explicit DriveDisclaimerController(
      std::unique_ptr<contextual_search::FpopService> fpop_service);
  ~DriveDisclaimerController();

  DriveDisclaimerController(const DriveDisclaimerController&) = delete;
  DriveDisclaimerController& operator=(const DriveDisclaimerController&) =
      delete;

  // Asynchronously verifies whether the user has accepted the required Drive
  // privacy disclaimer via the Footprints backend.
  void CheckDisclaimerStatusAsync(
      base::OnceCallback<void(DisclaimerStatus status)> completion_callback);

 private:
  void OnShouldShowMobileConsentFlowResponse(
      base::OnceCallback<void(DisclaimerStatus status)> completion_callback,
      bool success,
      const footprints::oneplatform::ShouldShowMobileConsentFlowResponse&
          response);

  std::unique_ptr<contextual_search::FpopService> fpop_service_;
  base::WeakPtrFactory<DriveDisclaimerController> weak_factory_{this};
};

}  // namespace drive_picker

#endif  // COMPONENTS_CONTEXTUAL_SEARCH_FOOTPRINTS_PUBLIC_DRIVE_DISCLAIMER_CONTROLLER_H_
