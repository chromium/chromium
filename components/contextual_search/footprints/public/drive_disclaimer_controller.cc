// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_search/footprints/public/drive_disclaimer_controller.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"

namespace drive_picker {

namespace {

// FPOP Backend IDs.
// For source definitions, see ConsentFlowId in:
// google3/java/com/google/personalization/footprints/
// transparencyandcontrol/proto/flow.proto
constexpr int kConsentFlowWorkspaceAim = 53;
constexpr int kProductIdChrome = 126;
constexpr int kKnownCallerSuperrootAsms = 1;

// For source definitions, see ConsentSettingId in:
// google3/java/com/google/personalization/footprints/
// transparencyandcontrol/proto/consent.proto
constexpr int kConsentIdWorkspaceAim = 38;

// ConsentEligibilityStatus enum values mapping.
// For source definitions, see ConsentEligibilityStatus in:
// google3/java/com/google/personalization/footprints/
// transparencyandcontrol/proto/consent.proto
constexpr int kEligibilityCannotConsent = 2;
constexpr int kEligibilityAlreadyConsented = 3;

}  // namespace

// static
std::string DriveDisclaimerController::DisclaimerStatusToString(
    DisclaimerStatus status) {
  switch (status) {
    case DisclaimerStatus::kAccepted:
      return "Accepted";
    case DisclaimerStatus::kNotAccepted:
      return "NotAccepted";
    case DisclaimerStatus::kRestricted:
      return "Restricted";
  }
}

DriveDisclaimerController::DriveDisclaimerController(
    std::unique_ptr<contextual_search::FpopService> fpop_service)
    : fpop_service_(std::move(fpop_service)) {
  CHECK(fpop_service_);
}

DriveDisclaimerController::~DriveDisclaimerController() = default;

void DriveDisclaimerController::CheckDisclaimerStatusAsync(
    base::OnceCallback<void(DisclaimerStatus status)> completion_callback) {
  footprints::oneplatform::ShouldShowMobileConsentFlowRequest request;
  request.set_consent_flow(kConsentFlowWorkspaceAim);

  auto* caller = request.mutable_caller();
  caller->set_product_id(kProductIdChrome);
  caller->set_known_caller(kKnownCallerSuperrootAsms);

  auto* setting = request.add_settings();
  setting->set_consent_id(kConsentIdWorkspaceAim);

  fpop_service_->ShouldShowMobileConsentFlow(
      request,
      base::BindOnce(
          &DriveDisclaimerController::OnShouldShowMobileConsentFlowResponse,
          weak_factory_.GetWeakPtr(), std::move(completion_callback)));
}

void DriveDisclaimerController::OnShouldShowMobileConsentFlowResponse(
    base::OnceCallback<void(DisclaimerStatus status)> completion_callback,
    bool success,
    const footprints::oneplatform::ShouldShowMobileConsentFlowResponse&
        response) {
  if (!success || !response.has_should_show_flow_result()) {
    std::move(completion_callback).Run(DisclaimerStatus::kRestricted);
    return;
  }

  int32_t status = response.should_show_flow_result().eligibility().status();
  DisclaimerStatus final_status = DisclaimerStatus::kNotAccepted;

  if (status == kEligibilityCannotConsent) {
    final_status = DisclaimerStatus::kRestricted;
  } else if (status == kEligibilityAlreadyConsented) {
    final_status = DisclaimerStatus::kAccepted;
  } else {
    final_status = DisclaimerStatus::kNotAccepted;
  }

  std::move(completion_callback).Run(final_status);
}

}  // namespace drive_picker
