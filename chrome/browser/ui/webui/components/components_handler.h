// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_COMPONENTS_COMPONENTS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_COMPONENTS_COMPONENTS_HANDLER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "components/component_updater/component_updater_service.h"
#include "components/update_client/update_client.h"
#include "content/public/browser/web_ui_message_handler.h"

// The handler for Javascript messages for the chrome://components/ page.
class ComponentsHandler : public content::WebUIMessageHandler,
                          public component_updater::ServiceObserver {
 public:
  ComponentsHandler(
      component_updater::ComponentUpdateService* component_updater);
  ComponentsHandler(const ComponentsHandler&) = delete;
  ComponentsHandler& operator=(const ComponentsHandler&) = delete;
  ~ComponentsHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // Callback for the "requestComponentsData" message.
  void HandleRequestComponentsData(const base::Value::List& args);

  // Callback for the "checkUpdate" message.
  void HandleCheckUpdate(const base::Value::List& args);

  // ServiceObserver implementation.
  void OnEvent(const update_client::CrxUpdateItem& item) override;

#if BUILDFLAG(IS_CHROMEOS)
  // Callback for the "crosUrlComponentsRedirect" message.
  void HandleCrosUrlComponentsRedirect(const base::Value::List& args);
#endif

 private:
  static std::u16string ServiceStatusToString(
      update_client::ComponentState state);

  base::Value::List LoadComponents();
  void OnDemandUpdate(const std::string& component_id);

  // Weak pointer; injected for testing.
  const raw_ptr<component_updater::ComponentUpdateService> component_updater_;

  base::ScopedObservation<component_updater::ComponentUpdateService,
                          component_updater::ComponentUpdateService::Observer>
      observation_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_COMPONENTS_COMPONENTS_HANDLER_H_
