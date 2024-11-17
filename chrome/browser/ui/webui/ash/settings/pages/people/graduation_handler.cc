// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/people/graduation_handler.h"

#include "ash/constants/ash_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_pages.h"

namespace ash::settings {

GraduationHandler::GraduationHandler(Profile* profile) : profile_(profile) {
  if (features::IsGraduationEnabled()) {
    ash::graduation::GraduationManager* manager =
        ash::graduation::GraduationManager::Get();
    CHECK(manager);
    manager->AddObserver(this);
  }
}

GraduationHandler::~GraduationHandler() = default;

void GraduationHandler::BindInterface(
    mojo::PendingReceiver<graduation::mojom::GraduationHandler> receiver) {
  if (receiver_.is_bound()) {
    receiver_.reset();
  }
  receiver_.Bind(std::move(receiver));
}

void GraduationHandler::LaunchGraduationApp() {
  chrome::ShowGraduationApp(profile_);
}

void GraduationHandler::AddObserver(
    mojo::PendingRemote<graduation::mojom::GraduationObserver> observer) {
  observer_list_.Add(std::move(observer));
}

void GraduationHandler::OnGraduationAppUpdate(bool is_enabled) {
  for (const auto& observer : observer_list_) {
    observer->OnGraduationAppUpdated(is_enabled);
  }
}

}  // namespace ash::settings
