// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/url_handler_manager.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/app_registrar.h"

namespace web_app {

UrlHandlerManager::UrlHandlerManager(Profile* profile) : profile_(profile) {}

UrlHandlerManager::~UrlHandlerManager() = default;

void UrlHandlerManager::SetSubsystems(AppRegistrar* const registrar) {
  registrar_ = registrar;
}

}  // namespace web_app
