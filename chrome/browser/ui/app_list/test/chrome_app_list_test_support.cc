// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/test/chrome_app_list_test_support.h"

#include <memory>
#include <string>

#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/app_list/app_list_client_impl.h"
#include "chrome/browser/ui/app_list/app_list_model_updater.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/ui/app_list/chrome_app_list_item.h"
#include "components/crx_file/id_util.h"

namespace test {

namespace {

// Create the icon image for the app-item with |id|.
// TODO(mukai): consolidate the implementation with
// ash/app_list/test/app_list_test_model.cc.
gfx::ImageSkia CreateImageSkia(int id) {
  const int size = ash::AppListConfig::instance().grid_icon_dimension();
  SkBitmap bitmap;
  bitmap.allocN32Pixels(size, size);
  bitmap.eraseARGB(255, 255 * ((id >> 2) % 2), 255 * ((id >> 1) % 2),
                   255 * (id % 2));
  return gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
}

}  // namespace

AppListModelUpdater* GetModelUpdater(AppListClientImpl* client) {
  return app_list::AppListSyncableServiceFactory::GetForProfile(
             client->GetCurrentAppListProfile())
      ->GetModelUpdater();
}

AppListClientImpl* GetAppListClient() {
  AppListClientImpl* client = AppListClientImpl::GetInstance();
  client->UpdateProfile();
  return client;
}

void PopulateDummyAppListItems(int n) {
  AppListClientImpl* client = GetAppListClient();
  Profile* profile = client->GetCurrentAppListProfile();
  AppListModelUpdater* model_updater = GetModelUpdater(client);
  for (int i = 0; i < n; ++i) {
    const std::string app_name = base::StringPrintf("app %d", i);
    const std::string app_id = crx_file::id_util::GenerateId(app_name);
    auto item =
        std::make_unique<ChromeAppListItem>(profile, app_id, model_updater);
    auto metadata = std::make_unique<ash::AppListItemMetadata>();
    metadata->id = app_id;
    metadata->name = app_name;
    metadata->short_name = app_name;
    metadata->icon = CreateImageSkia(i);
    item->SetMetadata(std::move(metadata));
    model_updater->AddItem(std::move(item));
  }
  // Wait for the AddItem mojo calls to be handled by Ash. Note that
  // FlushMojoForTesting() isn't working well here.
  // TODO(mukai): remove this once we eliminate the mojo for app-list.
  base::RunLoop().RunUntilIdle();
}

}  // namespace test
