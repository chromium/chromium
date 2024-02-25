// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_FAKE_URL_HANDLER_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_FAKE_URL_HANDLER_MANAGER_H_

#include "chrome/browser/web_applications/os_integration/url_handler_manager.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "components/webapps/common/web_app_id.h"

class Profile;

namespace web_app {

// Fake implementation of UrlHandlerManager.
class FakeUrlHandlerManager : public UrlHandlerManager {
 public:
  explicit FakeUrlHandlerManager(Profile* profile);
  ~FakeUrlHandlerManager() override;

  void RegisterUrlHandlers(const webapps::AppId& app_id,
                           ResultCallback callback) override;
  bool UnregisterUrlHandlers(const webapps::AppId& app_id) override;
  void UpdateUrlHandlers(
      const webapps::AppId& app_id,
      base::OnceCallback<void(bool success)> callback) override;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_FAKE_URL_HANDLER_MANAGER_H_
