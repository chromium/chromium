// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/desks_client.h"

#include <memory>

#include "ash/public/cpp/desk_template.h"
#include "ash/public/cpp/desks_helper.h"
#include "ash/public/cpp/session/session_controller.h"
#include "ash/public/cpp/session/session_observer.h"
#include "base/bind.h"
#include "base/guid.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/desk_template_app_launch_handler.h"
#include "components/desks_storage/core/local_desk_data_manager.h"
#include "extensions/common/constants.h"

namespace {
DesksClient* g_desks_client_instance = nullptr;

constexpr char kWindowCountHistogramName[] = "Ash.DeskTemplate.WindowCount";
constexpr char kTabCountHistogramName[] = "Ash.DeskTemplate.TabCount";
constexpr char kWindowAndTabCountHistogramName[] =
    "Ash.DeskTemplate.WindowAndTabCount";
constexpr char kLaunchFromTemplateHistogramName[] =
    "Ash.DeskTemplate.LaunchFromTemplate";

// Returns true if |profile| is a supported profile in desk template feature.
bool IsSupportedProfile(Profile* profile) {
  // Public users & guest users are not supported.
  return profile && profile->IsRegularProfile();
}

}  // namespace

DesksClient::DesksClient() : desks_helper_(ash::DesksHelper::Get()) {
  DCHECK(!g_desks_client_instance);
  g_desks_client_instance = this;
  ash::SessionController::Get()->AddObserver(this);
}

DesksClient::~DesksClient() {
  DCHECK_EQ(this, g_desks_client_instance);
  g_desks_client_instance = nullptr;
  ash::SessionController::Get()->RemoveObserver(this);
}

// static
DesksClient* DesksClient::Get() {
  return g_desks_client_instance;
}

void DesksClient::OnActiveUserSessionChanged(const AccountId& account_id) {
  Profile* profile =
      ash::ProfileHelper::Get()->GetProfileByAccountId(account_id);
  if (profile == active_profile_ || !IsSupportedProfile(profile))
    return;

  active_profile_ = profile;
  DCHECK(active_profile_);
  storage_manager_ = std::make_unique<desks_storage::LocalDeskDataManager>(
      active_profile_->GetPath());
}

void DesksClient::CaptureActiveDeskAndSaveTemplate(
    CaptureActiveDeskAndSaveTemplateCallback callback) {
  if (!active_profile_) {
    std::move(callback).Run(/*success=*/false, /*desk_template=*/nullptr);
    return;
  }

  std::unique_ptr<ash::DeskTemplate> desk_template =
      desks_helper_->CaptureActiveDeskAsTemplate();
  RecordWindowAndTabCount(desk_template.get());
  storage_manager_->AddOrUpdateEntry(
      desk_template->Clone(),
      base::BindOnce(&DesksClient::OnCaptureActiveDeskAndSaveTemplate,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(desk_template)));
}

void DesksClient::UpdateDeskTemplate(const std::string& template_uuid,
                                     const std::u16string& template_name,
                                     UpdateDeskTemplateCallback callback) {
  if (!active_profile_) {
    std::move(callback).Run(/*success=*/false);
    return;
  }

  storage_manager_->GetEntryByUUID(
      template_uuid, base::BindOnce(&DesksClient::OnGetTemplateToBeUpdated,
                                    weak_ptr_factory_.GetWeakPtr(),
                                    template_name, std::move(callback)));
}

