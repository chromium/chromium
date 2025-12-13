// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_BROWSER_SWITCH_BROWSER_SWITCH_UI_H_
#define CHROME_BROWSER_UI_WEBUI_BROWSER_SWITCH_BROWSER_SWITCH_UI_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/browser_switcher/browser_switcher_prefs.h"
#include "chrome/browser/browser_switcher/browser_switcher_service.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"

class BrowserSwitchUI;

class BrowserSwitchUIConfig
    : public content::DefaultWebUIConfig<BrowserSwitchUI> {
 public:
  BrowserSwitchUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUIBrowserSwitchHost) {}

  // WebUIConfig:
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

class BrowserSwitchUI : public content::WebUIController {
 public:
  explicit BrowserSwitchUI(content::WebUI* web_ui);

  BrowserSwitchUI(const BrowserSwitchUI&) = delete;
  BrowserSwitchUI& operator=(const BrowserSwitchUI&) = delete;
};

class BrowserSwitchHandler : public content::WebUIMessageHandler {
 public:
  BrowserSwitchHandler();

  BrowserSwitchHandler(const BrowserSwitchHandler&) = delete;
  BrowserSwitchHandler& operator=(const BrowserSwitchHandler&) = delete;

  ~BrowserSwitchHandler() override;

  // WebUIMessageHandler
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

 private:
  void OnAllRulesetsParsed(browser_switcher::BrowserSwitcherService* service);

  void OnBrowserSwitcherPrefsChanged(
      browser_switcher::BrowserSwitcherPrefs* prefs,
      const std::vector<std::string>& changed_prefs);

  // For the internals page: tell JS to update all the page contents.
  void SendDataChangedEvent();

  // Launches the given URL in the configured alternative browser. Acts as a
  // bridge for |AlternativeBrowserDriver::TryLaunch()|. Then, if that succeeds,
  // closes the current tab.
  //
  // If it fails, the JavaScript promise is rejected. If it succeeds, the
  // JavaScript promise is not resolved, because we close the tab anyways.
  void HandleLaunchAlternativeBrowserAndCloseTab(const base::Value::List& args);

  void OnLaunchFinished(base::TimeTicks start,
                        std::string callback_id,
                        bool success);

  // Navigates to the New Tab Page.
  void HandleGotoNewTabPage(const base::Value::List& args);

  // Resolves a promise with a JSON object with all the LBS rulesets, formatted
  // like this:
  //
  // {
  //   "gpo": {
  //     "sitelist": ["example.com", ...],
  //     "greylist": [...]
  //   },
  //   "ieem": { "sitelist": [...], "greylist": [...] },
  //   "external": { "sitelist": [...], "greylist": [...] }
  // }
  void HandleGetAllRulesets(const base::Value::List& args);

  // Resolves a promise with a JSON object describing the decision for a URL
  // (stay/go) + reason. The result is formatted like this:
  //
  // {
  //   "action": ("stay"|"go"),
  //   "reason": ("globally_disabled"|"protocol"|"sitelist"|...),
  //   "matching_rule": (string|undefined)
  // }
  void HandleGetDecision(const base::Value::List& args);

  // Resolves a promise with the time of the last policy fetch and next policy
  // fetch, as JS timestamps.
  //
  // {
  //   "last_fetch": 123456789,
  //   "next_fetch": 234567890
  // }
  void HandleGetTimestamps(const base::Value::List& args);

  // Resolves a promise with the configured sitelist XML download URLs. The keys
  // are the name of the pref associated with the sitelist.
  //
  // {
  //   "browser_switcher": {
  //     "use_ie_sitelist": "http://example.com/sitelist.xml",
  //     "external_sitelist_url": "http://example.com/other_sitelist.xml",
  //     "external_greylist_url": null
  //   }
  // }
  void HandleGetRulesetSources(const base::Value::List& args);

  // Immediately re-download and apply XML rules.
  void HandleRefreshXml(const base::Value::List& args);

  // Resolves a promise with the boolean value describing whether the feature
  // is enabled or not which is configured by BrowserSwitcherEnabled key
  void HandleIsBrowserSwitchEnabled(const base::Value::List& args);

  // Handles the request for all internals data as a JSON string.
  void HandleGetBrowserSwitchInternalsJson(const base::Value::List& args);

  // Gathers all the data for the JSON export.
  std::string GetBrowserSwitchInternalsJson();

  base::CallbackListSubscription prefs_subscription_;

  base::CallbackListSubscription service_subscription_;

  base::WeakPtrFactory<BrowserSwitchHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_BROWSER_SWITCH_BROWSER_SWITCH_UI_H_
