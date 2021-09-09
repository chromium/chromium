// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_PROTOCOL_HANDLER_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_PROTOCOL_HANDLER_MANAGER_H_

#include "base/bind.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "components/services/app_service/public/cpp/protocol_handler_info.h"
#include "content/public/common/custom_handlers/protocol_handler.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#include <vector>

class Profile;

namespace web_app {

class WebAppRegistrar;

using content::ProtocolHandler;

class WebAppProtocolHandlerManager {
 public:
  explicit WebAppProtocolHandlerManager(Profile* profile);
  WebAppProtocolHandlerManager(const WebAppProtocolHandlerManager&) = delete;
  WebAppProtocolHandlerManager& operator=(const WebAppProtocolHandlerManager&) =
      delete;
  virtual ~WebAppProtocolHandlerManager();

  // |registrar| is used to observe OnWebAppInstalled/Uninstalled events.
  void SetSubsystems(WebAppRegistrar* registrar);
  void Start();

  // If a protocol handler matching the scheme of |protocol_url| is installed
  // for the app indicated by |app_id|, this method will translate the protocol
  // to a full app URL.
  // If no matching handler is installed, no URL is returned.
  absl::optional<GURL> TranslateProtocolUrl(const AppId& app_id,
                                            const GURL& protocol_url) const;

  // Get the list of handlers for the given protocol.
  std::vector<ProtocolHandler> GetHandlersFor(
      const std::string& protocol) const;

  // Gets all protocol handlers for |app_id|.
  // `virtual` for testing.
  virtual std::vector<apps::ProtocolHandlerInfo> GetAppProtocolHandlerInfos(
      const std::string& app_id) const;

  // Gets all protocol handlers for |app_id| as custom handler objects.
  std::vector<ProtocolHandler> GetAppProtocolHandlers(
      const AppId& app_id) const;

  // Registers OS specific protocol handlers for OSs that need them, using the
  // protocol handler information supplied in the app manifest.
  void RegisterOsProtocolHandlers(const AppId& app_id,
                                  base::OnceCallback<void(bool)> callback);

  // Registers OS specific protocol handlers for OSs that need them, using
  // arbitrary protocol handler information.
  void RegisterOsProtocolHandlers(
      const AppId& app_id,
      const std::vector<apps::ProtocolHandlerInfo>& protocol_handlers,
      base::OnceCallback<void(bool)> callback);

  // Unregisters OS specific protocol handlers for an app.
  void UnregisterOsProtocolHandlers(const AppId& app_id,
                                    base::OnceCallback<void(bool)> callback);

 private:
  WebAppRegistrar* app_registrar_;
  Profile* const profile_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_PROTOCOL_HANDLER_MANAGER_H_
