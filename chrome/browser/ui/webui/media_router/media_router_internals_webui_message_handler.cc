// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/media_router/media_router_internals_webui_message_handler.h"

#include "base/bind.h"
#include "components/media_router/browser/media_router.h"

namespace media_router {

namespace {

base::Value::List CastProviderStateToValue(
    const mojom::CastProviderState& state) {
  base::Value::List result;
  for (const mojom::CastSessionStatePtr& session : state.session_state) {
    base::Value::Dict session_value;
    session_value.Set("sink_id", session->sink_id);
    session_value.Set("app_id", session->app_id);
    session_value.Set("session_id", session->session_id);
    session_value.Set("route_description", session->route_description);
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
    const base::Value::List& args) {
  AllowJavascript();
  const base::Value& callback_id = args[0];
  ResolveJavascriptCallback(callback_id, router_->GetState());
}

void MediaRouterInternalsWebUIMessageHandler::HandleGetProviderState(
    const base::Value::List& args) {
  AllowJavascript();
  base::Value callback_id = args[0].Clone();
  if (args.size() != 2 || !args[1].is_string()) {
    RejectJavascriptCallback(callback_id, base::Value("Invalid arguments"));
    return;
  }

  absl::optional<mojom::MediaRouteProviderId> provider_id =
      ProviderIdFromString(args[1].GetString());
  if (!provider_id) {
    RejectJavascriptCallback(callback_id,
                             base::Value("Unknown MediaRouteProviderId"));
    return;
  }
  router_->GetProviderState(
      *provider_id,
      base::BindOnce(&MediaRouterInternalsWebUIMessageHandler::OnProviderState,
                     weak_factory_.GetWeakPtr(), std::move(callback_id)));
}

void MediaRouterInternalsWebUIMessageHandler::HandleGetLogs(
    const base::Value::List& args) {
  AllowJavascript();
  const base::Value& callback_id = args[0];
  ResolveJavascriptCallback(callback_id, router_->GetLogs());
}

void MediaRouterInternalsWebUIMessageHandler::OnProviderState(
    base::Value callback_id,
    mojom::ProviderStatePtr state) {
  if (state && state->is_cast_provider_state() &&
      state->get_cast_provider_state()) {
    ResolveJavascriptCallback(
        callback_id,
        CastProviderStateToValue(*(state->get_cast_provider_state())));
  } else {
    ResolveJavascriptCallback(callback_id, base::Value());
  }
}

}  // namespace media_router
