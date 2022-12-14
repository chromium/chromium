// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/language/language_packs/language_pack_manager.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/hash/hash.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice.pb.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice_client.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash::language_packs {
namespace {

// Feature IDs.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// See enum LanguagePackFeatureIds in tools/metrics/histograms/enums.xml.
enum class FeatureIdsEnum {
  kUnknown = 0,
  kHandwriting = 1,
  kTts = 2,
  kMaxValue = kTts,
};

// PackResult that is returned by an invalid feature ID is specified.
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
          {{kHandwritingFeatureId, "tl"}, "handwriting-tl"},
          {{kHandwritingFeatureId, "tr"}, "handwriting-tr"},
          {{kHandwritingFeatureId, "uk"}, "handwriting-uk"},
          {{kHandwritingFeatureId, "ur"}, "handwriting-ur"},
          {{kHandwritingFeatureId, "vi"}, "handwriting-vi"},
          {{kHandwritingFeatureId, "zh"}, "handwriting-zh"},

          // Text-To-Speech.
          {{kTtsFeatureId, "en-us"}, "tts-en-us"},
          {{kTtsFeatureId, "es-es"}, "tts-es-es"},
          {{kTtsFeatureId, "es-us"}, "tts-es-us"},
          {{kTtsFeatureId, "fr-fr"}, "tts-fr-fr"},
          {{kTtsFeatureId, "hi-in"}, "tts-hi-in"},
          {{kTtsFeatureId, "ja-jp"}, "tts-ja-jp"},
          {{kTtsFeatureId, "nl-nl"}, "tts-nl-nl"},
          {{kTtsFeatureId, "pt-br"}, "tts-pt-br"},
          {{kTtsFeatureId, "sv-se"}, "tts-sv-se"},
      });

  return *all_dlc_ids;
}

const base::flat_map<std::string, std::string>& GetAllBasePackDlcIds() {
  // Map of all features and corresponding Base Pack DLC IDs.
  static const base::NoDestructor<base::flat_map<std::string, std::string>>
      all_dlc_ids({
          {kHandwritingFeatureId, "handwriting-base"},
      });

  return *all_dlc_ids;
}

// Finds the ID of the DLC corresponding to the given spec.
// Returns the DLC ID if the DLC exists or absl::nullopt otherwise.
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
                          const DlcserviceClient::InstallResult& dlc_result) {
  PackResult result;
  result.operation_error = dlc_result.error;

  const bool success = dlc_result.error == dlcservice::kErrorNone;
  if (success) {
    result.pack_state = PackResult::INSTALLED;
    result.path = dlc_result.root_path;
  } else {
    result.pack_state = PackResult::UNKNOWN;
  }

  base::UmaHistogramBoolean("ChromeOS.LanguagePacks.InstallComplete.Success",
                            success);

  std::move(callback).Run(result);
}

void OnUninstallDlcComplete(OnUninstallCompleteCallback callback,
                            const std::string& err) {
  PackResult result;
  result.operation_error = err;

  const bool success = err == dlcservice::kErrorNone;
  if (success) {
    result.pack_state = PackResult::NOT_INSTALLED;
  } else {
    result.pack_state = PackResult::UNKNOWN;
  }

  base::UmaHistogramBoolean("ChromeOS.LanguagePacks.UninstallComplete.Success",
                            success);

  std::move(callback).Run(result);
}

void OnGetDlcState(GetPackStateCallback callback,
                   const std::string& err,
                   const dlcservice::DlcState& dlc_state) {
  PackResult result;
  if (err == dlcservice::kErrorNone) {
    result = ConvertDlcStateToPackResult(dlc_state);
  } else {
    result.pack_state = PackResult::UNKNOWN;
  }

  result.operation_error = err;

  std::move(callback).Run(result);
}

// This function returns the enum value of a feature ID that matches the
// corresponding value in the UMA Histogram enum.
FeatureIdsEnum GetFeatureIdValueForUma(const std::string& feature_id) {
  if (feature_id == kHandwritingFeatureId)
    return FeatureIdsEnum::kHandwriting;
  if (feature_id == kTtsFeatureId)
    return FeatureIdsEnum::kTts;

  // Default value of unknown.
  return FeatureIdsEnum::kUnknown;
}

}  // namespace

bool LanguagePackManager::IsPackAvailable(const std::string& feature_id,
                                          const std::string& locale) {
  // We search in the static list for the given Pack spec.
  const PackSpecPair spec(feature_id, locale);
  return base::Contains(GetAllLanguagePackDlcIds(), spec);
}

void LanguagePackManager::InstallPack(const std::string& feature_id,
                                      const std::string& locale,
                                      OnInstallCompleteCallback callback) {
  const absl::optional<std::string> dlc_id =
      GetDlcIdForLanguagePack(feature_id, locale);

  // If the given Language Pack doesn't exist, run callback and don't reach the
  // DLC Service.
  if (!dlc_id) {
    std::move(callback).Run(CreateInvalidDlcPackResult());
    return;
  }

  InstallDlc(*dlc_id,
             base::BindOnce(&OnInstallDlcComplete, std::move(callback)));
}

void LanguagePackManager::GetPackState(const std::string& feature_id,
                                       const std::string& locale,
                                       GetPackStateCallback callback) {
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
      *dlc_id, base::BindOnce(&OnGetDlcState, std::move(callback)));
}

void LanguagePackManager::RemovePack(const std::string& feature_id,
                                     const std::string& locale,
                                     OnUninstallCompleteCallback callback) {
  const absl::optional<std::string> dlc_id =
      GetDlcIdForLanguagePack(feature_id, locale);

  // If the given Language Pack doesn't exist, run callback and don't reach the
  // DLC Service.
  if (!dlc_id) {
    std::move(callback).Run(CreateInvalidDlcPackResult());
    return;
  }

  DlcserviceClient::Get()->Uninstall(
      *dlc_id, base::BindOnce(&OnUninstallDlcComplete, std::move(callback)));
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

  InstallDlc(*dlc_id,
             base::BindOnce(&OnInstallDlcComplete, std::move(callback)));
}

void LanguagePackManager::AddObserver(Observer* const observer) {
  observers_.AddObserver(observer);
}

void LanguagePackManager::RemoveObserver(Observer* const observer) {
  observers_.RemoveObserver(observer);
}

void LanguagePackManager::NotifyPackStateChanged(
    const dlcservice::DlcState& dlc_state) {
  PackResult result = ConvertDlcStateToPackResult(dlc_state);
  for (Observer& observer : observers_)
    observer.OnPackStateChanged(result);
}

void LanguagePackManager::OnDlcStateChanged(
    const dlcservice::DlcState& dlc_state) {
  // As of now, we only have Handwriting as a client.
  // We will check the full list once we have more than one DLC.
  if (dlc_state.id() != kHandwritingFeatureId)
    return;

  NotifyPackStateChanged(dlc_state);
}

LanguagePackManager::LanguagePackManager() = default;
LanguagePackManager::~LanguagePackManager() = default;

void LanguagePackManager::Initialize() {
  DlcserviceClient::Get()->AddObserver(this);
}

void LanguagePackManager::ResetForTesting() {
  observers_.Clear();
}

// static
LanguagePackManager* LanguagePackManager::GetInstance() {
  static base::NoDestructor<LanguagePackManager> instance;
  return instance.get();
}

}  // namespace ash::language_packs
