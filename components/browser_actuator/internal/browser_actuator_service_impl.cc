// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/internal/browser_actuator_service_impl.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "components/browser_actuator/internal/features.h"
#include "components/browser_actuator/internal/session_stream_recorder.h"
#include "components/browser_actuator/internal/transport/message_stream_client.h"
#include "components/browser_actuator/internal/transport/proto_stream_client/proto_stream_client.h"
#include "components/browser_actuator/internal/transport/proto_stream_client/rust_stream_framer.h"
#include "components/browser_actuator/internal/transport/stream_connection_delegate.h"
#include "components/browser_actuator/internal/transport/upstream_message_client/upstream_message_client.h"
#include "components/browser_actuator/internal/transport_channel_impl.h"
#include "components/browser_actuator/public/common.h"
#include "components/browser_actuator/public/features.h"
#include "components/browser_actuator/public/transport_handler_factory.h"
#include "components/browser_actuator/public/transport_handler_factory_registry.h"
#include "components/browser_actuator/public/transport_session_registry.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace browser_actuator {

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
    std::unique_ptr<StreamConnectionDelegate> resume_delegate) {
  GURL endpoint = GetWatchSessionsEndPoint();
  return std::make_unique<ProtoStreamClient>(
      std::move(url_loader_factory), std::move(endpoint),
      std::move(resume_delegate), RustStreamFramer::MakeFactory(),
      GetTrafficAnnotation());
}

}  // namespace

BrowserActuatorServiceImpl::BrowserActuatorServiceImpl(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager,
    std::vector<std::unique_ptr<TransportHandlerFactory>> extra_factories)
    : extra_factories_(std::move(extra_factories)) {
  if (base::FeatureList::IsEnabled(kBrowserActuatorChannelEnabled)) {
    channel_ = std::make_unique<TransportChannelImpl>(
        std::make_unique<UpstreamMessageClient>(
            url_loader_factory, identity_manager,
            GetSendSessionMessageEndpoint(), GetTrafficAnnotation()),
        base::BindOnce(&CreateStreamClient, url_loader_factory));
  }

  // The internals page is the only consumer of the recorded session history,
  // so the recorder is created only when that page is enabled. This keeps the
  // memory cost at zero for regular users.
  if (base::FeatureList::IsEnabled(kBrowserActuatorInternals) &&
      !GetFactory(FactoryId::kSessionStreamRecorder)) {
    session_stream_recorder_factory_ =
        std::make_unique<SessionStreamRecorderFactory>();
  }

  // The channel is only created when the channel feature is on. When it is off
  // the factories are still owned here, but there is nothing to register with.
  TransportHandlerFactoryRegistry* registry =
      channel_ ? channel_->GetHandlerFactoryRegistry() : nullptr;
  if (session_stream_recorder_factory_) {
    if (registry) {
      registry->RegisterFactory(session_stream_recorder_factory_.get());
    }
    if (channel_) {
      channel_->AddObserver(session_stream_recorder_factory_.get());
    }
  }
  for (const auto& factory : extra_factories_) {
    CHECK(factory);
    if (registry) {
      registry->RegisterFactory(factory.get());
    }
  }
}

BrowserActuatorServiceImpl::~BrowserActuatorServiceImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!channel_) {
    return;
  }
  // Unregister before anything is destroyed. The registry and channel store
  // raw pointers and do not own the factories.
  TransportHandlerFactoryRegistry* registry =
      channel_->GetHandlerFactoryRegistry();
  for (const auto& factory : extra_factories_) {
    if (registry) {
      registry->UnregisterFactory(factory.get());
    }
  }
  if (session_stream_recorder_factory_) {
    if (registry) {
      registry->UnregisterFactory(session_stream_recorder_factory_.get());
    }
    channel_->RemoveObserver(session_stream_recorder_factory_.get());
  }
}

bool BrowserActuatorServiceImpl::IsInitialized() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return true;
}

TransportChannel* BrowserActuatorServiceImpl::GetChannel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return channel_.get();
}

TransportHandlerFactory* BrowserActuatorServiceImpl::GetFactory(FactoryId id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const auto& factory : extra_factories_) {
    if (factory->GetFactoryId() == id) {
      return factory.get();
    }
  }
  if (id == FactoryId::kSessionStreamRecorder) {
    return session_stream_recorder_factory_.get();
  }
  return nullptr;
}

TransportSession* BrowserActuatorServiceImpl::GetOrCreateSession(
    std::string_view session_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TransportChannel* channel = GetChannel();
  if (channel && channel->GetSessionRegistry()) {
    return channel->GetSessionRegistry()->GetOrCreateSession(session_id);
  }
  return nullptr;
}

TransportSession* BrowserActuatorServiceImpl::GetSession(
    std::string_view session_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TransportChannel* channel = GetChannel();
  if (channel && channel->GetSessionRegistry()) {
    return channel->GetSessionRegistry()->GetSession(session_id);
  }
  return nullptr;
}

}  // namespace browser_actuator
