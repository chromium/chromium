// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_INSPECT_UI_H_
#define CHROME_BROWSER_UI_WEBUI_INSPECT_UI_H_

#include <map>
#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"

namespace content {
class DevToolsAgentHost;
}

class Browser;
class DevToolsTargetsUIHandler;
class PortForwardingStatusSerializer;

class InspectUI : public content::WebUIController,
                  public content::WebContentsObserver {
 public:
  explicit InspectUI(content::WebUI* web_ui);
  InspectUI(const InspectUI&) = delete;
  InspectUI& operator=(const InspectUI&) = delete;
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

  void PopulateNativeUITargets(const base::Value::List& targets);
  void ShowNativeUILaunchButton(bool enabled);
  void SetHostVersion(const std::string& version);

  static void InspectDevices(Browser* browser);

 private:
  // content::WebContentsObserver:
  void WebContentsDestroyed() override;

  void StartListeningNotifications();
  void StopListeningNotifications();

  void UpdateDiscoverUsbDevicesEnabled();
  void UpdatePortForwardingEnabled();
  void UpdatePortForwardingConfig();
  void UpdateTCPDiscoveryEnabled();
  void UpdateTCPDiscoveryConfig();
  void UpdateBubbleLockingCheckbox();

  void SetPortForwardingDefaults();

  const base::Value* GetPrefValue(const char* name);

  void AddTargetUIHandler(std::unique_ptr<DevToolsTargetsUIHandler> handler);

  DevToolsTargetsUIHandler* FindTargetHandler(
      const std::string& source_id);
  scoped_refptr<content::DevToolsAgentHost> FindTarget(
      const std::string& source_id,
      const std::string& target_id);

  void PopulateTargets(const std::string& source_id,
                       const base::Value& targets);

  void PopulatePortStatus(base::Value status);

  void ShowIncognitoWarning();

  // A scoped container for preference change registries.
  PrefChangeRegistrar pref_change_registrar_;

  std::map<std::string, std::unique_ptr<DevToolsTargetsUIHandler>>
      target_handlers_;

  std::unique_ptr<PortForwardingStatusSerializer> port_status_serializer_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_INSPECT_UI_H_
