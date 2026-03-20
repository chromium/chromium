// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/android_app_description_tools.h"

#include <utility>

#include "base/check.h"
#include "base/containers/extend.h"
#include "components/payments/core/android_app_description.h"

namespace payments {

void SplitPotentiallyMultipleActivities(
    std::unique_ptr<AndroidAppDescription> app,
    std::vector<std::unique_ptr<AndroidAppDescription>>* destination) {
  CHECK(app);
  CHECK(destination);
  if (app->activities.empty())
    return;

  if (app->activities.size() == 1) {
    destination->push_back(std::move(app));
    return;
  }

  std::vector<std::unique_ptr<AndroidAppDescription>> split_apps;
  split_apps.reserve(app->activities.size() - 1);

  for (size_t i = 1; i < app->activities.size(); ++i) {
    auto single_activity_app = std::make_unique<AndroidAppDescription>();
    single_activity_app->package = app->package;
    single_activity_app->service_names = app->service_names;
    single_activity_app->activities.push_back(std::move(app->activities[i]));
    split_apps.push_back(std::move(single_activity_app));
  }

  app->activities.resize(1);
  // Append the original app then newly created apps.
  destination->reserve(destination->size() + 1 + split_apps.size());
  destination->push_back(std::move(app));
  base::Extend(*destination, std::move(split_apps));
}

}  // namespace payments
