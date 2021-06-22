// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/desks_client.h"

#include "ash/public/cpp/desk_template.h"
#include "ash/public/cpp/desks_helper.h"
#include "base/bind.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"

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

std::unique_ptr<ash::DeskTemplate> DesksClient::CaptureActiveDeskAsTemplate() {
  return desks_helper_->CaptureActiveDeskAsTemplate();
}

void DesksClient::LaunchDeskTemplate(double template_uuid) {
  // TODO: Find the saved template associated with `template_uuid` from storage.
  if (!launch_template_for_test_ ||
      launch_template_for_test_->uuid() != template_uuid) {
    return;
  }

  // Launch the windows as specified in the template to a new desk.
  desks_helper_->CreateAndActivateNewDeskForTemplate(
      launch_template_for_test_->template_name(),
      base::BindOnce(&DesksClient::OnCreateAndActivateNewDesk,
                     weak_ptr_factory_.GetWeakPtr(),
                     launch_template_for_test_.get()));
}

void DesksClient::OnCreateAndActivateNewDesk(ash::DeskTemplate* desk_template,
                                             bool on_create_activate_success) {
  if (!on_create_activate_success)
    return;

  // TODO: Launch windows.
}
