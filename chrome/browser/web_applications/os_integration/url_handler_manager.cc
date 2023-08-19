// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/url_handler_manager.h"

#include <utility>

#include "chrome/browser/profiles/profile.h"

namespace web_app {

UrlHandlerManager::UrlHandlerManager(Profile* profile)
    : profile_(profile),
      association_manager_(std::make_unique<WebAppOriginAssociationManager>()) {
}

UrlHandlerManager::~UrlHandlerManager() = default;

void UrlHandlerManager::SetAssociationManagerForTesting(
    std::unique_ptr<WebAppOriginAssociationManager> manager) {
  association_manager_ = std::move(manager);
}

}  // namespace web_app
