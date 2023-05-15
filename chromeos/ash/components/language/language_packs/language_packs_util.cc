// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/language/language_packs/language_packs_util.h"

#include "base/logging.h"
#include "third_party/cros_system_api/dbus/dlcservice/dbus-constants.h"

namespace ash::language_packs {

FeatureIdsEnum GetFeatureIdValueForUma(const std::string& feature_id) {
  if (feature_id == kHandwritingFeatureId) {
    return FeatureIdsEnum::kHandwriting;
  }
  if (feature_id == kTtsFeatureId) {
    return FeatureIdsEnum::kTts;
  }

  // Default value of unknown.
  return FeatureIdsEnum::kUnknown;
}

FeatureSuccessEnum GetSuccessValueForUma(const std::string& feature_id,
                                         const bool success) {
  if (feature_id == kHandwritingFeatureId) {
    if (success) {
      return FeatureSuccessEnum::kHandwritingSuccess;
    } else {
      return FeatureSuccessEnum::kHandwritingFailure;
    }
  }
  if (feature_id == kTtsFeatureId) {
    if (success) {
      return FeatureSuccessEnum::kTtsSuccess;
    } else {
      return FeatureSuccessEnum::kTtsFailure;
    }
  }

  // Default value of unknown.
  if (success) {
    return FeatureSuccessEnum::kUnknownSuccess;
  } else {
    return FeatureSuccessEnum::kUnknownFailure;
  }
}

DlcErrorTypeEnum GetDlcErrorTypeForUma(const std::string& error_str) {
  if (error_str == dlcservice::kErrorNone) {
    return DlcErrorTypeEnum::kErrorNone;
  } else if (error_str == dlcservice::kErrorInternal) {
    return DlcErrorTypeEnum::kErrorInternal;
  } else if (error_str == dlcservice::kErrorBusy) {
    return DlcErrorTypeEnum::kErrorBusy;
  } else if (error_str == dlcservice::kErrorNeedReboot) {
    return DlcErrorTypeEnum::kErrorNeedReboot;
  } else if (error_str == dlcservice::kErrorInvalidDlc) {
    return DlcErrorTypeEnum::kErrorInvalidDlc;
  } else if (error_str == dlcservice::kErrorAllocation) {
    return DlcErrorTypeEnum::kErrorAllocation;
  } else if (error_str == dlcservice::kErrorNoImageFound) {
    return DlcErrorTypeEnum::kErrorNoImageFound;
  }

  // Return unknown if we can't recognize the error.
  LOG(ERROR) << "Wrong error message received from DLC Service";
  return DlcErrorTypeEnum::kErrorUnknown;
}

PackResult CreateInvalidDlcPackResult() {
  return {
      .operation_error = dlcservice::kErrorInvalidDlc,
      .pack_state = PackResult::WRONG_ID,
  };
}

PackResult ConvertDlcStateToPackResult(const dlcservice::DlcState& dlc_state) {
  PackResult result;

  switch (dlc_state.state()) {
    case dlcservice::DlcState_State_INSTALLED:
      result.pack_state = PackResult::INSTALLED;
      result.path = dlc_state.root_path();
      break;
    case dlcservice::DlcState_State_INSTALLING:
      result.pack_state = PackResult::IN_PROGRESS;
      break;
    case dlcservice::DlcState_State_NOT_INSTALLED:
      result.pack_state = PackResult::NOT_INSTALLED;
      break;
    default:
      result.pack_state = PackResult::UNKNOWN;
      break;
  }

  return result;
}

}  // namespace ash::language_packs
