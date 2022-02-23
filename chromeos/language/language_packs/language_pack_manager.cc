// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/language/language_packs/language_pack_manager.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "chromeos/dbus/dlcservice/dlcservice.pb.h"
#include "chromeos/dbus/dlcservice/dlcservice_client.h"

namespace chromeos::language_packs {
namespace {

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

const base::flat_map<PackSpecPair, std::string>& GetAllDlcIds() {
  // Map of all DLCs and corresponding IDs.
  // It's a map from PackSpecPair to DLC ID. The pair is <feature id, locale>.
  // Whenever a new DLC is created, it needs to be added here.
  // Clients of Language Packs don't need to know the IDs.
  // Note: English is not included because it's still using LongForm.
  static const base::NoDestructor<base::flat_map<PackSpecPair, std::string>>
      all_dlc_ids({
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
      });

  return *all_dlc_ids;
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

}  // namespace

bool LanguagePackManager::IsPackAvailable(const std::string& pack_id,
                                          const std::string& locale) {
  // We search in the static list for the given Pack spec.
  const PackSpecPair spec(pack_id, locale);
  return base::Contains(GetAllDlcIds(), spec);
}

bool LanguagePackManager::GetDlcId(const std::string& pack_id,
                                   const std::string& locale,
                                   std::string* const dlc_id) {
  // We search in the static list for the given Pack spec.
  const PackSpecPair spec(pack_id, locale);
  const auto it = GetAllDlcIds().find(spec);

  if (it == GetAllDlcIds().end()) {
    return false;
  }

  *dlc_id = it->second;
  return true;
}

void LanguagePackManager::InstallPack(const std::string& pack_id,
                                      const std::string& locale,
                                      OnInstallCompleteCallback callback) {
  std::string dlc_id;
  const bool found = GetDlcId(pack_id, locale, &dlc_id);

  // If the given Language Pack doesn't exist, run callback and don't reach the
  // DLC Service.
  if (!found) {
    PackResult result;
    result.operation_error = dlcservice::kErrorInvalidDlc;
    result.pack_state = PackResult::WRONG_ID;
    std::move(callback).Run(result);
    return;
  }

  DlcserviceClient::Get()->Install(
      dlc_id, base::BindOnce(&OnInstallDlcComplete, std::move(callback)),
      base::DoNothing());
}

void LanguagePackManager::GetPackState(const std::string& pack_id,
                                       const std::string& locale,
                                       GetPackStateCallback callback) {
  std::string dlc_id;
  const bool found = GetDlcId(pack_id, locale, &dlc_id);

  // If the given Language Pack doesn't exist, run callback and don't reach the
  // DLC Service.
  if (!found) {
    PackResult result;
    result.operation_error = dlcservice::kErrorInvalidDlc;
    result.pack_state = PackResult::WRONG_ID;
    std::move(callback).Run(result);
    return;
  }

  DlcserviceClient::Get()->GetDlcState(
      dlc_id, base::BindOnce(&OnGetDlcState, std::move(callback)));
}

void LanguagePackManager::RemovePack(const std::string& pack_id,
                                     const std::string& locale,
                                     OnUninstallCompleteCallback callback) {
  std::string dlc_id;
  const bool found = GetDlcId(pack_id, locale, &dlc_id);

  // If the given Language Pack doesn't exist, run callback and don't reach the
  // DLC Service.
  if (!found) {
    PackResult result;
    result.operation_error = dlcservice::kErrorInvalidDlc;
    result.pack_state = PackResult::WRONG_ID;
    std::move(callback).Run(result);
    return;
  }

  DlcserviceClient::Get()->Uninstall(
      dlc_id, base::BindOnce(&OnUninstallDlcComplete, std::move(callback)));
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

}  // namespace chromeos::language_packs
