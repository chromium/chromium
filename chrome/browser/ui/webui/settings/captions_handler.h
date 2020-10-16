// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CAPTIONS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CAPTIONS_HANDLER_H_

#include <map>
#include <string>

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "components/component_updater/component_updater_service.h"

class PrefService;

namespace update_client {
struct CrxUpdateItem;
}

namespace settings {

// Settings handler for the captions settings page, chrome://settings/captions,
// and for caption settings on the main accessibility page,
// chrome://settings/accessibility, on non-ChromeOS desktop browsers.
class CaptionsHandler : public ::settings::SettingsPageUIHandler,
                        public component_updater::ServiceObserver {
 public:
  explicit CaptionsHandler(PrefService* prefs);
  ~CaptionsHandler() override;
  CaptionsHandler(const CaptionsHandler&) = delete;
  CaptionsHandler& operator=(const CaptionsHandler&) = delete;

  // SettingsPageUIHandler overrides:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

 private:
  void HandleCaptionsSubpageReady(const base::ListValue* args);
  void HandleOpenSystemCaptionsDialog(const base::ListValue* args);

  // component_updater::ServiceObserver:
  void OnEvent(Events event, const std::string& id) override;

  std::map<std::string, update_client::CrxUpdateItem> downloading_components_;
  PrefService* prefs_;
  ScopedObserver<component_updater::ComponentUpdateService,
                 component_updater::ComponentUpdateService::Observer>
      component_updater_observer_{this};
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CAPTIONS_HANDLER_H_
