// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/android_app_description_tools.h"

#include <utility>

#include "base/check.h"
#include "components/payments/core/android_app_description.h"

namespace payments {

void SplitPotentiallyMultipleActivities(
    std::unique_ptr<AndroidAppDescription> app,
    std::vector<std::unique_ptr<AndroidAppDescription>>* destination) {
  DCHECK(destination);
  if (app->activities.empty())
    return;
  destination->emplace_back(std::move(app));
  for (size_t i = 1; i < destination->front()->activities.size(); ++i) {
    auto single_activity_app = std::make_unique<AndroidAppDescription>();
    single_activity_app->package = destination->front()->package;
    single_activity_app->service_names = destination->front()->service_names;
    single_activity_app->activities.emplace_back(
        std::move(destination->front()->activities[i]));
    destination->emplace_back(std::move(single_activity_app));
  }
  destination->front()->activities.resize(1);
}

}  // namespace payments
