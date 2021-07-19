// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/desks_client.h"

#include <memory>

#include "ash/public/cpp/desk_template.h"
#include "ash/public/cpp/desks_helper.h"
#include "base/bind.h"
#include "base/guid.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/desk_template_app_launch_handler.h"
#include "extensions/common/constants.h"

namespace {

DesksClient* g_desks_client_instance = nullptr;

constexpr char kWindowCountHistogramName[] = "Ash.DeskTemplate.WindowCount";
constexpr char kTabCountHistogramName[] = "Ash.DeskTemplate.TabCount";
constexpr char kWindowAndTabCountHistogramName[] =
    "Ash.DeskTemplate.WindowAndTabCount";
constexpr char kLaunchFromTemplateHistogramName[] =
    "Ash.DeskTemplate.LaunchFromTemplate";

}  // namespace

DesksClient::DesksClient() : desks_helper_(ash::DesksHelper::Get()) {
  DCHECK(!g_desks_client_instance);
  g_desks_client_instance = this;
}

DesksClient::~DesksClient() {
  DCHECK_EQ(this, g_desks_client_instance);
  g_desks_client_instance = nullptr;
}

// static
DesksClient* DesksClient::Get() {
  return g_desks_client_instance;
}

void DesksClient::CaptureActiveDeskAndSaveTemplate(
    CaptureActiveDeskAndSaveTemplateCallback callback) {
  std::unique_ptr<ash::DeskTemplate> desk_template =
      desks_helper_->CaptureActiveDeskAsTemplate();
  // TODO: Save it to storage.
  RecordWindowAndTabCount(desk_template.get());
  std::move(callback).Run(/*success=*/true, std::move(desk_template));
}

void DesksClient::UpdateDeskTemplate(const std::string& template_uuid,
                                     const std::u16string& template_name,
                                     UpdateDeskTemplateCallback callback) {}

void DesksClient::DeleteDeskTemplate(const std::string& template_uuid,
                                     DeleteDeskTemplateCallback callback) {}

void DesksClient::GetDeskTemplates(GetDeskTemplatesCallback callback) {}

void DesksClient::LaunchDeskTemplate(const std::string& template_uuid,
                                     LaunchDeskTemplateCallback callback) {
  MaybeCreateAppLaunchHandler();
  if (!app_launch_handler_) {
    std::move(callback).Run(/*success=*/false);
    return;
  }

  // TODO: Find the saved template associated with `template_uuid` from storage.
  if (!launch_template_for_test_ ||
      launch_template_for_test_->uuid() !=
          base::GUID::ParseLowercase(template_uuid)) {
    std::move(callback).Run(/*success=*/false);
    return;
  }

  // Launch the windows as specified in the template to a new desk.
  desks_helper_->CreateAndActivateNewDeskForTemplate(
      launch_template_for_test_->template_name(),
      base::BindOnce(&DesksClient::OnCreateAndActivateNewDesk,
                     weak_ptr_factory_.GetWeakPtr(),
                     launch_template_for_test_.get(), std::move(callback)));
}

void DesksClient::OnCreateAndActivateNewDesk(
    ash::DeskTemplate* desk_template,
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

void DesksClient::MaybeCreateAppLaunchHandler() {
  if (app_launch_handler_)
    return;

  // TODO(sammiequon): Handle multiple profile case.
  Profile* profile = ProfileManager::GetActiveUserProfile();
  DCHECK(!ash::ProfileHelper::IsSigninProfile(profile));
  if (profile) {
    app_launch_handler_ =
        std::make_unique<DeskTemplateAppLaunchHandler>(profile);
  }
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
