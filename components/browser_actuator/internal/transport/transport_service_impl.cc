// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/internal/transport/transport_service_impl.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "components/browser_actuator/internal/transport/features.h"
#include "components/browser_actuator/internal/transport/message_stream_client.h"
#include "components/browser_actuator/internal/transport/oauth/oauth_stream_connection_delegate.h"
#include "components/browser_actuator/internal/transport/proto_stream_client/proto_stream_client.h"
#include "components/browser_actuator/internal/transport/proto_stream_client/rust_stream_framer.h"
#include "components/browser_actuator/internal/transport/session_stream_recorder.h"
#include "components/browser_actuator/internal/transport/stream_connection_delegate.h"
#include "components/browser_actuator/internal/transport/transport_channel_impl.h"
#include "components/browser_actuator/internal/transport/upstream_message_client/upstream_message_client.h"
#include "components/browser_actuator/public/transport/transport_handler_factory_registry.h"
#include "components/browser_actuator/public/transport/transport_session_registry.h"
#include "components/signin/public/base/oauth_consumer_id.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace browser_actuator::transport {

namespace {

net::NetworkTrafficAnnotationTag GetTrafficAnnotation() {
  return net::DefineNetworkTrafficAnnotation("browser_actuator_transport", R"(
    semantics {
      sender: "Browser Actuator Service"
      description:
        "Transfers browser agent commands, execution results, page state, "
        "and screenshots between Chrome and Google servers. This allows "
        "Chrome to receive and execute automated browsing actions requested "
        "by the user, and report execution status, annotated page context, "
        "and tab state back to the servers."
      trigger:
        "Triggered when a signed-in user initiates an automated task or "
        "workflow from the Gemini App or assistant surface that delegates "
        "actions to Chrome on their device, after an explicit device opt-in "
        "dialog for the first query."
      data:
        "OAuth2 authentication token, current webpage URLs, annotated page "
        "context (DOM elements and text), screenshots of the tab, and "
        "action execution results or status."
      destination: GOOGLE_OWNED_SERVICE
      internal {
        contacts {
          email: "chrome-agents-team@google.com"
        }
      }
      user_data {
        type: ACCESS_TOKEN
        type: SENSITIVE_URL
        type: USER_CONTENT
        type: WEB_CONTENT
        type: IMAGE
      }
      last_reviewed: "2026-08-25"
    }
    policy {
      cookies_allowed: NO
      setting:
        "Feature not launched and disabled by default. "
        "TODO(crbug.com/552478160) to create an explicit setting to toggle the "
        "browser actuator service"
      chrome_policy {
        # TODO(crbug.com/552478160): Add a feature-specific enterprise policy
        # once implemented.
        GenAiDefaultSettings {
          GenAiDefaultSettings: 2
        }
        GeminiSettings {
          GeminiSettings: 1
        }
      }
    })");
}

std::unique_ptr<MessageStreamClient> CreateStreamClient(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager,
    GURL endpoint,
    std::unique_ptr<StreamConnectionDelegate> resume_delegate) {
  return std::make_unique<ProtoStreamClient>(
      std::move(url_loader_factory), std::move(endpoint),
      std::make_unique<OAuthStreamConnectionDelegate>(
          std::move(resume_delegate), identity_manager,
          signin::OAuthConsumerId::kBrowserActuator),
      RustStreamFramer::MakeFactory(), GetTrafficAnnotation());
}

}  // namespace

TransportServiceConfig::TransportServiceConfig() = default;
TransportServiceConfig::TransportServiceConfig(const TransportServiceConfig&) =
    default;
TransportServiceConfig& TransportServiceConfig::operator=(
    const TransportServiceConfig&) = default;
TransportServiceConfig::TransportServiceConfig(TransportServiceConfig&&) =
    default;
TransportServiceConfig& TransportServiceConfig::operator=(
    TransportServiceConfig&&) = default;
TransportServiceConfig::~TransportServiceConfig() = default;

TransportServiceImpl::TransportServiceImpl(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager,
    TransportServiceConfig config) {
  if (base::FeatureList::IsEnabled(kBrowserActuatorChannelEnabled)) {
    channel_ = std::make_unique<TransportChannelImpl>(
        std::make_unique<UpstreamMessageClient>(
            url_loader_factory, identity_manager,
            std::move(config.send_session_message_endpoint),
            GetTrafficAnnotation()),
        // Safe because `TransportServiceFactory` depends on
        // `IdentityManagerFactory`, so `identity_manager` outlives `channel_`.
        base::BindOnce(&CreateStreamClient, url_loader_factory,
                       base::Unretained(identity_manager),
                       std::move(config.watch_sessions_endpoint)));
  }

  // The internals page is the only consumer of the recorded session history,
  // so the embedder only asks for recording while that page is enabled. This
  // keeps the memory cost at zero for regular users.
  if (config.record_sessions) {
    session_stream_recorder_factory_ =
        std::make_unique<SessionStreamRecorderFactory>();
  }

  // The channel is only created when the channel feature is on. When it is off
  // the recorder factory is still owned here, but there is nothing to register
  // with.
  if (session_stream_recorder_factory_ && channel_) {
    if (TransportHandlerFactoryRegistry* registry =
            channel_->GetHandlerFactoryRegistry()) {
      registry->RegisterFactory(session_stream_recorder_factory_.get());
    }
    channel_->AddObserver(session_stream_recorder_factory_.get());
  }
}

TransportServiceImpl::~TransportServiceImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!channel_) {
    return;
  }
  // Unregister before anything is destroyed. The registry and channel store
  // raw pointers and do not own the factory.
  if (session_stream_recorder_factory_) {
    if (TransportHandlerFactoryRegistry* registry =
            channel_->GetHandlerFactoryRegistry()) {
      registry->UnregisterFactory(session_stream_recorder_factory_.get());
    }
    channel_->RemoveObserver(session_stream_recorder_factory_.get());
  }
}

TransportChannel* TransportServiceImpl::GetChannel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return channel_.get();
}

SessionStreamRecorderFactory* TransportServiceImpl::GetStreamRecorderFactory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return session_stream_recorder_factory_.get();
}

TransportSession* TransportServiceImpl::CreateSession(
    std::string_view session_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TransportChannel* channel = GetChannel();
  if (channel && channel->GetSessionRegistry()) {
    return channel->GetSessionRegistry()->CreateSession(session_id);
  }
  return nullptr;
}

TransportSession* TransportServiceImpl::GetSession(
    std::string_view session_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TransportChannel* channel = GetChannel();
  if (channel && channel->GetSessionRegistry()) {
    return channel->GetSessionRegistry()->GetSession(session_id);
  }
  return nullptr;
}

}  // namespace browser_actuator::transport
