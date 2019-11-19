// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_MEDIA_ROUTER_MEDIA_ROUTER_INTERNALS_WEBUI_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_MEDIA_ROUTER_MEDIA_ROUTER_INTERNALS_WEBUI_MESSAGE_HANDLER_H_

#include <string>

#include "content/public/browser/web_ui_message_handler.h"

namespace media_router {

class MediaRouter;

// The handler for Javascript messages related to the media router internals
// page.
class MediaRouterInternalsWebUIMessageHandler
    : public content::WebUIMessageHandler {
 public:
  explicit MediaRouterInternalsWebUIMessageHandler(const MediaRouter* router);

 private:
  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

  // Handlers for JavaScript messages.
  void HandleGetStatus(const base::ListValue* args);

  // Pointer to the MediaRouter.
  const MediaRouter* router_;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_UI_WEBUI_MEDIA_ROUTER_MEDIA_ROUTER_INTERNALS_WEBUI_MESSAGE_HANDLER_H_
