// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/language_packs/language_pack_manager.h"

#include <string_view>

#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/hash/hash.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice.pb.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice_client.h"
#include "chromeos/ash/components/language_packs/handwriting.h"
#include "chromeos/ash/components/language_packs/language_packs_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/cros_system_api/dbus/dlcservice/dbus-constants.h"

namespace ash::language_packs {
namespace {

const base::flat_map<std::string, std::string>& GetAllBasePackDlcIds() {
  // Map of all features and corresponding Base Pack DLC IDs.
  static const base::NoDestructor<base::flat_map<std::string, std::string>>
      all_dlc_ids({
          {kHandwritingFeatureId, "handwriting-base"},
      });

  return *all_dlc_ids;
}

// Finds the ID of the DLC corresponding to the Base Pack for a feature.
// Returns the DLC ID if the feature has a Base Pack or absl::nullopt
// otherwise.
absl::optional<std::string> GetDlcIdForBasePack(const std::string& feature_id) {
  // We search in the static list for the given |feature_id|.
  const auto it = GetAllBasePackDlcIds().find(feature_id);

  if (it == GetAllBasePackDlcIds().end()) {
    return absl::nullopt;
  }

  return it->second;
}

void InstallDlc(const std::string& dlc_id,
                DlcserviceClient::InstallCallback install_callback) {
  dlcservice::InstallRequest install_request;
  install_request.set_id(dlc_id);
  DlcserviceClient::Get()->Install(install_request, std::move(install_callback),
                                   base::DoNothing());
}

void OnInstallDlcComplete(OnInstallCompleteCallback callback,
                          const std::string& feature_id,
                          const std::string& locale,
                          const DlcserviceClient::InstallResult& dlc_result) {
  PackResult result = ConvertDlcInstallResultToPackResult(dlc_result);
  result.feature_id = feature_id;
  result.language_code = locale;

  const bool success = result.operation_error == PackResult::ErrorCode::kNone;
  if (!success) {
    if (feature_id == kHandwritingFeatureId) {
      base::UmaHistogramEnumeration(
          "ChromeOS.LanguagePacks.InstallError.Handwriting",
          GetDlcErrorTypeForUma(dlc_result.error));
    } else if (feature_id == kTtsFeatureId) {
      base::UmaHistogramEnumeration("ChromeOS.LanguagePacks.InstallError.Tts",
                                    GetDlcErrorTypeForUma(dlc_result.error));
    }
  }

  base::UmaHistogramEnumeration("ChromeOS.LanguagePacks.InstallPack.Success",
                                GetSuccessValueForUma(feature_id, success));

  std::move(callback).Run(result);
}

void OnUninstallDlcComplete(OnUninstallCompleteCallback callback,
                            std::string_view feature_id,
                            const std::string& locale,
                            const std::string& err) {
  PackResult result;
  result.feature_id = feature_id;
  result.language_code = locale;
  result.operation_error = ConvertDlcErrorToErrorCode(err);

  const bool success = err == dlcservice::kErrorNone;
  if (success) {
    result.pack_state = PackResult::StatusCode::kNotInstalled;
  } else {
    result.pack_state = PackResult::StatusCode::kUnknown;
  }

  base::UmaHistogramBoolean("ChromeOS.LanguagePacks.UninstallComplete.Success",
                            success);

  std::move(callback).Run(result);
}

void OnGetDlcState(GetPackStateCallback callback,
                   std::string_view feature_id,
                   const std::string& locale,
                   const std::string& err,
                   const dlcservice::DlcState& dlc_state) {
  PackResult result;
  // GetDlcState() returns 2 errors:
  // one for the DBus call and one for the actual DLC.
  // If the first error is set we can ignore the DLC state.
  if (err.empty() || err == dlcservice::kErrorNone) {
    result = ConvertDlcStateToPackResult(dlc_state);
  } else {
    result.operation_error = ConvertDlcErrorToErrorCode(err);
    result.pack_state = PackResult::StatusCode::kUnknown;
  }

  result.feature_id = feature_id;
  result.language_code = locale;

  std::move(callback).Run(result);
}

}  // namespace

const base::flat_map<PackSpecPair, std::string>& GetAllLanguagePackDlcIds() {
  // Map of all DLCs and corresponding IDs.
  // It's a map from PackSpecPair to DLC ID. The pair is <feature id, locale>.
  // Whenever a new DLC is created, it needs to be added here.
  // Clients of Language Packs don't need to know the IDs.
  // Note: if you add new languages here, make sure to add them to the metrics
  //       test `LanguagePackMetricsTest.CheckLanguageCodes`.
  static const base::NoDestructor<base::flat_map<PackSpecPair, std::string>>
      all_dlc_ids({
          // Handwriting Recognition.
          // Note: English is not included because it's still using LongForm.
          {{kHandwritingFeatureId, "am"}, "handwriting-am"},
          {{kHandwritingFeatureId, "ar"}, "handwriting-ar"},
          {{kHandwritingFeatureId, "be"}, "handwriting-be"},
          {{kHandwritingFeatureId, "bg"}, "handwriting-bg"},
          {{kHandwritingFeatureId, "bn"}, "handwriting-bn"},
          {{kHandwritingFeatureId, "ca"}, "handwriting-ca"},
          {{kHandwritingFeatureId, "cs"}, "handwriting-cs"},
          {{kHandwritingFeatureId, "da"}, "handwriting-da"},
          {{kHandwritingFeatureId, "de"}, "handwriting-de"},
          {{kHandwritingFeatureId, "el"}, "handwriting-el"},
          {{kHandwritingFeatureId, "es"}, "handwriting-es"},
          {{kHandwritingFeatureId, "et"}, "handwriting-et"},
          {{kHandwritingFeatureId, "fa"}, "handwriting-fa"},
          {{kHandwritingFeatureId, "fi"}, "handwriting-fi"},
          {{kHandwritingFeatureId, "fil"}, "handwriting-fil"},
          {{kHandwritingFeatureId, "fr"}, "handwriting-fr"},
          {{kHandwritingFeatureId, "ga"}, "handwriting-ga"},
          {{kHandwritingFeatureId, "gu"}, "handwriting-gu"},
          {{kHandwritingFeatureId, "hi"}, "handwriting-hi"},
          {{kHandwritingFeatureId, "hr"}, "handwriting-hr"},
          {{kHandwritingFeatureId, "hu"}, "handwriting-hu"},
          {{kHandwritingFeatureId, "hy"}, "handwriting-hy"},
          {{kHandwritingFeatureId, "id"}, "handwriting-id"},
          {{kHandwritingFeatureId, "is"}, "handwriting-is"},
          {{kHandwritingFeatureId, "it"}, "handwriting-it"},
          {{kHandwritingFeatureId, "iw"}, "handwriting-iw"},
          {{kHandwritingFeatureId, "ja"}, "handwriting-ja"},
          {{kHandwritingFeatureId, "ka"}, "handwriting-ka"},
          {{kHandwritingFeatureId, "kk"}, "handwriting-kk"},
          {{kHandwritingFeatureId, "km"}, "handwriting-km"},
          {{kHandwritingFeatureId, "kn"}, "handwriting-kn"},
          {{kHandwritingFeatureId, "ko"}, "handwriting-ko"},
          {{kHandwritingFeatureId, "lo"}, "handwriting-lo"},
          {{kHandwritingFeatureId, "lt"}, "handwriting-lt"},
          {{kHandwritingFeatureId, "lv"}, "handwriting-lv"},
          {{kHandwritingFeatureId, "ml"}, "handwriting-ml"},
          {{kHandwritingFeatureId, "mn"}, "handwriting-mn"},
          {{kHandwritingFeatureId, "mr"}, "handwriting-mr"},
          {{kHandwritingFeatureId, "ms"}, "handwriting-ms"},
          {{kHandwritingFeatureId, "mt"}, "handwriting-mt"},
          {{kHandwritingFeatureId, "my"}, "handwriting-my"},
          {{kHandwritingFeatureId, "ne"}, "handwriting-ne"},
          {{kHandwritingFeatureId, "nl"}, "handwriting-nl"},
          {{kHandwritingFeatureId, "no"}, "handwriting-no"},
          {{kHandwritingFeatureId, "or"}, "handwriting-or"},
          {{kHandwritingFeatureId, "pa"}, "handwriting-pa"},
          {{kHandwritingFeatureId, "pl"}, "handwriting-pl"},
          {{kHandwritingFeatureId, "pt"}, "handwriting-pt"},
          {{kHandwritingFeatureId, "ro"}, "handwriting-ro"},
          {{kHandwritingFeatureId, "ru"}, "handwriting-ru"},
          {{kHandwritingFeatureId, "si"}, "handwriting-si"},
          {{kHandwritingFeatureId, "sk"}, "handwriting-sk"},
          {{kHandwritingFeatureId, "sl"}, "handwriting-sl"},
          {{kHandwritingFeatureId, "sr"}, "handwriting-sr"},
          {{kHandwritingFeatureId, "sv"}, "handwriting-sv"},
          {{kHandwritingFeatureId, "ta"}, "handwriting-ta"},
          {{kHandwritingFeatureId, "te"}, "handwriting-te"},
          {{kHandwritingFeatureId, "th"}, "handwriting-th"},
          {{kHandwritingFeatureId, "ti"}, "handwriting-ti"},
          {{kHandwritingFeatureId, "tr"}, "handwriting-tr"},
          {{kHandwritingFeatureId, "uk"}, "handwriting-uk"},
          {{kHandwritingFeatureId, "ur"}, "handwriting-ur"},
          {{kHandwritingFeatureId, "vi"}, "handwriting-vi"},
          {{kHandwritingFeatureId, "zh"}, "handwriting-zh"},
          {{kHandwritingFeatureId, "zh-HK"}, "handwriting-zh-HK"},

          // Text-To-Speech.
          {{kTtsFeatureId, "bn"}, "tts-bn-bd-b"},
          {{kTtsFeatureId, "cs"}, "tts-cs-cz-b"},
          {{kTtsFeatureId, "da"}, "tts-da-dk-b"},
          {{kTtsFeatureId, "de"}, "tts-de-de-b"},
          {{kTtsFeatureId, "el"}, "tts-el-gr-b"},
          {{kTtsFeatureId, "en-au"}, "tts-en-au-b"},
          {{kTtsFeatureId, "en-gb"}, "tts-en-gb-b"},
          {{kTtsFeatureId, "en-us"}, "tts-en-us-b"},
          {{kTtsFeatureId, "es-es"}, "tts-es-es-b"},
          {{kTtsFeatureId, "es-us"}, "tts-es-us-b"},
          {{kTtsFeatureId, "fi"}, "tts-fi-fi-b"},
          {{kTtsFeatureId, "fil"}, "tts-fil-ph-b"},
          {{kTtsFeatureId, "fr"}, "tts-fr-fr-b"},
          {{kTtsFeatureId, "hi"}, "tts-hi-in-b"},
          {{kTtsFeatureId, "hu"}, "tts-hu-hu-b"},
          {{kTtsFeatureId, "id"}, "tts-id-id-b"},
          {{kTtsFeatureId, "it"}, "tts-it-it-b"},
          {{kTtsFeatureId, "ja"}, "tts-ja-jp-b"},
          {{kTtsFeatureId, "km"}, "tts-km-kh-b"},
          {{kTtsFeatureId, "ko"}, "tts-ko-kr-b"},
          {{kTtsFeatureId, "nb"}, "tts-nb-no-b"},
          {{kTtsFeatureId, "ne"}, "tts-ne-np-b"},
          {{kTtsFeatureId, "nl"}, "tts-nl-nl-b"},
          {{kTtsFeatureId, "pl"}, "tts-pl-pl-b"},
          {{kTtsFeatureId, "pt"}, "tts-pt-br-b"},
          {{kTtsFeatureId, "si"}, "tts-si-lk-b"},
          {{kTtsFeatureId, "sk"}, "tts-sk-sk-b"},
          {{kTtsFeatureId, "sv"}, "tts-sv-se-b"},
          {{kTtsFeatureId, "th"}, "tts-th-th-b"},
          {{kTtsFeatureId, "tr"}, "tts-tr-tr-b"},
          {{kTtsFeatureId, "uk"}, "tts-uk-ua-b"},
          {{kTtsFeatureId, "vi"}, "tts-vi-vn-b"},
          {{kTtsFeatureId, "yue"}, "tts-yue-hk-b"},
      });

  return *all_dlc_ids;
}

// TODO: b/294162606 - Calling this function with a `std::string_view` or a
// `const char*` argument causes two string copies per argument - one to call
// the function, and one to create the `PackSpecPair` to look up in the map.
// Either refactor this function to take in a `std::string_view` to reduce it
// down to one string copy per argument, use heterogeneous lookup
// (https://abseil.io/tips/144) to reduce it down to zero string copies, or
// rewrite this function completely.
absl::optional<std::string> GetDlcIdForLanguagePack(
    const std::string& feature_id,
    const std::string& locale) {
  // We search in the static list for the given Pack spec.
  const PackSpecPair spec(feature_id, locale);
  const auto it = GetAllLanguagePackDlcIds().find(spec);

  if (it == GetAllLanguagePackDlcIds().end()) {
    return absl::nullopt;
  }

  return it->second;
}

///////////////////////////////////////////////////////////
// PackResult constructors and destructors.
PackResult::PackResult() {
  this->pack_state = PackResult::StatusCode::kUnknown;
}

PackResult::~PackResult() = default;

PackResult::PackResult(const PackResult&) = default;
///////////////////////////////////////////////////////////

bool LanguagePackManager::IsPackAvailable(const std::string& feature_id,
                                          const std::string& input_locale) {
  const std::string locale = ResolveLocale(feature_id, input_locale);

  // We search in the static list for the given Pack spec.
  const PackSpecPair spec(feature_id, locale);
  return base::Contains(GetAllLanguagePackDlcIds(), spec);
}

void LanguagePackManager::InstallPack(const std::string& feature_id,
                                      const std::string& input_locale,
                                      OnInstallCompleteCallback callback) {
  const std::string locale = ResolveLocale(feature_id, input_locale);
  const absl::optional<std::string> dlc_id =
      GetDlcIdForLanguagePack(feature_id, locale);

  // If the given Language Pack doesn't exist, run callback and don't reach the
  // DLC Service.
  if (!dlc_id) {
    std::move(callback).Run(CreateInvalidDlcPackResult());
    return;
  }

  InstallDlc(*dlc_id, base::BindOnce(&OnInstallDlcComplete, std::move(callback),
                                     feature_id, locale));
}

void LanguagePackManager::GetPackState(const std::string& feature_id,
                                       const std::string& input_locale,
                                       GetPackStateCallback callback) {
  const std::string locale = ResolveLocale(feature_id, input_locale);
  const absl::optional<std::string> dlc_id =
      GetDlcIdForLanguagePack(feature_id, locale);

  // If the given Language Pack doesn't exist, run callback and don't reach the
  // DLC Service.
  if (!dlc_id) {
    std::move(callback).Run(CreateInvalidDlcPackResult());
    return;
  }

  base::UmaHistogramSparse("ChromeOS.LanguagePacks.GetPackState.LanguageCode",
                           static_cast<int32_t>(base::PersistentHash(locale)));
  base::UmaHistogramEnumeration("ChromeOS.LanguagePacks.GetPackState.FeatureId",
                                GetFeatureIdValueForUma(feature_id));

  DlcserviceClient::Get()->GetDlcState(
      *dlc_id,
      base::BindOnce(&OnGetDlcState, std::move(callback), feature_id, locale));
}

void LanguagePackManager::RemovePack(const std::string& feature_id,
                                     const std::string& input_locale,
                                     OnUninstallCompleteCallback callback) {
  const std::string locale = ResolveLocale(feature_id, input_locale);
  const absl::optional<std::string> dlc_id =
      GetDlcIdForLanguagePack(feature_id, locale);

  // If the given Language Pack doesn't exist, run callback and don't reach the
  // DLC Service.
  if (!dlc_id) {
    std::move(callback).Run(CreateInvalidDlcPackResult());
    return;
  }

  DlcserviceClient::Get()->Uninstall(
      *dlc_id, base::BindOnce(&OnUninstallDlcComplete, std::move(callback),
                              feature_id, locale));
}

void LanguagePackManager::InstallBasePack(
    const std::string& feature_id,
    OnInstallBasePackCompleteCallback callback) {
  const absl::optional<std::string> dlc_id = GetDlcIdForBasePack(feature_id);

  // If the given |feature_id| doesn't have a Base Pack, run callback and
  // don't reach the DLC Service.
  if (!dlc_id) {
    std::move(callback).Run(CreateInvalidDlcPackResult());
    return;
  }

  base::UmaHistogramEnumeration(
      "ChromeOS.LanguagePacks.InstallBasePack.FeatureId",
      GetFeatureIdValueForUma(feature_id));

  InstallDlc(*dlc_id, base::BindOnce(&OnInstallDlcComplete, std::move(callback),
                                     feature_id, ""));
}

void LanguagePackManager::UpdatePacksForOobe(
    const std::string& input_locale,
    OnUpdatePacksForOobeCallback callback) {
  if (!IsOobe()) {
    DLOG(ERROR) << "Language Packs: UpdatePackForOobe called while not in OOBE";
    return;
  }

  // For now, TTS is the only feature we want to install during OOBE.
  // In the future we'll have a function that returns the list of features to
  // install.
  const std::string locale = ResolveLocale(kTtsFeatureId, input_locale);
  const absl::optional<std::string> dlc_id =
      GetDlcIdForLanguagePack(kTtsFeatureId, locale);

  if (dlc_id) {
    base::UmaHistogramBoolean("ChromeOS.LanguagePacks.Oobe.ValidLocale", true);
    InstallDlc(*dlc_id,
               base::BindOnce(&OnInstallDlcComplete, std::move(callback),
                              kTtsFeatureId, locale));
  } else {
    base::UmaHistogramBoolean("ChromeOS.LanguagePacks.Oobe.ValidLocale", false);
    DLOG(ERROR) << "Language Packs: UpdatePacksForOobe locale does not exist";
    std::move(callback).Run(CreateInvalidDlcPackResult());
  }
}

void LanguagePackManager::AddObserver(Observer* const observer) {
  observers_.AddObserver(observer);
}

void LanguagePackManager::RemoveObserver(Observer* const observer) {
  observers_.RemoveObserver(observer);
}

void LanguagePackManager::NotifyPackStateChanged(
    std::string_view feature_id,
    std::string_view locale,
    const dlcservice::DlcState& dlc_state) {
  PackResult result = ConvertDlcStateToPackResult(dlc_state);
  result.feature_id = feature_id;
  result.language_code = locale;
  for (Observer& observer : observers_) {
    observer.OnPackStateChanged(result);
  }
}

void LanguagePackManager::OnDlcStateChanged(
    const dlcservice::DlcState& dlc_state) {
  // As of now, we only have Handwriting as a client.
  // We will check the full list once we have more than one DLC.
  const absl::optional<std::string> handwriting_locale =
      DlcToHandwritingLocale(dlc_state.id());
  if (!handwriting_locale.has_value()) {
    return;
  }

  NotifyPackStateChanged(kHandwritingFeatureId, *handwriting_locale, dlc_state);
}

LanguagePackManager::LanguagePackManager() {
  obs_.Observe(DlcserviceClient::Get());
}

LanguagePackManager::~LanguagePackManager() {}

void LanguagePackManager::ResetForTesting() {
  observers_.Clear();
}

// static
LanguagePackManager* LanguagePackManager::GetInstance() {
  static base::NoDestructor<LanguagePackManager> instance;
  return instance.get();
}

}  // namespace ash::language_packs
