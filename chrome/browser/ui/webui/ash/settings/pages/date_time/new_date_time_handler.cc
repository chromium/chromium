// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/date_time/new_date_time_handler.h"

#include <vector>

#include "ash/public/cpp/child_accounts/parent_access_controller.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/child_accounts/parent_access_code/parent_access_service.h"
#include "chrome/browser/ash/system/timezone_util.h"
#include "chrome/browser/ui/webui/ash/set_time/set_time_dialog.h"
#include "chrome/browser/ui/webui/ash/settings/pages/date_time/mojom/date_time_handler.mojom.h"
#include "components/user_manager/user_manager.h"

namespace ash::settings {

NewDateTimeHandler::NewDateTimeHandler(
    mojo::PendingReceiver<date_time::mojom::PageHandler> receiver,
    mojo::PendingRemote<date_time::mojom::Page> page,
    content::WebUI* web_ui,
    Profile* profile)
    : web_ui_(web_ui),
      profile_(profile),
      page_(std::move(page)),
      receiver_(this, std::move(receiver)) {
  SystemClockClient* system_clock_client = SystemClockClient::Get();
  scoped_observation_.Observe(system_clock_client);
  SystemClockCanSetTimeChanged(system_clock_client->CanSetTime());
}

NewDateTimeHandler::~NewDateTimeHandler() {
  scoped_observation_.Reset();
}

void NewDateTimeHandler::ShowParentAccessForTimezone() {
  DCHECK(user_manager::UserManager::Get()->GetActiveUser()->IsChild());

  if (!parent_access::ParentAccessService::IsApprovalRequired(
          SupervisedAction::kUpdateTimezone)) {
    OnParentAccessValidation(true);
    return;
  }

  ParentAccessController::Get()->ShowWidget(
      user_manager::UserManager::Get()->GetActiveUser()->GetAccountId(),
      base::BindOnce(&NewDateTimeHandler::OnParentAccessValidation,
                     weak_ptr_factory_.GetWeakPtr()),
      SupervisedAction::kUpdateTimezone, false /* extra_dimmer */,
      base::Time::Now());
}

void NewDateTimeHandler::GetTimezones(GetTimezonesCallback callback) {
  std::vector<std::vector<std::string>> timezones;
  for (auto& timezone_values : system::GetTimezoneList()) {
    std::vector<std::string> timezone;
    for (auto& value : timezone_values.GetList()) {
      timezone.emplace_back(value.GetString());
    }
    timezones.emplace_back(std::move(timezone));
  }
  std::move(callback).Run(std::move(timezones));
}

void NewDateTimeHandler::ShowSetDateTimeUI() {
  // Make sure the clock status hasn't changed since the button was clicked.
  if (!SystemClockClient::Get()->CanSetTime()) {
    return;
  }
  SetTimeDialog::ShowDialog(
      web_ui_->GetWebContents()->GetTopLevelNativeWindow());
}

void NewDateTimeHandler::SystemClockCanSetTimeChanged(bool can_set_time) {
  page_->OnSystemClockCanSetTimeChanged(can_set_time);
}

void NewDateTimeHandler::OnParentAccessValidation(bool success) {
  if (success) {
    page_->OnParentAccessValidationComplete(true);
  }
}

}  // namespace ash::settings
