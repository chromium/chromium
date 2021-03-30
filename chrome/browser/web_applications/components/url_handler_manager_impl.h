// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_URL_HANDLER_MANAGER_IMPL_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_URL_HANDLER_MANAGER_IMPL_H_

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/components/url_handler_launch_params.h"
#include "chrome/browser/web_applications/components/url_handler_manager.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "components/services/app_service/public/cpp/url_handler_info.h"
#include "url/gurl.h"

class Profile;

namespace web_app {

// UrlHandlerManagerImpl keeps track of web app install/update/uninstalls. This
// bookkeeping enables URL handler matching at browser startup time without
// needing to load additional user profiles.
class UrlHandlerManagerImpl : public UrlHandlerManager {
 public:
  explicit UrlHandlerManagerImpl(Profile* profile);
  ~UrlHandlerManagerImpl() override;

  UrlHandlerManagerImpl(const UrlHandlerManagerImpl&) = delete;
  UrlHandlerManagerImpl& operator=(const UrlHandlerManagerImpl&) = delete;

  // Looks for a URL in |command_line|. If one is found, find registered URL
  // handlers that match that URL.
  static std::vector<UrlHandlerLaunchParams> GetUrlHandlerMatches(
      const base::CommandLine& command_line);

  // Returns false if blink::features::kWebAppEnableUrlHandlers is disabled.
  void RegisterUrlHandlers(
      const AppId& app_id,
      base::OnceCallback<void(bool success)> callback) override;

  bool UnregisterUrlHandlers(const AppId& app_id) override;

  // Returns false and unregisters url handlers for |app_id| if
  // blink::features::kWebAppEnableUrlHandlers is disabled.
  void UpdateUrlHandlers(
      const AppId& app_id,
      base::OnceCallback<void(bool success)> callback) override;

 private:
  void OnDidGetAssociationsAtInstall(
      const AppId& app_id,
      base::OnceCallback<void(bool success)> callback,
      apps::UrlHandlers url_handlers);
  void OnDidGetAssociationsAtUpdate(
      const AppId& app_id,
      base::OnceCallback<void(bool success)> callback,
      apps::UrlHandlers url_handlers);

  base::WeakPtrFactory<UrlHandlerManagerImpl> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_URL_HANDLER_MANAGER_IMPL_H_
