// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/desks_client.h"

#include "ash/public/cpp/desks_helper.h"
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
  const user_manager::User* primary_user =
      user_manager::UserManager::Get()->GetPrimaryUser();
  if (!primary_user)
    return nullptr;

  Profile* user_profile =
      ash::ProfileHelper::Get()->GetProfileByUser(primary_user);
  if (!user_profile)
    return nullptr;

  return desks_helper_->CaptureActiveDeskAsTemplate(user_profile->GetPath());
}
