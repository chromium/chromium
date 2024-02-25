// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_PROTOCOL_HANDLERS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_PROTOCOL_HANDLERS_HANDLER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_install_manager_observer.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registrar_observer.h"
#include "components/custom_handlers/protocol_handler_registry.h"
#include "components/webapps/common/web_app_id.h"

////////////////////////////////////////////////////////////////////////////////
// ProtocolHandlersHandler

// Listen for changes to protocol handlers registrations.
// This get triggered whenever a user allows or disallows a specific website or
// application to handle clicks on a link with a specified protocol (i.e.
// mailto: -> Gmail).

namespace settings {

class ProtocolHandlersHandler
    : public SettingsPageUIHandler,
      public custom_handlers::ProtocolHandlerRegistry::Observer,
      public web_app::WebAppRegistrarObserver,
      public web_app::WebAppInstallManagerObserver {
 public:
  explicit ProtocolHandlersHandler(Profile* profile);

  ProtocolHandlersHandler(const ProtocolHandlersHandler&) = delete;
  ProtocolHandlersHandler& operator=(const ProtocolHandlersHandler&) = delete;

  ~ProtocolHandlersHandler() override;

  // SettingsPageUIHandler:
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;
  void RegisterMessages() override;

  // ProtocolHandlerRegistry::Observer:
  void OnProtocolHandlerRegistryChanged() override;

  // web_app::WebAppInstallManagerObserver:
  void OnWebAppUninstalled(
      const webapps::AppId& app_id,
      webapps::WebappUninstallSource uninstall_source) override;
  void OnWebAppInstallManagerDestroyed() override;

  // web_app::WebAppRegistrarObserver:
  void OnWebAppProtocolSettingsChanged() override;
  void OnAppRegistrarDestroyed() override;

 private:
  // Called to fetch the state of the protocol handlers. If the full list of
  // handlers is not needed, consider HandleObserveProtocolHandlersEnabledState
  // instead.
  void HandleObserveProtocolHandlers(const base::Value::List& args);

  // Called to begin updates to the handlers enabled status. This is a subset
  // (lighter alternative) of HandleObserveProtocolHandlers. There's no need to
  // call this function if HandleObserveProtocolHandlers is called.
  void HandleObserveProtocolHandlersEnabledState(const base::Value::List& args);

  // Notifies the JS side whether the handlers are enabled or not.
  void SendHandlersEnabledValue();

  // Called when the user toggles whether custom handlers are enabled.
  void HandleSetHandlersEnabled(const base::Value::List& args);

  // Called when the user sets a new default handler for a protocol.
  void HandleSetDefault(const base::Value::List& args);

  // Parses a ProtocolHandler out of the arguments passed back from the view.
  // |args| is a list of [protocol, url].
  custom_handlers::ProtocolHandler ParseHandlerFromArgs(
      const base::Value::List& args) const;

  // Populates a JSON object describing the set of protocol handlers for the
  // given protocol.
  base::Value::Dict GetHandlersForProtocol(const std::string& protocol);

  // Returns a JSON list of the ignored protocol handlers.
  base::Value::List GetIgnoredHandlers();

  // Called when the JS PasswordManager object is initialized.
  void UpdateHandlerList();

  // Remove a handler.
  // |args| is a list of [protocol, url].
  void HandleRemoveHandler(const base::Value::List& args);

  custom_handlers::ProtocolHandlerRegistry* GetProtocolHandlerRegistry();

  base::ScopedObservation<custom_handlers::ProtocolHandlerRegistry,
                          custom_handlers::ProtocolHandlerRegistry::Observer>
      registry_observation_{this};

  // Web App Protocol Handler specific functions:

  // Called to fetch the state of the app protocol handlers.
  void HandleObserveAppProtocolHandlers(const base::Value::List& args);

  // Parses an App ProtocolHandler out of |args|, which is a list of [protocol,
  // url, app_id].
  custom_handlers::ProtocolHandler ParseAppHandlerFromArgs(
      const base::Value::List& args) const;

  // Returns a Value::Dict describing the set of app protocol handlers for
  // the given |protocol| in the given |handlers| list.
  base::Value::Dict GetAppHandlersForProtocol(
      const std::string& protocol,
      custom_handlers::ProtocolHandlerRegistry::ProtocolHandlerList handlers);

  // Called when OnWebAppProtocolSettingsChanged() is notified or on page load.
  void UpdateAllAllowedLaunchProtocols();

  // Called when OnWebAppProtocolSettingsChanged() is notified or on page load.
  void UpdateAllDisallowedLaunchProtocols();

  // Used to remove a protocol handler from the approved or disapproved list.
  // |args| is a list of [protocol, url, app_id].
  void ResetProtocolHandlerUserApproval(const base::Value::List& args);

  const raw_ptr<Profile> profile_;
  const raw_ptr<web_app::WebAppProvider> web_app_provider_;

  base::ScopedObservation<web_app::WebAppRegistrar,
                          web_app::WebAppRegistrarObserver>
      app_observation_{this};
  base::ScopedObservation<web_app::WebAppInstallManager,
                          web_app::WebAppInstallManagerObserver>
      install_manager_observation_{this};
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_PROTOCOL_HANDLERS_HANDLER_H_
