// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/desks_storage/core/admin_template_service.h"

#include "ash/constants/ash_pref_names.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/app_constants/constants.h"
#include "components/desks_storage/core/desk_template_conversion.h"
#include "components/policy/policy_constants.h"
#include "components/services/app_service/public/cpp/app_types.h"

namespace desks_storage {

AdminTemplateService::AdminTemplateService(
    const base::FilePath& user_data_dir_path,
    const AccountId& account_id,
    PrefService* pref_service)
    : account_id_(account_id), pref_service_(pref_service) {
  data_manager_ = std::make_unique<LocalDeskDataManager>(
      user_data_dir_path, account_id,
      LocalDeskDataManager::StorageLocation::kAppLaunchAutomationDir);

  model_obs_.Observe(data_manager_.get());

  auto& apps_cache_wrapper = apps::AppRegistryCacheWrapper::Get();
  auto* apps_cache = apps_cache_wrapper.GetAppRegistryCache(account_id_);

  // If we don't have an apps cache then we observe the wrapper to
  // wait for it to be ready.
  if (apps_cache) {
    app_cache_obs_.Observe(apps_cache);
  } else {
    app_cache_wrapper_obs_.Observe(&apps_cache_wrapper);
  }

  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      ash::prefs::kAppLaunchAutomation,
      base::BindRepeating(&AdminTemplateService::UpdateModelWithPolicy,
                          weak_ptr_factory_.GetWeakPtr()));
}

AdminTemplateService::~AdminTemplateService() = default;

void AdminTemplateService::UpdateModelWithPolicy() {
  if (!IsReady()) {
    return;
  }

  if (pref_service_ == nullptr) {
    return;
  }

  // Query for the desired preference.
  const PrefService::Preference* app_launch_automation_preference =
      pref_service_->FindPreference(ash::prefs::kAppLaunchAutomation);

  if (app_launch_automation_preference == nullptr) {
    return;
  }

  const base::Value* pref_value = app_launch_automation_preference->GetValue();

  CHECK(pref_value != nullptr);

  if (pref_value->type() != base::Value::Type::LIST) {
    return;
  }

  std::vector<std::unique_ptr<ash::DeskTemplate>> desk_templates =
      desks_storage::desk_template_conversion::
          ParseAdminTemplatesFromPolicyValue(*pref_value);

  // If templates exist that aren't in the current policy we should delete them.
  std::set<base::Uuid> desk_uuids_to_delete = data_manager_->GetAllEntryUuids();

  for (auto& desk_template : desk_templates) {
    // Something went wrong when parsing the template
    if (desk_template == nullptr) {
      continue;
    }

    // Something has gone wrong if the field isn't a dict.
    if (desk_template->policy_definition().type() != base::Value::Type::DICT) {
      continue;
    }

    // Query model to determine if this entry exists already.
    auto get_entry_result =
        data_manager_->GetEntryByUUID(desk_template->uuid());
    auto entry_status = get_entry_result.status;

    // If this template exists in the current policy then don't delete it after
    // updating the locally stored policy. Note: this call is a noop if the
    // template in question is a new template.
    if (entry_status == desks_storage::DeskModel::GetEntryByUuidStatus::kOk ||
        entry_status ==
            desks_storage::DeskModel::GetEntryByUuidStatus::kNotFound) {
      desk_uuids_to_delete.erase(desk_template->uuid());

      // There was an error when retrieving the template, do nothing and delete
      // the template.
    } else {
      continue;
    }

    // If the policy template already exists in the model and has been unchanged
    // since the last policy update don't overwrite the data.  This will
    // preserve the user's window information for that template.
    if (entry_status == desks_storage::DeskModel::GetEntryByUuidStatus::kOk &&
        get_entry_result.entry->policy_definition() ==
            desk_template->policy_definition()) {
      continue;
    }

    // If the policy template exists in an updated form or is new then either
    // add it to the model or overwrite the existing definition.
    data_manager_->AddOrUpdateEntry(std::move(desk_template),
                                    base::DoNothing());
  }

  // Remove all templates that aren't in the policy.  If the policy is empty
  // then this will remove all admin templates from the device.
  for (auto uuid : desk_uuids_to_delete) {
    data_manager_->DeleteEntry(uuid, base::DoNothing());
  }
}

AdminTemplateModel* AdminTemplateService::GetAdminModel() {
  return data_manager_.get();
}

DeskModel* AdminTemplateService::GetFullDeskModel() {
  return data_manager_.get();
}

bool AdminTemplateService::IsReady() {
  CHECK(data_manager_);
  return data_manager_->IsReady() && is_cache_ready_;
}

void AdminTemplateService::DeskModelLoaded() {
  is_cache_ready_ = WillAppRegistryCacheResolveAppIds();
  UpdateModelWithPolicy();
}

void AdminTemplateService::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  // Disallow updating the model.  If this is happening we're likely going
  // to be deallocated soon as well.
  is_cache_ready_ = false;

  app_cache_obs_.Reset();
}

void AdminTemplateService::OnAppTypeInitialized(apps::AppType app_type) {
  // If the cache is already ready we don't need to update the model.
  if (is_cache_ready_) {
    return;
  }
  is_cache_ready_ = WillAppRegistryCacheResolveAppIds();

  // If we're here it means that we have a policy that needs to be parsed but
  // until this point the AppRegistryCache wasn't ready.
  UpdateModelWithPolicy();
}

void AdminTemplateService::OnAppRegistryCacheAdded(
    const AccountId& account_id) {
  if (account_id != account_id_ || app_cache_obs_.IsObserving()) {
    return;
  }

  auto* apps_cache =
      apps::AppRegistryCacheWrapper::Get().GetAppRegistryCache(account_id);
  app_cache_obs_.Observe(apps_cache);
}

bool AdminTemplateService::WillAppRegistryCacheResolveAppIds() {
  if (!app_cache_obs_.IsObserving()) {
    return false;
  }

  apps::AppRegistryCache* cache =
      apps::AppRegistryCacheWrapper::Get().GetAppRegistryCache(account_id_);
  CHECK(cache);

  const std::set<apps::AppType>& initialized_types =
      cache->InitializedAppTypes();

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  return initialized_types.contains(apps::AppType::kStandaloneBrowser);
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return initialized_types.contains(apps::AppType::kChromeApp);
#endif
}

}  // namespace desks_storage
