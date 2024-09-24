// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_UI_STATE_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_UI_STATE_MANAGER_H_

#include "base/containers/flat_map.h"
#include "components/webapps/common/web_app_id.h"

namespace web_app {

class WebAppUiStateManager {
 public:
  WebAppUiStateManager();
  WebAppUiStateManager& operator=(const WebAppUiStateManager&) = delete;
  WebAppUiStateManager(const WebAppUiStateManager&) = delete;
  ~WebAppUiStateManager();

  // Events forwarded from WebAppTabHelper
  void NotifyWebAppWindowDidEnterForeground(const webapps::AppId);
  void NotifyWebAppWindowWillEnterBackground(const webapps::AppId);

  // Events forwarded from WebAppWindowController
  void NotifyWebAppWindowDidBecomeActive(const webapps::AppId);
  void NotifyWebAppWindowDidBecomeInactive(const webapps::AppId);

 private:
  struct WebAppUiState {};
  base::flat_map<webapps::AppId, WebAppUiState> ids_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_UI_STATE_MANAGER_H_
