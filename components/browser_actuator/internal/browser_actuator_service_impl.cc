// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/internal/browser_actuator_service_impl.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "components/browser_actuator/internal/features.h"
#include "components/browser_actuator/internal/transport/message_stream_client.h"
#include "components/browser_actuator/internal/transport/proto_stream_client/proto_stream_client.h"
#include "components/browser_actuator/internal/transport/proto_stream_client/rust_stream_framer.h"
#include "components/browser_actuator/internal/transport/stream_connection_delegate.h"
#include "components/browser_actuator/internal/transport/upstream_message_client/upstream_message_client.h"
#include "components/browser_actuator/internal/transport_channel_impl.h"
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
    signin::IdentityManager* identity_manager) {
  if (base::FeatureList::IsEnabled(kBrowserActuatorChannelEnabled)) {
    channel_ = std::make_unique<TransportChannelImpl>(
        std::make_unique<UpstreamMessageClient>(
            url_loader_factory, identity_manager,
            GetSendSessionMessageEndpoint(), GetTrafficAnnotation()),
        base::BindOnce(&CreateStreamClient, url_loader_factory));
  }
}

BrowserActuatorServiceImpl::~BrowserActuatorServiceImpl() = default;

bool BrowserActuatorServiceImpl::IsInitialized() const {
  return true;
}

TransportChannel* BrowserActuatorServiceImpl::GetChannel() {
  return channel_.get();
}

TransportSession* BrowserActuatorServiceImpl::GetOrCreateSession(
    std::string_view session_id) {
  TransportChannel* channel = GetChannel();
  if (channel && channel->GetSessionRegistry()) {
    return channel->GetSessionRegistry()->GetOrCreateSession(session_id);
  }
  return nullptr;
}

TransportSession* BrowserActuatorServiceImpl::GetSession(
    std::string_view session_id) {
  TransportChannel* channel = GetChannel();
  if (channel && channel->GetSessionRegistry()) {
    return channel->GetSessionRegistry()->GetSession(session_id);
  }
  return nullptr;
}

}  // namespace browser_actuator
