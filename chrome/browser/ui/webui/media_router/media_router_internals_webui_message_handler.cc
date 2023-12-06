// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/media_router/media_router_internals_webui_message_handler.h"

#include "base/functional/bind.h"
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
    MediaRouterInternalsWebUIMessageHandler(const MediaRouter* router,
                                            MediaRouterDebugger& debugger)
    : router_(router), debugger_(debugger) {
  DCHECK(router_);
  debugger_->AddObserver(*this);
}

MediaRouterInternalsWebUIMessageHandler::
    ~MediaRouterInternalsWebUIMessageHandler() {
  debugger_->RemoveObserver(*this);
}

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
  web_ui()->RegisterMessageCallback(
      "getMirroringStats",
      base::BindRepeating(
          &MediaRouterInternalsWebUIMessageHandler::HandleGetMirroringStats,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setMirroringStatsEnabled",
      base::BindRepeating(&MediaRouterInternalsWebUIMessageHandler::
                              HandleSetMirroringStatsEnabled,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "isMirroringStatsEnabled",
      base::BindRepeating(&MediaRouterInternalsWebUIMessageHandler::
                              HandleIsMirroringStatsEnabled,
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

  std::optional<mojom::MediaRouteProviderId> provider_id =
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

void MediaRouterInternalsWebUIMessageHandler::HandleGetMirroringStats(
    const base::Value::List& args) {
  AllowJavascript();
  const base::Value& callback_id = args[0];
  ResolveJavascriptCallback(callback_id, debugger_->GetMirroringStats());
}

void MediaRouterInternalsWebUIMessageHandler::HandleSetMirroringStatsEnabled(
    const base::Value::List& args) {
  AllowJavascript();
  const base::Value& callback_id = args[0];
  const bool should_enable = args[1].GetBool();

  if (should_enable) {
    debugger_->EnableRtcpReports();
    ResolveJavascriptCallback(
        callback_id,
        base::Value("Mirroring Stats will be fetched and displayed "
                    "on your next mirroring session."));
  } else {
    debugger_->DisableRtcpReports();

    ResolveJavascriptCallback(
        callback_id, base::Value("Mirroring Stats will not be fetched or "
                                 "displayed during your mirroring sessions."));
  }
}

void MediaRouterInternalsWebUIMessageHandler::HandleIsMirroringStatsEnabled(
    const base::Value::List& args) {
  AllowJavascript();
  const base::Value& callback_id = args[0];

  ResolveJavascriptCallback(callback_id,
                            debugger_->ShouldFetchMirroringStats());
}

void MediaRouterInternalsWebUIMessageHandler::OnMirroringStatsUpdated(
    const base::Value::Dict& json_logs) {
  AllowJavascript();
  FireWebUIListener("on-mirroring-stats-update", std::move(json_logs));
}

}  // namespace media_router
