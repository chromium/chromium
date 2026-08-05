// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/internal/browser_actuator_service_impl.h"

#include "base/feature_list.h"
#include "components/browser_actuator/internal/features.h"
#include "components/browser_actuator/internal/transport_channel_impl.h"

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

}  // namespace browser_actuator
