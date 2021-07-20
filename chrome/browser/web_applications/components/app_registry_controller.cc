// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/app_registry_controller.h"

namespace web_app {

AppRegistryController::AppRegistryController(Profile* profile)
    : profile_(profile) {}

AppRegistryController::~AppRegistryController() = default;

}  // namespace web_app
