// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/language/language_packs/language_packs_util.h"

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "components/language/core/common/locale_util.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/session_manager_types.h"
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

const std::string ResolveLocaleForHandwriting(const std::string& input_locale) {
  // Chinese HongKong is an exception.
  if (base::EqualsCaseInsensitiveASCII(input_locale, "zh-hk")) {
    return "zh-HK";
  }
  return std::string(language::ExtractBaseLanguage(input_locale));
}

const std::string ResolveLocaleForTts(const std::string& input_locale) {
  // Consider exceptions first.
  if (base::EqualsCaseInsensitiveASCII(input_locale, "en-au") ||
      base::EqualsCaseInsensitiveASCII(input_locale, "en-gb") ||
      base::EqualsCaseInsensitiveASCII(input_locale, "en-us") ||
      base::EqualsCaseInsensitiveASCII(input_locale, "es-es") ||
      base::EqualsCaseInsensitiveASCII(input_locale, "es-us")) {
    return base::ToLowerASCII(input_locale);
  }
  return std::string(language::ExtractBaseLanguage(input_locale));
}

bool IsOobe() {
  return session_manager::SessionManager::Get()->session_state() ==
         session_manager::SessionState::OOBE;
}

}  // namespace ash::language_packs
