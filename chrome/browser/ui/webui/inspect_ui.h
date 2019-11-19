// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_INSPECT_UI_H_
#define CHROME_BROWSER_UI_WEBUI_INSPECT_UI_H_

#include <map>
#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"

namespace base {
class Value;
class ListValue;
}

namespace content {
class DevToolsAgentHost;
}

class Browser;
class DevToolsTargetsUIHandler;
class PortForwardingStatusSerializer;

class InspectUI : public content::WebUIController,
                  public content::NotificationObserver {
 public:
  explicit InspectUI(content::WebUI* web_ui);
  ~InspectUI() override;

  void InitUI();
  void Inspect(const std::string& source_id, const std::string& target_id);
  void InspectFallback(const std::string& source_id,
                       const std::string& target_id);
  void Activate(const std::string& source_id, const std::string& target_id);
  void Close(const std::string& source_id, const std::string& target_id);
  void Reload(const std::string& source_id, const std::string& target_id);
  void Open(const std::string& source_id,
            const std::string& browser_id,
            const std::string& url);
  void Pause(const std::string& source_id, const std::string& target_id);
  void InspectBrowserWithCustomFrontend(
      const std::string& source_id,
      const std::string& browser_id,
      const GURL& frontend_url);

  static void InspectDevices(Browser* browser);

 private:
  // content::NotificationObserver overrides.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  void StartListeningNotifications();
  void StopListeningNotifications();

  content::WebUIDataSource* CreateInspectUIHTMLSource();

  void UpdateDiscoverUsbDevicesEnabled();
  void UpdatePortForwardingEnabled();
  void UpdatePortForwardingConfig();
  void UpdateTCPDiscoveryEnabled();
  void UpdateTCPDiscoveryConfig();

  void SetPortForwardingDefaults();

  const base::Value* GetPrefValue(const char* name);

  void AddTargetUIHandler(std::unique_ptr<DevToolsTargetsUIHandler> handler);

  DevToolsTargetsUIHandler* FindTargetHandler(
      const std::string& source_id);
  scoped_refptr<content::DevToolsAgentHost> FindTarget(
      const std::string& source_id,
      const std::string& target_id);

  void PopulateTargets(const std::string& source_id,
                       const base::ListValue& targets);

  void PopulateAdditionalTargets(const base::Value& targets);

  void PopulatePortStatus(const base::Value& status);

  void ShowIncognitoWarning();

  // A scoped container for notification registries.
  content::NotificationRegistrar notification_registrar_;

  // A scoped container for preference change registries.
  PrefChangeRegistrar pref_change_registrar_;

  std::map<std::string, std::unique_ptr<DevToolsTargetsUIHandler>>
      target_handlers_;

  std::unique_ptr<PortForwardingStatusSerializer> port_status_serializer_;

  DISALLOW_COPY_AND_ASSIGN(InspectUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_INSPECT_UI_H_