void DesksClient::DeleteDeskTemplate(const std::string& template_uuid,
                                     DeleteDeskTemplateCallback callback) {
  if (!active_profile_) {
    std::move(callback).Run(/*success=*/false);
    return;
  }

  storage_manager_->DeleteEntry(
      template_uuid,
      base::BindOnce(&DesksClient::OnDeleteDeskTemplate,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void DesksClient::GetDeskTemplates(GetDeskTemplatesCallback callback) {
  if (!active_profile_) {
    std::move(callback).Run(/*success=*/false, /*desk_templates=*/{});
    return;
  }

  storage_manager_->GetAllEntries(
      base::BindOnce(&DesksClient::OnGetAllTemplates,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void DesksClient::LaunchDeskTemplate(const std::string& template_uuid,
                                     LaunchDeskTemplateCallback callback) {
  if (!active_profile_) {
    std::move(callback).Run(/*success=*/false);
    return;
  }

  MaybeCreateAppLaunchHandler();
  if (!app_launch_handler_) {
    std::move(callback).Run(/*success=*/false);
    return;
  }

  // TODO: verify this method works in tests when reading from storage.
  if (launch_template_for_test_) {
    OnGetTemplateForDeskLaunch(
        std::move(callback),
        desks_storage::DeskModel::GetEntryByUuidStatus::kOk,
        launch_template_for_test_->Clone());
    return;
  }

  storage_manager_->GetEntryByUUID(
      template_uuid,
      base::BindOnce(&DesksClient::OnGetTemplateForDeskLaunch,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void DesksClient::MaybeCreateAppLaunchHandler() {
  if (app_launch_handler_ &&
      app_launch_handler_->profile() == active_profile_) {
    return;
  }

  DCHECK(active_profile_);
  app_launch_handler_ =
      std::make_unique<DeskTemplateAppLaunchHandler>(active_profile_);
}

void DesksClient::RecordWindowAndTabCount(ash::DeskTemplate* desk_template) {
  full_restore::RestoreData* restore_data = desk_template->desk_restore_data();
  DCHECK(restore_data);

  int window_count = 0;
  int tab_count = 0;
  int total_count = 0;

  const auto& launch_list = restore_data->app_id_to_launch_list();
  for (const auto& iter : launch_list) {
    // Since apps aren't guaranteed to have the url field set up correctly, this
    // is necessary to ensure things are not double-counted.
    if (iter.first != extension_misc::kChromeAppId) {
      ++window_count;
      ++total_count;
      continue;
    }

    for (const auto& window_iter : iter.second) {
      absl::optional<std::vector<GURL>> urls = window_iter.second->urls;
      if (!urls || urls->empty())
        continue;

      ++window_count;
      tab_count += urls->size();
      total_count += urls->size();
    }
  }

  base::UmaHistogramCounts100(kWindowCountHistogramName, window_count);
  base::UmaHistogramCounts100(kTabCountHistogramName, tab_count);
  base::UmaHistogramCounts100(kWindowAndTabCountHistogramName, total_count);
}

void DesksClient::RecordLaunchFromTemplate() {
  base::UmaHistogramBoolean(kLaunchFromTemplateHistogramName, true);
}

void DesksClient::OnGetTemplateForDeskLaunch(
    LaunchDeskTemplateCallback callback,
    desks_storage::DeskModel::GetEntryByUuidStatus status,
    std::unique_ptr<ash::DeskTemplate> entry) {
  if (status != desks_storage::DeskModel::GetEntryByUuidStatus::kOk) {
    std::move(callback).Run(/*success=*/false);
    return;
  }

  // Launch the windows as specified in the template to a new desk.
  desks_helper_->CreateAndActivateNewDeskForTemplate(
      entry->template_name(),
      base::BindOnce(&DesksClient::OnCreateAndActivateNewDesk,
                     weak_ptr_factory_.GetWeakPtr(), std::move(entry),
                     std::move(callback)));
}

void DesksClient::OnCreateAndActivateNewDesk(
    std::unique_ptr<ash::DeskTemplate> desk_template,
    LaunchDeskTemplateCallback callback,
    bool on_create_activate_success) {
  if (!on_create_activate_success) {
    std::move(callback).Run(/*success=*/false);
    return;
  }

  DCHECK(desk_template);
  full_restore::RestoreData* restore_data = desk_template->desk_restore_data();
  if (!restore_data) {
    std::move(callback).Run(/*success=*/false);
    return;
  }

  DCHECK(app_launch_handler_);
  app_launch_handler_->SetRestoreDataAndLaunch(restore_data->Clone());
  std::move(callback).Run(/*success=*/true);

  RecordLaunchFromTemplate();
}

void DesksClient::OnCaptureActiveDeskAndSaveTemplate(
    DesksClient::CaptureActiveDeskAndSaveTemplateCallback callback,
    std::unique_ptr<ash::DeskTemplate> desk_template,
    desks_storage::DeskModel::AddOrUpdateEntryStatus status) {
  std::move(callback).Run(
      status == desks_storage::DeskModel::AddOrUpdateEntryStatus::kOk,
      std::move(desk_template));
}

void DesksClient::OnDeleteDeskTemplate(
    DesksClient::DeleteDeskTemplateCallback callback,
    desks_storage::DeskModel::DeleteEntryStatus status) {
  std::move(callback).Run(status ==
                          desks_storage::DeskModel::DeleteEntryStatus::kOk);
}

void DesksClient::OnUpdateDeskTemplate(
    DesksClient::UpdateDeskTemplateCallback callback,
    desks_storage::DeskModel::AddOrUpdateEntryStatus status) {
  std::move(callback).Run(
      status == desks_storage::DeskModel::AddOrUpdateEntryStatus::kOk);
}

void DesksClient::OnGetTemplateToBeUpdated(
    const std::u16string& template_name,
    DesksClient::UpdateDeskTemplateCallback callback,
    desks_storage::DeskModel::GetEntryByUuidStatus status,
    std::unique_ptr<ash::DeskTemplate> entry) {
  if (status != desks_storage::DeskModel::GetEntryByUuidStatus::kOk) {
    std::move(callback).Run(false);
    return;
  }

  entry->set_template_name(template_name);
  storage_manager_->AddOrUpdateEntry(
      std::move(entry),
      base::BindOnce(&DesksClient::OnUpdateDeskTemplate,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void DesksClient::OnGetAllTemplates(
    DesksClient::GetDeskTemplatesCallback callback,
    desks_storage::DeskModel::GetAllEntriesStatus status,
    std::vector<ash::DeskTemplate*> entries) {
  std::move(callback).Run(
      status == desks_storage::DeskModel::GetAllEntriesStatus::kOk, entries);
}
