// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_ui_manager.h"

namespace web_app {

WebAppUiManager::WebAppUiManager() = default;

WebAppUiManager::~WebAppUiManager() {
  for (WebAppUiManagerObserver& observer : observers_)
    observer.OnWebAppUiManagerDestroyed();
}

void WebAppUiManager::AddObserver(WebAppUiManagerObserver* observer) {
  observers_.AddObserver(observer);
}

void WebAppUiManager::RemoveObserver(WebAppUiManagerObserver* observer) {
  observers_.RemoveObserver(observer);
}

void WebAppUiManager::NotifyReadyToCommitNavigation(
    const AppId& app_id,
    content::NavigationHandle* navigation_handle) {
  for (WebAppUiManagerObserver& observer : observers_)
    observer.OnReadyToCommitNavigation(app_id, navigation_handle);
}

}  // namespace web_app
