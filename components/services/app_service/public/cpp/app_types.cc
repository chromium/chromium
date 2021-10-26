// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/app_types.h"

namespace apps {

App::App(AppType app_type, const std::string& app_id)
    : app_type(app_type), app_id(app_id) {}

App::~App() = default;

std::unique_ptr<App> App::Clone() const {
  std::unique_ptr<App> app = std::make_unique<App>(app_type, app_id);

  app->readiness = readiness;
  app->name = name;
  app->short_name = short_name;
  app->publisher_id = publisher_id;
  app->description = description;
  app->version = version;

  if (icon_key.has_value()) {
    app->icon_key = apps::IconKey(icon_key->timeline, icon_key->resource_id,
                                  icon_key->icon_effects);
  }
  return app;
}

}  // namespace apps
