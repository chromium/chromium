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
  // TODO(b/223250258): We currently only have 2 languages. Add all remaining
  // languages once the infra issue is fixed.
  static const base::NoDestructor<base::flat_map<PackSpecPair, std::string>>
      all_dlc_ids({
          {{kHandwritingFeatureId, "es"}, "handwriting-es"},
          {{kHandwritingFeatureId, "ja"}, "handwriting-ja"},
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

bool LanguagePackManager::IsPackAvailable(const std::string& feature_id,
                                          const std::string& locale) {
  // We search in the static list for the given Pack spec.
  const PackSpecPair spec(feature_id, locale);
  return base::Contains(GetAllDlcIds(), spec);
}

bool LanguagePackManager::GetDlcId(const std::string& feature_id,
                                   const std::string& locale,
                                   std::string* const dlc_id) {
  // We search in the static list for the given Pack spec.
  const PackSpecPair spec(feature_id, locale);
  const auto it = GetAllDlcIds().find(spec);

  if (it == GetAllDlcIds().end()) {
    return false;
  }

  *dlc_id = it->second;
  return true;
}

void LanguagePackManager::InstallPack(const std::string& feature_id,
                                      const std::string& locale,
                                      OnInstallCompleteCallback callback) {
  std::string dlc_id;
  const bool found = GetDlcId(feature_id, locale, &dlc_id);

  // If the given Language Pack doesn't exist, run callback and don't reach the
  // DLC Service.
  if (!found) {
    PackResult result;
    result.operation_error = dlcservice::kErrorInvalidDlc;
    result.pack_state = PackResult::WRONG_ID;
    std::move(callback).Run(result);
    return;
  }

  dlcservice::InstallRequest install_request;
  install_request.set_id(dlc_id);
  DlcserviceClient::Get()->Install(
      install_request,
      base::BindOnce(&OnInstallDlcComplete, std::move(callback)),
      base::DoNothing());
}

void LanguagePackManager::GetPackState(const std::string& feature_id,
                                       const std::string& locale,
                                       GetPackStateCallback callback) {
  std::string dlc_id;
  const bool found = GetDlcId(feature_id, locale, &dlc_id);

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

void LanguagePackManager::RemovePack(const std::string& feature_id,
                                     const std::string& locale,
                                     OnUninstallCompleteCallback callback) {
  std::string dlc_id;
  const bool found = GetDlcId(feature_id, locale, &dlc_id);

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
