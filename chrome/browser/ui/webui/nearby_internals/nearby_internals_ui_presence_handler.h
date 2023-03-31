// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NEARBY_INTERNALS_NEARBY_INTERNALS_UI_PRESENCE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_NEARBY_INTERNALS_NEARBY_INTERNALS_UI_PRESENCE_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "content/public/browser/web_ui_message_handler.h"

class NearbyInternalsPresenceHandler : public content::WebUIMessageHandler {
 public:
  NearbyInternalsPresenceHandler();
  NearbyInternalsPresenceHandler(const NearbyInternalsPresenceHandler&) =
      delete;
  NearbyInternalsPresenceHandler& operator=(
      const NearbyInternalsPresenceHandler&) = delete;
  ~NearbyInternalsPresenceHandler() override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

 private:
  void Initialize(const base::Value::List& args);
  void HandleStartPresenceScan(const base::Value::List& args);

  base::WeakPtrFactory<NearbyInternalsPresenceHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_NEARBY_INTERNALS_NEARBY_INTERNALS_UI_PRESENCE_HANDLER_H_
