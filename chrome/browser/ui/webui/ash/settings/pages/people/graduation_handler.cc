// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/people/graduation_handler.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_pages.h"

namespace ash::settings {

GraduationHandler::GraduationHandler(Profile* profile) : profile_(profile) {}

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

}  // namespace ash::settings
