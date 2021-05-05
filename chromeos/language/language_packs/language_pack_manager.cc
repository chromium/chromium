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
#include "chromeos/dbus/dlcservice/dlcservice.pb.h"
#include "chromeos/dbus/dlcservice/dlcservice_client.h"

namespace chromeos {
namespace language_packs {
namespace {

const base::flat_map<PackSpecPair, std::string>& GetAllDlcIds() {
  // Create the map of all DLCs and corresponding IDs.
  // Whenever a new DLC is created, it needs to be added here.
  // Clients of Language Packs don't need to know the IDs.
  static const base::NoDestructor<base::flat_map<PackSpecPair, std::string>>
      all_dlc_ids({{{kHandwritingFeatureId, "en"}, "handwriting-en-dlc"}});

  return *all_dlc_ids;
}

void OnInstallDlcComplete(
    OnInstallCompleteCallback callback,
    const chromeos::DlcserviceClient::InstallResult& dlc_result) {
  PackResult result;
  result.operation_error = dlc_result.error;

  if (dlc_result.error == dlcservice::kErrorNone) {
    result.pack_state = PackResult::INSTALLED;
    result.path = dlc_result.root_path;
  } else {
    result.pack_state = PackResult::UNKNOWN;
  }

  std::move(callback).Run(result);
}

void OnGetDlcState(GetPackStateCallback callback,
                   const std::string& err,
                   const dlcservice::DlcState& dlc_state) {
  PackResult result;
  result.operation_error = err;

  if (err == dlcservice::kErrorNone) {
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

  } else {
    result.pack_state = PackResult::UNKNOWN;
  }

  std::move(callback).Run(result);
}

void OnUninstallDlcComplete(OnUninstallCompleteCallback callback,
                            const std::string& err) {
  PackResult result;
  result.operation_error = err;

  if (err == dlcservice::kErrorNone) {
    result.pack_state = PackResult::NOT_INSTALLED;
  } else {
    result.pack_state = PackResult::UNKNOWN;
  }

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

  chromeos::DlcserviceClient::Get()->Install(
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

  chromeos::DlcserviceClient::Get()->GetDlcState(
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

  chromeos::DlcserviceClient::Get()->Uninstall(
      dlc_id, base::BindOnce(&OnUninstallDlcComplete, std::move(callback)));
}

LanguagePackManager::LanguagePackManager() = default;
LanguagePackManager::~LanguagePackManager() = default;

// static
LanguagePackManager* LanguagePackManager::GetInstance() {
  static base::NoDestructor<LanguagePackManager> instance;
  return instance.get();
}

}  // namespace language_packs
}  // namespace chromeos
