// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_INTERNALS_QUERY_TILES_QUERY_TILES_INTERNALS_UI_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_INTERNALS_QUERY_TILES_QUERY_TILES_INTERNALS_UI_MESSAGE_HANDLER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "components/query_tiles/logger.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace base {
class ListValue;
}  // namespace base

class Profile;

namespace query_tiles {
class TileService;
}

class QueryTilesInternalsUIMessageHandler
    : public content::WebUIMessageHandler,
      public query_tiles::Logger::Observer {
 public:
  explicit QueryTilesInternalsUIMessageHandler(Profile* profile);
  ~QueryTilesInternalsUIMessageHandler() override;

  // content::WebUIMessageHandler implementation.
  void RegisterMessages() override;

 private:
  // Logger::Observer implementation.
  void OnServiceStatusChanged(const base::Value& status) override;
  void OnTileDataAvailable(const base::Value& status) override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  void HandleGetServiceStatus(const base::ListValue* args);
  void HandleGetTileData(const base::ListValue* args);
  void HandleStartFetch(const base::ListValue* args);
  void HandlePurgeDb(const base::ListValue* args);
  void HandleSetServerUrl(const base::ListValue* args);

  query_tiles::TileService* tile_service_;

  base::ScopedObservation<query_tiles::Logger, query_tiles::Logger::Observer>
      logger_observation_{this};

  base::WeakPtrFactory<QueryTilesInternalsUIMessageHandler> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_INTERNALS_QUERY_TILES_QUERY_TILES_INTERNALS_UI_MESSAGE_HANDLER_H_
