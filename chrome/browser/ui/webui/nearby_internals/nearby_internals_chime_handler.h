// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NEARBY_INTERNALS_NEARBY_INTERNALS_CHIME_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_NEARBY_INTERNALS_NEARBY_INTERNALS_CHIME_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "content/public/browser/web_ui_message_handler.h"

class NearbyInternalsChimeHandler : public content::WebUIMessageHandler {
 public:
  NearbyInternalsChimeHandler();
  NearbyInternalsChimeHandler(const NearbyInternalsChimeHandler&) = delete;
  NearbyInternalsChimeHandler& operator=(const NearbyInternalsChimeHandler&) =
      delete;
  ~NearbyInternalsChimeHandler() override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

 private:
  void Initialize(const base::Value::List& args);
  void HandleAddChimeClient(const base::Value::List& args);

  base::WeakPtrFactory<NearbyInternalsChimeHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_NEARBY_INTERNALS_NEARBY_INTERNALS_CHIME_HANDLER_H_
