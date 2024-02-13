// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/language_packs/language_packs_util.h"

#include <optional>

#include "ash/constants/ash_pref_names.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice_client.h"
#include "chromeos/ash/components/language_packs/language_pack_manager.h"
#include "components/language/core/common/locale_util.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/session_manager_types.h"
#include "third_party/cros_system_api/dbus/dlcservice/dbus-constants.h"

namespace ash::language_packs {

namespace {

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
      base::EqualsCaseInsensitiveASCII(input_locale, "es-us") ||
      base::EqualsCaseInsensitiveASCII(input_locale, "pt-br") ||
      base::EqualsCaseInsensitiveASCII(input_locale, "pt-pt")) {
    return base::ToLowerASCII(input_locale);
  }
  return std::string(language::ExtractBaseLanguage(input_locale));
}

}  // namespace

FeatureIdsEnum GetFeatureIdValueForUma(const std::string& feature_id) {
  if (feature_id == kHandwritingFeatureId) {
    return FeatureIdsEnum::kHandwriting;
  }
  if (feature_id == kTtsFeatureId) {
    return FeatureIdsEnum::kTts;
  }
  if (feature_id == kFontsFeatureId) {
    return FeatureIdsEnum::kFonts;
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
  if (feature_id == kFontsFeatureId) {
    if (success) {
      return FeatureSuccessEnum::kFontsSuccess;
    } else {
      return FeatureSuccessEnum::kFontsFailure;
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
  PackResult result;
  result.operation_error = PackResult::ErrorCode::kWrongId;
  result.pack_state = PackResult::StatusCode::kUnknown;
  return result;
}

PackResult ConvertDlcStateToPackResult(const dlcservice::DlcState& dlc_state) {
  PackResult result;

  switch (dlc_state.state()) {
    case dlcservice::DlcState_State_INSTALLED:
      result.pack_state = PackResult::StatusCode::kInstalled;
      result.path = dlc_state.root_path();
      break;
    case dlcservice::DlcState_State_INSTALLING:
      result.pack_state = PackResult::StatusCode::kInProgress;
      break;
    case dlcservice::DlcState_State_NOT_INSTALLED:
      result.pack_state = PackResult::StatusCode::kNotInstalled;
      break;
    default:
      result.pack_state = PackResult::StatusCode::kUnknown;
      break;
  }

  result.operation_error =
      ConvertDlcErrorToErrorCode(dlc_state.last_error_code());

  return result;
}

PackResult ConvertDlcInstallResultToPackResult(
    const DlcserviceClient::InstallResult& install_result) {
  PackResult result;

  result.operation_error = ConvertDlcErrorToErrorCode(install_result.error);

  if (result.operation_error == PackResult::ErrorCode::kNone) {
    result.pack_state = PackResult::StatusCode::kInstalled;
    result.path = install_result.root_path;
  } else {
    result.pack_state = PackResult::StatusCode::kUnknown;
  }

  return result;
}

PackResult::ErrorCode ConvertDlcErrorToErrorCode(std::string_view err) {
  if (err.empty() || err == dlcservice::kErrorNone) {
    return PackResult::ErrorCode::kNone;
  } else if (err == dlcservice::kErrorInvalidDlc) {
    return PackResult::ErrorCode::kWrongId;
  } else if (err == dlcservice::kErrorNeedReboot) {
    return PackResult::ErrorCode::kNeedReboot;
  } else if (err == dlcservice::kErrorAllocation) {
    return PackResult::ErrorCode::kAllocation;
  } else {
    // We use INTERNAL for all remaining errors thrown by DLC Service because
    // there's nothing we or the client can do about it.
    // Error code BUSY is never returned.
    return PackResult::ErrorCode::kOther;
  }
}

const std::string ResolveLocale(const std::string& feature_id,
                                const std::string& locale) {
  if (feature_id == kHandwritingFeatureId) {
    return ResolveLocaleForHandwriting(locale);
  } else if (feature_id == kTtsFeatureId) {
    return ResolveLocaleForTts(locale);
  } else if (feature_id == kFontsFeatureId) {
    // Language pack resolution is handled by the client.
    return locale;
  } else {
    DLOG(ERROR) << "ResolveLocale called with wrong feature_id";
    return "";
  }
}

bool IsOobe() {
  return session_manager::SessionManager::Get()->session_state() ==
         session_manager::SessionState::OOBE;
}

base::flat_set<std::string> MapThenFilterStrings(
    base::span<const std::string> inputs,
    base::RepeatingCallback<std::optional<std::string>(const std::string&)>
        input_mapping) {
  std::vector<std::string> output;
  for (const auto& input : inputs) {
    const std::optional<std::string> result = input_mapping.Run(input);
    if (result.has_value()) {
      output.push_back(std::move(*result));
    }
  }

  return output;
}

std::vector<std::string> ExtractInputMethodsFromPrefs(PrefService* prefs) {
  const std::string& preload_engines_str =
      prefs->GetString(prefs::kLanguagePreloadEngines);
  return base::SplitString(preload_engines_str, ",", base::TRIM_WHITESPACE,
                           base::SPLIT_WANT_ALL);
}

}  // namespace ash::language_packs
