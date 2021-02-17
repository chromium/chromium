// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_PROTOCOL_HANDLER_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_PROTOCOL_HANDLER_MANAGER_H_

#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/common/custom_handlers/protocol_handler.h"
#include "components/services/app_service/public/cpp/protocol_handler_info.h"

#include <vector>

namespace web_app {

class ProtocolHandlerManager {
 public:
  explicit ProtocolHandlerManager(Profile* profile);
  ProtocolHandlerManager(const ProtocolHandlerManager&) = delete;
  ProtocolHandlerManager& operator=(const ProtocolHandlerManager&) = delete;
  virtual ~ProtocolHandlerManager();

  // |registrar| is used to observe OnWebAppInstalled/Uninstalled events.
  void SetSubsystems(AppRegistrar* registrar);
  void Start();

  // Get the list of handlers for the given protocol.
  std::vector<ProtocolHandler> GetHandlersFor(
      const std::string& protocol) const;

  // Gets all protocol handlers for |app_id|.
  virtual std::vector<apps::ProtocolHandlerInfo> GetAppProtocolHandlerInfos(
      const std::string& app_id) const = 0;

  // Gets all protocol handlers for |app_id| as custom handler objects.
  std::vector<ProtocolHandler> GetAppProtocolHandlers(
      const AppId& app_id) const;

  // Registers OS specific protocol handlers for OSs that need them, using the
  // protocol handler information supplied in the app manifest.
  void RegisterOsProtocolHandlers(const AppId& app_id);

  // Registers OS specific protocol handlers for OSs that need them, using
  // arbitrary protocol handler information.
  void RegisterOsProtocolHandlers(
      const AppId& app_id,
      const std::vector<apps::ProtocolHandlerInfo>& protocol_handlers);

  // Unregisters OS specific protocol handlers for OSs that need them, using the
  // protocol handler information supplied in the app manifest.
  void UnregisterOsProtocolHandlers(const AppId& app_id);

  // Unregisters OS specific protocol handlers for OSs that need them, using
  // arbitrary protocol handler information.
  void UnregisterOsProtocolHandlers(
      const AppId& app_id,
      const std::vector<apps::ProtocolHandlerInfo>& protocol_handlers);

  AppRegistrar* app_registrar_;

 private:
  Profile* const profile_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_PROTOCOL_HANDLER_MANAGER_H_
