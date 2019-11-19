// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_APP_MANAGEMENT_APP_MANAGEMENT_SHELF_DELEGATE_CHROMEOS_H_
#define CHROME_BROWSER_UI_WEBUI_APP_MANAGEMENT_APP_MANAGEMENT_SHELF_DELEGATE_CHROMEOS_H_

#include "ash/public/cpp/shelf_model_observer.h"
#include "base/macros.h"
#include "chrome/browser/ui/webui/app_management/app_management.mojom.h"

class AppManagementPageHandler;

// This is a helper class used by the AppManagementPageHandler to manage
// shelf-related functionality, which is only meaningful when running Chrome OS.
// It observes the ShelfModel, and notifies the AppManagementPageHandler when
// apps are pinned or unpinned.
class AppManagementShelfDelegate : public ash::ShelfModelObserver {
 public:
  explicit AppManagementShelfDelegate(AppManagementPageHandler* page_handler);
  ~AppManagementShelfDelegate() override;

  bool IsPinned(const std::string& app_id);
  void SetPinned(const std::string& app_id, apps::mojom::OptionalBool pinned);

  bool IsPolicyPinned(const std::string& app_id) const;

 private:
  // ash::ShelfModelObserver:
  void ShelfItemAdded(int index) override;
  void ShelfItemRemoved(int index, const ash::ShelfItem& old_item) override;
  void ShelfItemChanged(int index, const ash::ShelfItem& old_item) override;

  AppManagementPageHandler* page_handler_;

  DISALLOW_COPY_AND_ASSIGN(AppManagementShelfDelegate);
};

#endif  // CHROME_BROWSER_UI_WEBUI_APP_MANAGEMENT_APP_MANAGEMENT_SHELF_DELEGATE_CHROMEOS_H_
