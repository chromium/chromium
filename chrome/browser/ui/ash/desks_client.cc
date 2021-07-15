// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/desks_client.h"

#include <memory>

#include "ash/public/cpp/desk_template.h"
#include "ash/public/cpp/desks_helper.h"
#include "base/bind.h"
#include "base/guid.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/desk_template_app_launch_handler.h"

namespace {

DesksClient* g_desks_client_instance = nullptr;

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
