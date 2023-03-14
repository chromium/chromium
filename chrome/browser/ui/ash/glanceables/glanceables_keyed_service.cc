// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/glanceables/glanceables_keyed_service.h"

#include <memory>

#include "ash/glanceables/glanceables_v2_controller.h"
#include "ash/shell.h"
#include "base/check.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/glanceables/glanceables_tasks_client_impl.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user.h"

namespace ash {

GlanceablesKeyedService::GlanceablesKeyedService(Profile* profile)
    : account_id_(BrowserContextHelper::Get()
                      ->GetUserByBrowserContext(profile)
                      ->GetAccountId()) {
  CreateClients();
}

GlanceablesKeyedService::~GlanceablesKeyedService() = default;

void GlanceablesKeyedService::Shutdown() {
  tasks_client_.reset();
  UpdateRegistrationInAsh();
}

void GlanceablesKeyedService::CreateClients() {
  tasks_client_ = std::make_unique<GlanceablesTasksClientImpl>();
  UpdateRegistrationInAsh();
}

void GlanceablesKeyedService::UpdateRegistrationInAsh() const {
  if (!Shell::HasInstance()) {
    return;
  }
  DCHECK(Shell::Get()->glanceables_v2_controller());
  Shell::Get()->glanceables_v2_controller()->UpdateClientsRegistration(
      account_id_, GlanceablesV2Controller::ClientsRegistration{
                       .tasks_client = tasks_client_.get()});
}

}  // namespace ash
