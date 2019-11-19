// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_PROTOCOL_HANDLERS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_PROTOCOL_HANDLERS_HANDLER_H_

#include <string>

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "chrome/common/custom_handlers/protocol_handler.h"

////////////////////////////////////////////////////////////////////////////////
// ProtocolHandlersHandler

// Listen for changes to protocol handlers (i.e. registerProtocolHandler()).
// This get triggered whenever a user allows a specific website or application
// to handle clicks on a link with a specified protocol (i.e. mailto: -> Gmail).

namespace base {
class DictionaryValue;
}

namespace settings {

class ProtocolHandlersHandler : public SettingsPageUIHandler,
                                public ProtocolHandlerRegistry::Observer {
 public:
  ProtocolHandlersHandler();
  ~ProtocolHandlersHandler() override;

  // SettingsPageUIHandler:
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;
  void RegisterMessages() override;

  // ProtocolHandlerRegistry::Observer:
  void OnProtocolHandlerRegistryChanged() override;

 private:
  // Called to fetch the state of the protocol handlers. If the full list of
  // handlers is not needed, consider HandleObserveProtocolHandlersEnabledState
  // instead.
  void HandleObserveProtocolHandlers(const base::ListValue* args);

  // Called to begin updates to the handlers enabled status. This is a subset
  // (lighter alternative) of HandleObserveProtocolHandlers. There's no need to
  // call this function if HandleObserveProtocolHandlers is called.
  void HandleObserveProtocolHandlersEnabledState(const base::ListValue* args);

  // Notifies the JS side whether the handlers are enabled or not.
  void SendHandlersEnabledValue();

  // Called when the user toggles whether custom handlers are enabled.
  void HandleSetHandlersEnabled(const base::ListValue* args);

  // Called when the user sets a new default handler for a protocol.
  void HandleSetDefault(const base::ListValue* args);

  // Parses a ProtocolHandler out of the arguments passed back from the view.
  // |args| is a list of [protocol, url].
  ProtocolHandler ParseHandlerFromArgs(const base::ListValue* args) const;

  // Returns a JSON object describing the set of protocol handlers for the
  // given protocol.
  void GetHandlersForProtocol(const std::string& protocol,
                              base::DictionaryValue* value);

  // Returns a JSON list of the ignored protocol handlers.
  void GetIgnoredHandlers(base::ListValue* handlers);

  // Called when the JS PasswordManager object is initialized.
  void UpdateHandlerList();

  // Remove a handler.
  // |args| is a list of [protocol, url].
  void HandleRemoveHandler(const base::ListValue* args);

  ProtocolHandlerRegistry* GetProtocolHandlerRegistry();

  ScopedObserver<ProtocolHandlerRegistry, ProtocolHandlerRegistry::Observer>
      registry_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(ProtocolHandlersHandler);
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_PROTOCOL_HANDLERS_HANDLER_H_
