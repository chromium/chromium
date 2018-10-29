// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_REGISTRAR_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_REGISTRAR_H_

#include <map>
#include <memory>

#include "base/macros.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"

namespace web_app {

class WebApp;

class WebAppRegistrar {
 public:
  WebAppRegistrar();
  ~WebAppRegistrar();

  void RegisterApp(std::unique_ptr<WebApp> web_app);
  std::unique_ptr<WebApp> UnregisterApp(const AppId& app_id);

  WebApp* GetAppById(const AppId& app_id);

  using Registry = std::map<AppId, std::unique_ptr<WebApp>>;
  const Registry& registry() const { return registry_; }

  bool is_empty() const { return registry_.empty(); }

  // Clears registry.
  void UnregisterAll();

 private:
  Registry registry_;

  DISALLOW_COPY_AND_ASSIGN(WebAppRegistrar);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_REGISTRAR_H_
