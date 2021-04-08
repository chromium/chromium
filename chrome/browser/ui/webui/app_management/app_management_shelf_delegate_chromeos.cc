// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/app_management/app_management_shelf_delegate_chromeos.h"

#include <algorithm>

#include "ash/public/cpp/shelf_item.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/shelf_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/chrome_launcher_prefs.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller_util.h"
#include "chrome/browser/ui/ash/launcher/launcher_controller_helper.h"
#include "chrome/browser/ui/webui/app_management/app_management_page_handler.h"
#include "components/services/app_service/public/mojom/types.mojom.h"

using apps::mojom::OptionalBool;

AppManagementShelfDelegate::AppManagementShelfDelegate(
    AppManagementPageHandler* page_handler,
    Profile* profile)
    : page_handler_(page_handler) {
  auto* launcher_controller = ChromeLauncherController::instance();
  if (!launcher_controller) {
    return;
  }

  auto* shelf_model = launcher_controller->shelf_model();
  if (!shelf_model) {
    return;
  }
  launcher_controller_helper_ = new LauncherControllerHelper(profile);

  shelf_model->AddObserver(this);
}

AppManagementShelfDelegate::~AppManagementShelfDelegate() {
  auto* launcher_controller = ChromeLauncherController::instance();
  if (!launcher_controller) {
    return;
  }

  auto* shelf_model = launcher_controller->shelf_model();
  if (!shelf_model) {
    return;
  }

  shelf_model->RemoveObserver(this);
}

bool AppManagementShelfDelegate::IsPinned(const std::string& app_id) {
  auto* launcher_controller = ChromeLauncherController::instance();
  if (!launcher_controller) {
    return false;
  }
  return launcher_controller->IsAppPinned(app_id);
}

bool AppManagementShelfDelegate::IsPolicyPinned(
    const std::string& app_id) const {
  auto* launcher_controller = ChromeLauncherController::instance();

  if (!launcher_controller) {
    return false;
  }

  auto* shelf_item = launcher_controller->GetItem(ash::ShelfID(app_id));
  if (shelf_item) {
    return shelf_item->pinned_by_policy;
  }
  // The app doesn't exist on the shelf - check launcher prefs instead.
  std::vector<std::string> policy_pinned_apps =
      GetAppsPinnedByPolicy(launcher_controller_helper_);
  return std::any_of(policy_pinned_apps.begin(), policy_pinned_apps.end(),
                     [app_id](std::string app) { return app_id == app; });
}

void AppManagementShelfDelegate::SetPinned(const std::string& app_id,
                                           OptionalBool pinned) {
  auto* launcher_controller = ChromeLauncherController::instance();

  if (!launcher_controller) {
    return;
  }

  if (pinned == OptionalBool::kTrue) {
    launcher_controller->PinAppWithID(app_id);
  } else if (pinned == OptionalBool::kFalse) {
    launcher_controller->UnpinAppWithID(app_id);
  } else {
    NOTREACHED();
  }
}

void AppManagementShelfDelegate::ShelfItemAdded(int index) {
  auto* launcher_controller = ChromeLauncherController::instance();
  if (!launcher_controller) {
    return;
  }

  auto* shelf_model = launcher_controller->shelf_model();
  if (!shelf_model) {
    return;
  }

  if (index >= shelf_model->item_count()) {
    // index out of bounds.
    return;
  }

  const std::string& app_id = shelf_model->items()[index].id.app_id;
  bool is_pinned = launcher_controller->IsAppPinned(app_id);

  page_handler_->OnPinnedChanged(app_id, is_pinned);
}

void AppManagementShelfDelegate::ShelfItemRemoved(
    int index,
    const ash::ShelfItem& old_item) {
  // If the app has been removed from the shelf model, it is not longer pinned.
  page_handler_->OnPinnedChanged(old_item.id.app_id, false);
}

void AppManagementShelfDelegate::ShelfItemChanged(
    int index,
    const ash::ShelfItem& old_item) {
  auto* launcher_controller = ChromeLauncherController::instance();
  if (!launcher_controller) {
    return;
  }

  auto* shelf_model = launcher_controller->shelf_model();
  if (!shelf_model) {
    return;
  }

  if (index >= shelf_model->item_count()) {
    // index out of bounds.
    return;
  }

  const std::string& app_id = shelf_model->items()[index].id.app_id;
  bool is_pinned = launcher_controller->IsAppPinned(app_id);

  page_handler_->OnPinnedChanged(app_id, is_pinned);
}
