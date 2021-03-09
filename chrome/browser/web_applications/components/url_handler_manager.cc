// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/url_handler_manager.h"

#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/app_registrar.h"

namespace web_app {

UrlHandlerManager::UrlHandlerManager(Profile* profile)
    : profile_(profile),
      association_manager_(std::make_unique<WebAppOriginAssociationManager>()) {
}

UrlHandlerManager::~UrlHandlerManager() = default;

void UrlHandlerManager::SetSubsystems(AppRegistrar* const registrar) {
  registrar_ = registrar;
}

void UrlHandlerManager::SetAssociationManagerForTesting(
    std::unique_ptr<WebAppOriginAssociationManager> manager) {
  association_manager_ = std::move(manager);
}

}  // namespace web_app
