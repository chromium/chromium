// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_FAKE_WEB_APP_ORIGIN_ASSOCIATION_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_FAKE_WEB_APP_ORIGIN_ASSOCIATION_MANAGER_H_

#include <map>

#include "chrome/browser/web_applications/web_app_origin_association_manager.h"

namespace web_app {

// Fake implementation of WebAppOriginAssociationManager.
class FakeWebAppOriginAssociationManager
    : public WebAppOriginAssociationManager {
 public:
  FakeWebAppOriginAssociationManager();
  ~FakeWebAppOriginAssociationManager() override;

  // Sends back preset data.
  // Sends back |url_handlers| as is if pass_through_ is set.
  void GetWebAppOriginAssociations(
      const GURL& manifest_url,
      apps::UrlHandlers url_handlers,
      OnDidGetWebAppOriginAssociations callback) override;

  void SetData(std::map<apps::UrlHandlerInfo, apps::UrlHandlerInfo> data);

  void set_pass_through(bool value) { pass_through_ = value; }

 private:
  // Maps a url handler to the corresponding result to send back in the
  // callback.
  std::map<apps::UrlHandlerInfo, apps::UrlHandlerInfo> data_;
  bool pass_through_ = false;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_FAKE_WEB_APP_ORIGIN_ASSOCIATION_MANAGER_H_
