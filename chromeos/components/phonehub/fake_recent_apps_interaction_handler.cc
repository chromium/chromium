// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/fake_recent_apps_interaction_handler.h"
#include "chromeos/components/phonehub/notification.h"

namespace chromeos {
namespace phonehub {

FakeRecentAppsInteractionHandler::FakeRecentAppsInteractionHandler() = default;

FakeRecentAppsInteractionHandler::~FakeRecentAppsInteractionHandler() = default;

void FakeRecentAppsInteractionHandler::NotifyRecentAppClicked(
    const Notification::AppMetadata& app_metadata) {
  handled_recent_apps_count_++;
}

void FakeRecentAppsInteractionHandler::AddRecentAppClickObserver(
    RecentAppClickObserver* observer) {
  recent_app_click_observer_count_++;
}

void FakeRecentAppsInteractionHandler::RemoveRecentAppClickObserver(
    RecentAppClickObserver* observer) {
  recent_app_click_observer_count_--;
}

}  // namespace phonehub
}  // namespace chromeos
