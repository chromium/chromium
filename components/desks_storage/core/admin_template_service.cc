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
#include "components/desks_storage/core/desk_template_conversion.h"
#include "components/policy/policy_constants.h"

namespace desks_storage {

namespace {

// Reads from `pref_service` and updates the model with the contents within.
void UpdateModelWithPolicy(desks_storage::DeskModel* desk_model,
                           const PrefService* pref_service) {
  // Query for the desired preference.
  if (pref_service == nullptr) {
    return;
  }

  const PrefService::Preference* app_launch_automation_preference =
      pref_service->FindPreference(ash::prefs::kAppLaunchAutomation);

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

  if (desk_model == nullptr) {
    return;
  } else if (!desk_model->IsReady()) {
    LOG(WARNING) << "Attempted to update model before model was ready.";
    return;
  }

  // If templates exist that aren't in the current policy we should delete them.
  std::vector<base::Uuid> desk_uuids_to_delete = desk_model->GetAllEntryUuids();

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
    auto get_entry_result = desk_model->GetEntryByUUID(desk_template->uuid());
    auto entry_status = get_entry_result.status;

    // If this template exists in the current policy then don't delete it after
    // updating the locally stored policy. Note: this call is a noop if the
    // template in question is a new template.
    if (entry_status == desks_storage::DeskModel::GetEntryByUuidStatus::kOk ||
        entry_status ==
            desks_storage::DeskModel::GetEntryByUuidStatus::kNotFound) {
      base::Erase(desk_uuids_to_delete, desk_template->uuid());

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
    desk_model->AddOrUpdateEntry(std::move(desk_template), base::DoNothing());
  }

  // Remove all templates that aren't in the policy.  If the policy is empty
  // then this will remove all admin templates from the device.
  for (auto uuid : desk_uuids_to_delete) {
    desk_model->DeleteEntry(uuid, base::DoNothing());
  }
}

}  // namespace

AdminTemplateService::AdminTemplateService(
    const base::FilePath& user_data_dir_path,
    const AccountId& account_id,
    PrefService* pref_service)
    : pref_service_(pref_service) {
  data_manager_ = std::make_unique<LocalDeskDataManager>(
      user_data_dir_path, account_id,
      LocalDeskDataManager::StorageLocation::kAppLaunchAutomationDir);

  data_manager_->AddObserver(this);

  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      ash::prefs::kAppLaunchAutomation,
      base::BindRepeating(&UpdateModelWithPolicy, data_manager_.get(),
                          pref_service_));
}

AdminTemplateService::~AdminTemplateService() {
  data_manager_->RemoveObserver(this);
}

AdminTemplateModel* AdminTemplateService::GetAdminModel() {
  return data_manager_.get();
}

DeskModel* AdminTemplateService::GetFullDeskModel() {
  return data_manager_.get();
}

bool AdminTemplateService::IsReady() {
  return data_manager_->IsReady();
}

void AdminTemplateService::DeskModelLoaded() {
  UpdateModelWithPolicy(data_manager_.get(), pref_service_);
}

// Noops, we're not interested in these events.
void AdminTemplateService::OnDeskModelDestroying() {}
void AdminTemplateService::EntriesAddedOrUpdatedRemotely(
    const std::vector<const ash::DeskTemplate*>& new_entries) {}
void AdminTemplateService::EntriesRemovedRemotely(
    const std::vector<base::Uuid>& uuids) {}

}  // namespace desks_storage
