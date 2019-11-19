// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_APP_SERVICE_APP_SERVICE_APP_MODEL_BUILDER_H_
#define CHROME_BROWSER_UI_APP_LIST_APP_SERVICE_APP_SERVICE_APP_MODEL_BUILDER_H_

#include "base/macros.h"
#include "chrome/browser/ui/app_list/app_list_model_builder.h"
#include "chrome/services/app_service/public/cpp/app_registry_cache.h"

class AppListControllerDelegate;

class AppServiceAppModelBuilder : public AppListModelBuilder,
                                  public apps::AppRegistryCache::Observer {
 public:
  explicit AppServiceAppModelBuilder(AppListControllerDelegate* controller);

  ~AppServiceAppModelBuilder() override;

 private:
  class CrostiniFolderObserver;

  // AppListModelBuilder overrides:
  void BuildModel() override;

  // apps::AppRegistryCache::Observer overrides:
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

  std::unique_ptr<AppListModelUpdaterObserver> crostini_folder_observer_;

  DISALLOW_COPY_AND_ASSIGN(AppServiceAppModelBuilder);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_APP_SERVICE_APP_SERVICE_APP_MODEL_BUILDER_H_
