// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_MEDIA_ROUTER_MEDIA_ROUTER_INTERNALS_WEBUI_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_MEDIA_ROUTER_MEDIA_ROUTER_INTERNALS_WEBUI_MESSAGE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "components/media_router/browser/media_router_debugger.h"
#include "components/media_router/common/mojom/media_router.mojom.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace media_router {

class MediaRouter;

// The handler for Javascript messages related to the media router internals
// page.
class MediaRouterInternalsWebUIMessageHandler
    : public content::WebUIMessageHandler,
      public MediaRouterDebugger::MirroringStatsObserver {
 public:
  explicit MediaRouterInternalsWebUIMessageHandler(
      const MediaRouter* router,
      MediaRouterDebugger& debugger);
  ~MediaRouterInternalsWebUIMessageHandler() override;

 private:
  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

  // Handlers for JavaScript messages.
  void HandleGetState(const base::Value::List& args);
  void HandleGetProviderState(const base::Value::List& args);
  void HandleGetLogs(const base::Value::List& args);

  void HandleGetMirroringStats(const base::Value::List& args);
  void HandleSetMirroringStatsEnabled(const base::Value::List& args);
  void HandleIsMirroringStatsEnabled(const base::Value::List& args);

  // MirroringStatsObserver implementation.
  void OnMirroringStatsUpdated(const base::Value::Dict& json_logs) override;

  void OnProviderState(base::Value callback_id, mojom::ProviderStatePtr state);

  // Pointer to the MediaRouter.
  const raw_ptr<const MediaRouter> router_;
  const raw_ref<MediaRouterDebugger> debugger_;

  base::WeakPtrFactory<MediaRouterInternalsWebUIMessageHandler> weak_factory_{
      this};
};

}  // namespace media_router

#endif  // CHROME_BROWSER_UI_WEBUI_MEDIA_ROUTER_MEDIA_ROUTER_INTERNALS_WEBUI_MESSAGE_HANDLER_H_
