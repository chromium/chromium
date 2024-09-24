// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_ui_state_manager.h"

namespace web_app {

WebAppUiStateManager::WebAppUiStateManager() = default;

WebAppUiStateManager::~WebAppUiStateManager() = default;

void WebAppUiStateManager::NotifyWebAppWindowDidEnterForeground(
    const webapps::AppId app_id) {}

void WebAppUiStateManager::NotifyWebAppWindowWillEnterBackground(
    const webapps::AppId app_id) {}

void WebAppUiStateManager::NotifyWebAppWindowDidBecomeActive(
    const webapps::AppId app_id) {}

void WebAppUiStateManager::NotifyWebAppWindowDidBecomeInactive(
    const webapps::AppId app_id) {}

}  // namespace web_app
