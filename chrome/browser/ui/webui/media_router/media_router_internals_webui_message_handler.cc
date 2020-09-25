// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/media_router/media_router_internals_webui_message_handler.h"

#include "base/bind.h"
#include "components/media_router/browser/media_router.h"

namespace media_router {

namespace {

base::Value CastProviderStateToValue(const mojom::CastProviderState& state) {
  base::Value result(base::Value::Type::LIST);
  for (const mojom::CastSessionStatePtr& session : state.session_state) {
    base::Value session_value(base::Value::Type::DICTIONARY);
    session_value.SetStringKey("sink_id", session->sink_id);
    session_value.SetStringKey("app_id", session->app_id);
    session_value.SetStringKey("session_id", session->session_id);
    session_value.SetStringKey("route_description", session->route_description);
    result.Append(std::move(session_value));
  }
  return result;
}

}  // namespace

MediaRouterInternalsWebUIMessageHandler::
    MediaRouterInternalsWebUIMessageHandler(const MediaRouter* router)
    : router_(router) {
  DCHECK(router_);
}

MediaRouterInternalsWebUIMessageHandler::
    ~MediaRouterInternalsWebUIMessageHandler() = default;

void MediaRouterInternalsWebUIMessageHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getState", base::BindRepeating(
                      &MediaRouterInternalsWebUIMessageHandler::HandleGetState,
                      base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getProviderState",
      base::BindRepeating(
          &MediaRouterInternalsWebUIMessageHandler::HandleGetProviderState,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getLogs", base::BindRepeating(
                     &MediaRouterInternalsWebUIMessageHandler::HandleGetLogs,
                     base::Unretained(this)));
}

void MediaRouterInternalsWebUIMessageHandler::HandleGetState(
    const base::ListValue* args) {
  AllowJavascript();
  const base::Value& callback_id = args->GetList()[0];
  ResolveJavascriptCallback(callback_id, router_->GetState());
}

void MediaRouterInternalsWebUIMessageHandler::HandleGetProviderState(
    const base::ListValue* args) {
  AllowJavascript();
  base::Value callback_id = args->GetList()[0].Clone();
  if (args->GetList().size() != 2 || !args->GetList()[1].is_string()) {
    RejectJavascriptCallback(callback_id, base::Value("Invalid arguments"));
  }

  MediaRouteProviderId provider_id =
      ProviderIdFromString(args->GetList()[1].GetString());
  if (provider_id == MediaRouteProviderId::UNKNOWN) {
    RejectJavascriptCallback(callback_id,
                             base::Value("Unknown MediaRouteProviderId"));
  }
  router_->GetProviderState(
      provider_id,
      base::BindOnce(&MediaRouterInternalsWebUIMessageHandler::OnProviderState,
                     weak_factory_.GetWeakPtr(), std::move(callback_id)));
}

void MediaRouterInternalsWebUIMessageHandler::HandleGetLogs(
    const base::ListValue* args) {
  AllowJavascript();
  const base::Value& callback_id = args->GetList()[0];
  ResolveJavascriptCallback(callback_id, router_->GetLogs());
}

void MediaRouterInternalsWebUIMessageHandler::OnProviderState(
    base::Value callback_id,
    mojom::ProviderStatePtr state) {
  base::Value result;
  if (state && state->is_cast_provider_state() &&
      state->get_cast_provider_state()) {
    result = CastProviderStateToValue(*(state->get_cast_provider_state()));
  }
  ResolveJavascriptCallback(callback_id, result);
}

}  // namespace media_router
