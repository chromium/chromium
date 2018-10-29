// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_registrar.h"

#include "base/logging.h"
#include "chrome/browser/web_applications/web_app.h"

namespace web_app {

WebAppRegistrar::WebAppRegistrar() {}

WebAppRegistrar::~WebAppRegistrar() = default;

void WebAppRegistrar::RegisterApp(std::unique_ptr<WebApp> web_app) {
  const auto app_id = web_app->app_id();
  DCHECK(!app_id.empty());
  DCHECK(!GetAppById(app_id));

  registry_.emplace(std::make_pair(app_id, std::move(web_app)));
}

std::unique_ptr<WebApp> WebAppRegistrar::UnregisterApp(const AppId& app_id) {
  DCHECK(!app_id.empty());

  auto kv = registry_.find(app_id);
  DCHECK(kv != registry_.end());

  auto web_app = std::move(kv->second);
  registry_.erase(kv);
  return web_app;
}

WebApp* WebAppRegistrar::GetAppById(const AppId& app_id) {
  auto kv = registry_.find(app_id);
  return kv == registry_.end() ? nullptr : kv->second.get();
}

void WebAppRegistrar::UnregisterAll() {
  registry_.clear();
}

}  // namespace web_app
