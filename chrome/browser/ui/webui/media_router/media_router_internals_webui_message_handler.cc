// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/media_router/media_router_internals_webui_message_handler.h"

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/trace_event/trace_config.h"
#include "components/media_router/browser/media_router.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/tracing_controller.h"
#include "services/tracing/public/cpp/perfetto/perfetto_config.h"
#include "third_party/perfetto/include/perfetto/tracing/core/trace_config.h"

namespace media_router {

namespace {

struct TraceReader : public base::RefCountedThreadSafe<TraceReader> {
  explicit TraceReader(std::unique_ptr<perfetto::TracingSession> session)
      : session(std::move(session)) {}
  std::unique_ptr<perfetto::TracingSession> session;

 private:
  friend class base::RefCountedThreadSafe<TraceReader>;
  ~TraceReader() = default;
};

base::ListValue CastProviderStateToValue(
    const mojom::CastProviderState& state) {
  base::ListValue result;
  for (const mojom::CastSessionStatePtr& session : state.session_state) {
    base::DictValue session_value;
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
    MediaRouterInternalsWebUIMessageHandler(MediaRouter* router,
                                            MediaRouterDebugger& debugger)
    : router_(router), debugger_(debugger) {
  DCHECK(router_);
  debugger_->AddObserver(*this);
  if (router_->GetLogger()) {
    router_->GetLogger()->AddObserver(this);
  }
}

MediaRouterInternalsWebUIMessageHandler::
    ~MediaRouterInternalsWebUIMessageHandler() {
  debugger_->RemoveObserver(*this);
  if (router_->GetLogger()) {
    router_->GetLogger()->RemoveObserver(this);
  }
}

void MediaRouterInternalsWebUIMessageHandler::OnLogAdded(
    const LoggerImpl::Entry& entry) {
  if (IsJavascriptAllowed()) {
    FireWebUIListener("on-log-added", LoggerImpl::AsValue(entry));
  }
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
  web_ui()->RegisterMessageCallback(
      "startTracing",
      base::BindRepeating(
          &MediaRouterInternalsWebUIMessageHandler::HandleStartTracing,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "stopTracing",
      base::BindRepeating(
          &MediaRouterInternalsWebUIMessageHandler::HandleStopTracing,
          base::Unretained(this)));
}

void MediaRouterInternalsWebUIMessageHandler::HandleGetState(
    const base::ListValue& args) {
  AllowJavascript();
  const base::Value& callback_id = args[0];
  ResolveJavascriptCallback(callback_id, router_->GetState());
}

void MediaRouterInternalsWebUIMessageHandler::HandleGetProviderState(
    const base::ListValue& args) {
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
    const base::ListValue& args) {
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
    const base::ListValue& args) {
  AllowJavascript();
  const base::Value& callback_id = args[0];
  ResolveJavascriptCallback(callback_id, debugger_->GetMirroringStats());
}

void MediaRouterInternalsWebUIMessageHandler::HandleSetMirroringStatsEnabled(
    const base::ListValue& args) {
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
    const base::ListValue& args) {
  AllowJavascript();
  const base::Value& callback_id = args[0];

  ResolveJavascriptCallback(callback_id,
                            debugger_->ShouldFetchMirroringStats());
}

void MediaRouterInternalsWebUIMessageHandler::HandleStartTracing(
    const base::ListValue& args) {
  AllowJavascript();
  const base::Value& callback_id = args[0];

  if (tracing_session_) {
    ResolveJavascriptCallback(callback_id, base::Value(false));
    return;
  }

  const base::trace_event::TraceConfig trace_config(
      "media.cast,openscreen,gpu,media,base,toplevel", "record-until-full");

  tracing_session_ =
      perfetto::Tracing::NewTrace(perfetto::BackendType::kCustomBackend);

  auto perfetto_config = tracing::GetDefaultPerfettoConfig(
      trace_config, /*privacy_filtering_enabled=*/false);
  tracing_session_->Setup(perfetto_config);

  // base::Value is move-only, but std::function requires copyable arguments.
  // Since we only need the string callback_id, we can extract it.
  std::string callback_id_str;
  if (callback_id.is_string()) {
    callback_id_str = callback_id.GetString();
  }

  tracing_session_->SetOnStartCallback([weak_this = weak_factory_.GetWeakPtr(),
                                        callback_id_str]() {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](base::WeakPtr<MediaRouterInternalsWebUIMessageHandler> weak_this,
               std::string callback_id_str) {
              if (weak_this) {
                weak_this->ResolveJavascriptCallback(
                    base::Value(callback_id_str), base::Value(true));
              }
            },
            std::move(weak_this), std::move(callback_id_str)));
  });
  tracing_session_->Start();
}

void MediaRouterInternalsWebUIMessageHandler::HandleStopTracing(
    const base::ListValue& args) {
  AllowJavascript();
  const base::Value& callback_id = args[0];

  if (!tracing_session_) {
    ResolveJavascriptCallback(callback_id, base::Value(false));
    return;
  }

  std::string callback_id_str;
  if (callback_id.is_string()) {
    callback_id_str = callback_id.GetString();
  }

  // Wrap the tracing session in a ref-counted struct so it can be safely
  // captured by copy into the std::function callbacks used by Perfetto,
  // matching Chromium's idiomatic approach for these APIs.
  auto trace_reader =
      base::MakeRefCounted<TraceReader>(std::move(tracing_session_));

  trace_reader->session->SetOnStopCallback([trace_reader,
                                            weak_this =
                                                weak_factory_.GetWeakPtr(),
                                            callback_id_str]() {
    trace_reader->session->SetOnStopCallback([]() {});
    trace_reader->session->ReadTrace(
        [trace_reader, weak_this, callback_id_str](
            perfetto::TracingSession::ReadTraceCallbackArgs args) {
          if (args.size > 0) {
            std::string base64_chunk =
                base::Base64Encode(std::string_view(args.data, args.size));
            content::GetUIThreadTaskRunner({})->PostTask(
                FROM_HERE,
                base::BindOnce(
                    [](base::WeakPtr<MediaRouterInternalsWebUIMessageHandler>
                           weak_this,
                       std::string base64_chunk) {
                      if (weak_this) {
                        weak_this->FireWebUIListener("on-trace-chunk",
                                                     base::Value(base64_chunk));
                      }
                    },
                    weak_this, std::move(base64_chunk)));
          }

          if (args.has_more) {
            return;
          }

          content::GetUIThreadTaskRunner({})->PostTask(
              FROM_HERE,
              base::BindOnce(
                  [](base::WeakPtr<MediaRouterInternalsWebUIMessageHandler>
                         weak_this,
                     std::string callback_id_str) {
                    if (weak_this) {
                      weak_this->ResolveJavascriptCallback(
                          base::Value(callback_id_str), base::Value(true));
                    }
                  },
                  std::move(weak_this), std::move(callback_id_str)));
        });
  });
  trace_reader->session->Stop();
}

void MediaRouterInternalsWebUIMessageHandler::OnMirroringStatsUpdated(
    const base::DictValue& json_logs) {
  if (IsJavascriptAllowed()) {
    FireWebUIListener("on-mirroring-stats-update", std::move(json_logs));
  }
}

}  // namespace media_router
