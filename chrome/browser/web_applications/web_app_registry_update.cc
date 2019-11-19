// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_registry_update.h"

#include "base/bind_helpers.h"
#include "base/stl_util.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"

namespace web_app {

RegistryUpdateData::RegistryUpdateData() = default;

RegistryUpdateData::~RegistryUpdateData() = default;

bool RegistryUpdateData::IsEmpty() const {
  return apps_to_create.empty() && apps_to_delete.empty() &&
         apps_to_update.empty();
}

WebAppRegistryUpdate::WebAppRegistryUpdate(const WebAppRegistrar* registrar,
                                           util::PassKey<WebAppSyncBridge>)
    : registrar_(registrar) {
  DCHECK(registrar_);
  update_data_ = std::make_unique<RegistryUpdateData>();
}

WebAppRegistryUpdate::~WebAppRegistryUpdate() = default;

void WebAppRegistryUpdate::CreateApp(std::unique_ptr<WebApp> web_app) {
  DCHECK(update_data_);
  DCHECK(!web_app->app_id().empty());
  DCHECK(!registrar_->GetAppById(web_app->app_id()));
  DCHECK(!base::Contains(update_data_->apps_to_create, web_app));

  update_data_->apps_to_create.push_back(std::move(web_app));
}

void WebAppRegistryUpdate::DeleteApp(const AppId& app_id) {
  DCHECK(update_data_);
  DCHECK(!app_id.empty());
  DCHECK(registrar_->GetAppById(app_id));
  DCHECK(!base::Contains(update_data_->apps_to_delete, app_id));

  update_data_->apps_to_delete.push_back(app_id);
}

WebApp* WebAppRegistryUpdate::UpdateApp(const AppId& app_id) {
  DCHECK(update_data_);
  const WebApp* original_app = registrar_->GetAppById(app_id);
  if (!original_app)
    return nullptr;

  for (auto& app_to_update : update_data_->apps_to_update) {
    if (app_to_update->app_id() == app_id)
      return app_to_update.get();
  }

  // Make a copy on write.
  auto app_copy = std::make_unique<WebApp>(*original_app);
  WebApp* app_copy_ptr = app_copy.get();
  update_data_->apps_to_update.push_back(std::move(app_copy));

  return app_copy_ptr;
}

std::unique_ptr<RegistryUpdateData> WebAppRegistryUpdate::TakeUpdateData() {
  return std::move(update_data_);
}

ScopedRegistryUpdate::ScopedRegistryUpdate(WebAppSyncBridge* sync_bridge)
    : update_(sync_bridge->BeginUpdate()), sync_bridge_(sync_bridge) {}

ScopedRegistryUpdate::~ScopedRegistryUpdate() {
  sync_bridge_->CommitUpdate(std::move(update_), base::DoNothing());
}

}  // namespace web_app
