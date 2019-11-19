// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_registrar.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "chrome/browser/web_applications/web_app.h"

namespace web_app {

WebAppRegistrar::WebAppRegistrar(Profile* profile) : AppRegistrar(profile) {}

WebAppRegistrar::~WebAppRegistrar() = default;

const WebApp* WebAppRegistrar::GetAppById(const AppId& app_id) const {
  auto it = registry_.find(app_id);
  return it == registry_.end() ? nullptr : it->second.get();
}

bool WebAppRegistrar::IsInstalled(const AppId& app_id) const {
  return GetAppById(app_id) != nullptr;
}

bool WebAppRegistrar::IsLocallyInstalled(const AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->is_locally_installed() : false;
}

bool WebAppRegistrar::WasExternalAppUninstalledByUser(
    const AppId& app_id) const {
  NOTIMPLEMENTED();
  return false;
}

bool WebAppRegistrar::WasInstalledByUser(const AppId& app_id) const {
  // TODO(crbug.com/1012171): Implement.
  NOTIMPLEMENTED();
  return true;
}

int WebAppRegistrar::CountUserInstalledApps() const {
  NOTIMPLEMENTED();

  int num_user_installed = 0;
  for (const WebApp& app : AllApps()) {
    if (!app.is_locally_installed())
      continue;

    // TODO(crbug.com/1012171): Exclude if not installed by user.

    ++num_user_installed;
  }
  return num_user_installed;
}

std::string WebAppRegistrar::GetAppShortName(const AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->name() : std::string();
}

std::string WebAppRegistrar::GetAppDescription(const AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->description() : std::string();
}

base::Optional<SkColor> WebAppRegistrar::GetAppThemeColor(
    const AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->theme_color() : base::nullopt;
}

const GURL& WebAppRegistrar::GetAppLaunchURL(const AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->launch_url() : GURL::EmptyGURL();
}

base::Optional<GURL> WebAppRegistrar::GetAppScope(const AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  if (!web_app)
    return base::nullopt;

  // TODO(crbug.com/910016): Treat shortcuts as PWAs.
  // Shortcuts on the WebApp system have empty scopes, while the implementation
  // of IsShortcutApp just checks if the scope is |base::nullopt|, so make sure
  // we return |base::nullopt| rather than an empty scope.
  if (web_app->scope().is_empty())
    return base::nullopt;

  return web_app->scope();
}

DisplayMode WebAppRegistrar::GetAppDisplayMode(const AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->display_mode() : DisplayMode::kUndefined;
}

DisplayMode WebAppRegistrar::GetAppUserDisplayMode(const AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->user_display_mode() : DisplayMode::kUndefined;
}

std::vector<AppId> WebAppRegistrar::GetAppIds() const {
  std::vector<AppId> app_ids;
  app_ids.reserve(registry_.size());

  for (const WebApp& app : AllApps())
    app_ids.push_back(app.app_id());

  return app_ids;
}

WebAppRegistrar::AppSet::AppSet(const WebAppRegistrar* registrar)
    : registrar_(registrar)
#if DCHECK_IS_ON()
      ,
      mutations_count_(registrar->mutations_count_)
#endif
{
}

WebAppRegistrar::AppSet::~AppSet() {
#if DCHECK_IS_ON()
  DCHECK_EQ(mutations_count_, registrar_->mutations_count_);
#endif
}

WebAppRegistrar::AppSet::iterator WebAppRegistrar::AppSet::begin() {
  return iterator(registrar_->registry_.begin());
}

WebAppRegistrar::AppSet::iterator WebAppRegistrar::AppSet::end() {
  return iterator(registrar_->registry_.end());
}

WebAppRegistrar::AppSet::const_iterator WebAppRegistrar::AppSet::begin() const {
  return const_iterator(registrar_->registry_.begin());
}

WebAppRegistrar::AppSet::const_iterator WebAppRegistrar::AppSet::end() const {
  return const_iterator(registrar_->registry_.end());
}

const WebAppRegistrar::AppSet WebAppRegistrar::AllApps() const {
  return AppSet(this);
}

void WebAppRegistrar::SetRegistry(Registry&& registry) {
  registry_ = std::move(registry);
}

void WebAppRegistrar::CountMutation() {
#if DCHECK_IS_ON()
  ++mutations_count_;
#endif
}

WebAppRegistrarMutable::WebAppRegistrarMutable(Profile* profile)
    : WebAppRegistrar(profile) {}

WebAppRegistrarMutable::~WebAppRegistrarMutable() = default;

void WebAppRegistrarMutable::InitRegistry(Registry&& registry) {
  DCHECK(is_empty());
  SetRegistry(std::move(registry));
}

WebApp* WebAppRegistrarMutable::GetAppByIdMutable(const AppId& app_id) {
  return const_cast<WebApp*>(GetAppById(app_id));
}

WebAppRegistrar::AppSet WebAppRegistrarMutable::AllAppsMutable() {
  return AppSet(this);
}

bool IsRegistryEqual(const Registry& registry, const Registry& registry2) {
  if (registry.size() != registry2.size())
    return false;

  for (auto& kv : registry) {
    const WebApp* web_app = kv.second.get();
    const WebApp* web_app2 = registry2.at(web_app->app_id()).get();
    if (*web_app != *web_app2)
      return false;
  }

  return true;
}

}  // namespace web_app
