// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_registry_update.h"

#include "base/containers/contains.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/types/pass_key.h"
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
                                           base::PassKey<WebAppSyncBridge>)
    : registrar_(registrar) {
  DCHECK(registrar_);
  update_data_ = std::make_unique<RegistryUpdateData>();
}

WebAppRegistryUpdate::~WebAppRegistryUpdate() = default;

void WebAppRegistryUpdate::CreateApp(std::unique_ptr<WebApp> web_app) {
  DCHECK(update_data_);
  CHECK(web_app->manifest_id().is_valid());
  DCHECK(!web_app->app_id().empty());
  DCHECK(!registrar_->GetAppById(web_app->app_id()));
  DCHECK(!base::Contains(update_data_->apps_to_create, web_app));

  update_data_->apps_to_create.push_back(std::move(web_app));
}

void WebAppRegistryUpdate::DeleteApp(const webapps::AppId& app_id) {
  DCHECK(update_data_);
  DCHECK(!app_id.empty());
  DCHECK(registrar_->GetAppById(app_id));
  DCHECK(!base::Contains(update_data_->apps_to_delete, app_id));

  update_data_->apps_to_delete.push_back(app_id);
}

WebApp* WebAppRegistryUpdate::UpdateApp(const webapps::AppId& app_id) {
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

std::unique_ptr<RegistryUpdateData> WebAppRegistryUpdate::TakeUpdateData(
    base::PassKey<WebAppSyncBridge> pass_key) {
  return std::move(update_data_);
}

ScopedRegistryUpdate::ScopedRegistryUpdate(
    base::PassKey<WebAppSyncBridge>,
    std::unique_ptr<WebAppRegistryUpdate> update,
    base::OnceCallback<void(std::unique_ptr<WebAppRegistryUpdate>)>
        commit_update)
    : update_(std::move(update)), commit_update_(std::move(commit_update)) {}

ScopedRegistryUpdate::ScopedRegistryUpdate(ScopedRegistryUpdate&&) noexcept =
    default;

ScopedRegistryUpdate::~ScopedRegistryUpdate() {
  if (update_) {
    std::move(commit_update_).Run(std::move(update_));
  }
}

}  // namespace web_app
