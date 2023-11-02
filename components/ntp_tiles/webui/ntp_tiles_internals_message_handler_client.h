// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_TILES_WEBUI_NTP_TILES_INTERNALS_MESSAGE_HANDLER_CLIENT_H_
#define COMPONENTS_NTP_TILES_WEBUI_NTP_TILES_INTERNALS_MESSAGE_HANDLER_CLIENT_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/values.h"
#include "components/ntp_tiles/tile_source.h"

class PrefService;

namespace ntp_tiles {

class MostVisitedSites;

// Implemented by embedders to hook up NTPTilesInternalsMessageHandler.
class NTPTilesInternalsMessageHandlerClient {
 public:
  NTPTilesInternalsMessageHandlerClient(
      const NTPTilesInternalsMessageHandlerClient&) = delete;
  NTPTilesInternalsMessageHandlerClient& operator=(
      const NTPTilesInternalsMessageHandlerClient&) = delete;

  // Returns the PrefService for the embedder and containing WebUI page.
  virtual PrefService* GetPrefs() = 0;

  // False if in a browser mode (e.g. incognito) where tiles aren't supported.
  virtual bool SupportsNTPTiles() = 0;

  // Returns true if the given source is enabled (even if, in practice, none of
  // the tiles would come from it).
  virtual bool DoesSourceExist(TileSource source) = 0;

  // Creates a new MostVisitedSites based on the context pf the WebUI page.
  virtual std::unique_ptr<ntp_tiles::MostVisitedSites>
  MakeMostVisitedSites() = 0;

  // Registers a callback in Javascript. See content::WebUI and web::WebUIIOS.
  virtual void RegisterMessageCallback(
      base::StringPiece message,
      base::RepeatingCallback<void(const base::Value::List&)> callback) = 0;

  // Invokes a function in Javascript. See content::WebUI and web::WebUIIOS.
  virtual void CallJavascriptFunctionSpan(
      base::StringPiece name,
      base::span<const base::ValueView> values) = 0;

  // Convenience function for CallJavascriptFunctionSpan().
  template <typename... Arg>
  void CallJavascriptFunction(base::StringPiece name, const Arg&... arg) {
    base::ValueView args[] = {arg...};
    CallJavascriptFunctionSpan(name, args);
  }

 protected:
  NTPTilesInternalsMessageHandlerClient();
  virtual ~NTPTilesInternalsMessageHandlerClient();
};

}  // namespace ntp_tiles

#endif  // COMPONENTS_NTP_TILES_WEBUI_NTP_TILES_INTERNALS_MESSAGE_HANDLER_CLIENT_H_
