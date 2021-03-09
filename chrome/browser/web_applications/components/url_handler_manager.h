// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_URL_HANDLER_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_URL_HANDLER_MANAGER_H_

#include <memory>

#include "base/callback.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/browser/web_applications/components/web_app_origin_association_manager.h"

class Profile;

namespace web_app {

class AppRegistrar;

// UrlHandlerManager allows different manager implementations: local state
// prefs, App Service, and OS-specific implementations to enable integration
// with system-wide URL handling APIs.
class UrlHandlerManager {
 public:
  UrlHandlerManager() = delete;
  explicit UrlHandlerManager(Profile* const profile);
  virtual ~UrlHandlerManager();

  UrlHandlerManager(const UrlHandlerManager&) = delete;
  UrlHandlerManager& operator=(const UrlHandlerManager&) = delete;

  void SetSubsystems(AppRegistrar* const registrar);

  // Returns true if registration succeeds, false otherwise.
  virtual void RegisterUrlHandlers(
      const AppId& app_id,
      base::OnceCallback<void(bool success)> callback) = 0;
  // Returns true if unregistration succeeds, false otherwise.
  virtual bool UnregisterUrlHandlers(const AppId& app_id) = 0;
  // Returns true if update succeeds, false otherwise.
  virtual void UpdateUrlHandlers(
      const AppId& app_id,
      base::OnceCallback<void(bool success)> callback) = 0;

  void SetAssociationManagerForTesting(
      std::unique_ptr<WebAppOriginAssociationManager> manager);

 protected:
  Profile* profile() const { return profile_; }
  AppRegistrar* registrar() const { return registrar_; }
  WebAppOriginAssociationManager& association_manager() {
    return *association_manager_;
  }

 private:
  Profile* const profile_;
  AppRegistrar* registrar_;
  std::unique_ptr<WebAppOriginAssociationManager> association_manager_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_URL_HANDLER_MANAGER_H_
