// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_UI_WEBUI_NEARBY_INTERNALS_NEARBY_INTERNALS_PREFS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_NEARBY_INTERNALS_NEARBY_INTERNALS_PREFS_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace content {
class BrowserContext;
}  // namespace content

// WebUIMessageHandler for Nearby Prefs to allow users to Clear Prefs from the
// chrome://nearby-internals logging tab.
class NearbyInternalsPrefsHandler : public content::WebUIMessageHandler {
 public:
  explicit NearbyInternalsPrefsHandler(content::BrowserContext* context);
  NearbyInternalsPrefsHandler(const NearbyInternalsPrefsHandler&) = delete;
  NearbyInternalsPrefsHandler& operator=(const NearbyInternalsPrefsHandler&) =
      delete;
  ~NearbyInternalsPrefsHandler() override;

  // content::WebUIMessageHandler
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

 private:
  // Message handler callback that clears Nearby prefs in order to put the user
  // back into a state of before they have touched the feature.
  void HandleClearNearbyPrefs(const base::Value::List& args);

  raw_ptr<PrefService> pref_service_ = nullptr;

  base::WeakPtrFactory<NearbyInternalsPrefsHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_NEARBY_INTERNALS_NEARBY_INTERNALS_PREFS_HANDLER_H_
