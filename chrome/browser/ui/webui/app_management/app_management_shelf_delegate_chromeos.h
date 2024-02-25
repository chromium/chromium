// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_APP_MANAGEMENT_APP_MANAGEMENT_SHELF_DELEGATE_CHROMEOS_H_
#define CHROME_BROWSER_UI_WEBUI_APP_MANAGEMENT_APP_MANAGEMENT_SHELF_DELEGATE_CHROMEOS_H_

#include <memory>

#include "ash/public/cpp/shelf_model_observer.h"
#include "base/memory/raw_ptr.h"
#include "ui/webui/resources/cr_components/app_management/app_management.mojom.h"

class AppManagementPageHandlerChromeOs;
class ShelfControllerHelper;
class Profile;

// This is a helper class used by the AppManagementPageHandlerBase to manage
// shelf-related functionality, which is only meaningful when running Chrome OS.
// It observes the ShelfModel, and notifies the AppManagementPageHandlerBase
// when apps are pinned or unpinned.
class AppManagementShelfDelegate : public ash::ShelfModelObserver {
 public:
  explicit AppManagementShelfDelegate(
      AppManagementPageHandlerChromeOs* page_handler,
      Profile* profile);

  AppManagementShelfDelegate(const AppManagementShelfDelegate&) = delete;
  AppManagementShelfDelegate& operator=(const AppManagementShelfDelegate&) =
      delete;

  ~AppManagementShelfDelegate() override;

  bool IsPinned(const std::string& app_id);
  void SetPinned(const std::string& app_id, bool pinned);

  bool IsPolicyPinned(const std::string& app_id) const;

 private:
  // ash::ShelfModelObserver:
  void ShelfItemAdded(int index) override;
  void ShelfItemRemoved(int index, const ash::ShelfItem& old_item) override;
  void ShelfItemChanged(int index, const ash::ShelfItem& old_item) override;

  raw_ptr<AppManagementPageHandlerChromeOs> page_handler_;
  std::unique_ptr<ShelfControllerHelper> shelf_controller_helper_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_APP_MANAGEMENT_APP_MANAGEMENT_SHELF_DELEGATE_CHROMEOS_H_
