// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/wm/coral_delegate_impl.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/desks/desks_templates_app_launch_handler.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/user_manager/user_manager.h"

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
      DesksTemplatesAppLaunchHandler::GetNextLaunchId());
}

void CoralDelegateImpl::OpenNewDeskWithGroup(coral::mojom::GroupPtr group) {}

void CoralDelegateImpl::CreateSavedDeskFromGroup(coral::mojom::GroupPtr group) {
}
