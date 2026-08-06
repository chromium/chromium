// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/internal/browser_actuator_service_impl.h"

#include "base/feature_list.h"
#include "components/browser_actuator/internal/features.h"
#include "components/browser_actuator/internal/transport_channel_impl.h"
#include "components/browser_actuator/public/transport_session_registry.h"

namespace browser_actuator {

BrowserActuatorServiceImpl::BrowserActuatorServiceImpl() {
  if (base::FeatureList::IsEnabled(kBrowserActuatorChannelEnabled)) {
    // TODO(crbug.com/532660606): Pass in the StreamClientFactory used to
    // establish physical network connections here
    channel_ = std::make_unique<TransportChannelImpl>();
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
