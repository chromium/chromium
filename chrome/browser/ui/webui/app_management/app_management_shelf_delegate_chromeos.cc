// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/app_management/app_management_shelf_delegate_chromeos.h"

#include "ash/public/cpp/shelf_item.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/shelf_types.h"
#include "base/containers/contains.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/shelf/app_shortcut_shelf_item_controller.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller_util.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_prefs.h"
#include "chrome/browser/ui/ash/shelf/shelf_controller_helper.h"
#include "chrome/browser/ui/webui/app_management/app_management_page_handler_chromeos.h"

AppManagementShelfDelegate::AppManagementShelfDelegate(
    AppManagementPageHandlerChromeOs* page_handler,
    Profile* profile)
    : page_handler_(page_handler),
      shelf_controller_helper_(
          std::make_unique<ShelfControllerHelper>(profile)) {
  auto* shelf_controller = ChromeShelfController::instance();
  if (!shelf_controller) {
    return;
  }

  auto* shelf_model = shelf_controller->shelf_model();
  if (!shelf_model) {
    return;
  }

  shelf_model->AddObserver(this);
}

AppManagementShelfDelegate::~AppManagementShelfDelegate() {
  auto* shelf_controller = ChromeShelfController::instance();
  if (!shelf_controller) {
    return;
  }

  auto* shelf_model = shelf_controller->shelf_model();
  if (!shelf_model) {
    return;
  }

  shelf_model->RemoveObserver(this);
}

bool AppManagementShelfDelegate::IsPinned(const std::string& app_id) {
  auto* shelf_controller = ChromeShelfController::instance();
  if (!shelf_controller) {
    return false;
  }
  return shelf_controller->IsAppPinned(app_id);
}

bool AppManagementShelfDelegate::IsPolicyPinned(
    const std::string& app_id) const {
  auto* shelf_controller = ChromeShelfController::instance();

  if (!shelf_controller) {
    return false;
  }

  auto* shelf_item = shelf_controller->GetItem(ash::ShelfID(app_id));
  if (shelf_item) {
    return shelf_item->pinned_by_policy;
  }
  // The app doesn't exist on the shelf - check launcher prefs instead.
  std::vector<std::string> policy_pinned_apps =
      ChromeShelfPrefs::GetAppsPinnedByPolicy(
          shelf_controller_helper_->profile());
  return base::Contains(policy_pinned_apps, app_id);
}

void AppManagementShelfDelegate::SetPinned(const std::string& app_id,
                                           bool pinned) {
  auto* shelf_controller = ChromeShelfController::instance();
  if (!shelf_controller) {
    return;
  }

  if (pinned) {
    PinAppWithIDToShelf(app_id);
  } else {
    UnpinAppWithIDFromShelf(app_id);
  }
}

void AppManagementShelfDelegate::ShelfItemAdded(int index) {
  auto* shelf_controller = ChromeShelfController::instance();
  if (!shelf_controller) {
    return;
  }

  auto* shelf_model = shelf_controller->shelf_model();
  if (!shelf_model) {
    return;
  }

  if (index >= shelf_model->item_count()) {
    // index out of bounds.
    return;
  }

  const std::string& app_id = shelf_model->items()[index].id.app_id;
  bool is_pinned = shelf_controller->IsAppPinned(app_id);

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
  auto* shelf_controller = ChromeShelfController::instance();
  if (!shelf_controller) {
    return;
  }

  auto* shelf_model = shelf_controller->shelf_model();
  if (!shelf_model) {
    return;
  }

  if (index >= shelf_model->item_count()) {
    // index out of bounds.
    return;
  }

  const std::string& app_id = shelf_model->items()[index].id.app_id;
  bool is_pinned = shelf_controller->IsAppPinned(app_id);

  page_handler_->OnPinnedChanged(app_id, is_pinned);
}
