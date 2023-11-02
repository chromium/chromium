// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/internals/query_tiles/query_tiles_internals_ui_message_handler.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/query_tiles/tile_service_factory.h"
#include "components/query_tiles/tile_service.h"
#include "content/public/browser/web_ui.h"

QueryTilesInternalsUIMessageHandler::QueryTilesInternalsUIMessageHandler(
    Profile* profile)
    : tile_service_(query_tiles::TileServiceFactory::GetForKey(
          profile->GetProfileKey())) {
  CHECK(tile_service_);
}

QueryTilesInternalsUIMessageHandler::~QueryTilesInternalsUIMessageHandler() =
    default;

void QueryTilesInternalsUIMessageHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "startFetch", base::BindRepeating(
                        &QueryTilesInternalsUIMessageHandler::HandleStartFetch,
                        base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "purgeDb",
      base::BindRepeating(&QueryTilesInternalsUIMessageHandler::HandlePurgeDb,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "getServiceStatus",
      base::BindRepeating(
          &QueryTilesInternalsUIMessageHandler::HandleGetServiceStatus,
          weak_ptr_factory_.GetWeakPtr()));

  web_ui()->RegisterMessageCallback(
      "getTileData",
      base::BindRepeating(
          &QueryTilesInternalsUIMessageHandler::HandleGetTileData,
          weak_ptr_factory_.GetWeakPtr()));

  web_ui()->RegisterMessageCallback(
      "setServerUrl",
      base::BindRepeating(
          &QueryTilesInternalsUIMessageHandler::HandleSetServerUrl,
          weak_ptr_factory_.GetWeakPtr()));
}

void QueryTilesInternalsUIMessageHandler::HandleGetTileData(
    const base::Value::List& args) {
  AllowJavascript();
  const base::Value& callback_id = args[0];
  ResolveJavascriptCallback(callback_id,
                            tile_service_->GetLogger()->GetTileData());
}

void QueryTilesInternalsUIMessageHandler::HandleGetServiceStatus(
    const base::Value::List& args) {
  AllowJavascript();
  const base::Value& callback_id = args[0];
  ResolveJavascriptCallback(callback_id,
                            tile_service_->GetLogger()->GetServiceStatus());
}

void QueryTilesInternalsUIMessageHandler::HandleStartFetch(
    const base::Value::List& args) {
  AllowJavascript();
  tile_service_->StartFetchForTiles(false /*is_from_reduce_mode*/,
                                    base::BindOnce([](bool reschedule) {}));
}

void QueryTilesInternalsUIMessageHandler::HandlePurgeDb(
    const base::Value::List& args) {
  tile_service_->PurgeDb();
}

void QueryTilesInternalsUIMessageHandler::HandleSetServerUrl(
    const base::Value::List& args) {
  AllowJavascript();
  DCHECK_EQ(args.size(), 1u) << "Missing argument server URL.";
  tile_service_->SetServerUrl(args[0].GetString());
}

void QueryTilesInternalsUIMessageHandler::OnServiceStatusChanged(
    const base::Value& status) {
  FireWebUIListener("service-status-changed", status);
}

void QueryTilesInternalsUIMessageHandler::OnTileDataAvailable(
    const base::Value& data) {
  FireWebUIListener("tile-data-available", data);
}

void QueryTilesInternalsUIMessageHandler::OnJavascriptAllowed() {
  logger_observation_.Observe(tile_service_->GetLogger());
}

void QueryTilesInternalsUIMessageHandler::OnJavascriptDisallowed() {
  logger_observation_.Reset();
}
