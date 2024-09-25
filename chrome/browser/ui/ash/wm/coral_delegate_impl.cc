// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/wm/coral_delegate_impl.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/desks/desks_templates_app_launch_handler.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/services/coral/public/mojom/coral_service.mojom.h"
#include "components/app_constants/constants.h"
#include "components/app_restore/restore_data.h"
#include "components/user_manager/user_manager.h"

namespace {

std::unique_ptr<app_restore::RestoreData> CoralGroupToRestoreData(
    coral::mojom::GroupPtr group) {
  auto restore_data = std::make_unique<app_restore::RestoreData>();
  std::vector<GURL> tab_urls;
  for (const coral::mojom::EntityKeyPtr& entity : group->entities) {
    if (entity->is_tab_url()) {
      tab_urls.push_back(entity->get_tab_url());
    }
  }

  if (!tab_urls.empty()) {
    auto& launch_list =
        restore_data
            ->mutable_app_id_to_launch_list()[app_constants::kChromeAppId];
    // All tabs go into the same window.
    auto& app_restore_data = launch_list[/*window_id=*/0];
    app_restore_data = std::make_unique<app_restore::AppRestoreData>();
    app_restore_data->browser_extra_info.urls = std::move(tab_urls);
  }

  // TODO(http::/b/365839465): Handle apps.

  return restore_data;
}

}  // namespace

CoralDelegateImpl::CoralDelegateImpl() = default;

CoralDelegateImpl::~CoralDelegateImpl() = default;

void CoralDelegateImpl::LaunchPostLoginGroup(coral::mojom::GroupPtr group) {
  if (app_launch_handler_) {
    return;
  }

  auto* active_user = user_manager::UserManager::Get()->GetActiveUser();
  if (!active_user) {
    return;
  }

  Profile* active_profile = Profile::FromBrowserContext(
      ash::BrowserContextHelper::Get()->GetBrowserContextByUser(active_user));
  if (!active_profile) {
    return;
  }

  app_launch_handler_ = std::make_unique<DesksTemplatesAppLaunchHandler>(
      active_profile, DesksTemplatesAppLaunchHandler::Type::kCoral);
  app_launch_handler_->LaunchCoralGroup(
      CoralGroupToRestoreData(std::move(group)),
      DesksTemplatesAppLaunchHandler::GetNextLaunchId());
}

void CoralDelegateImpl::OpenNewDeskWithGroup(coral::mojom::GroupPtr group) {}

void CoralDelegateImpl::CreateSavedDeskFromGroup(coral::mojom::GroupPtr group) {
}
