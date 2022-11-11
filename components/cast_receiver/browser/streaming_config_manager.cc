// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_receiver/browser/public/streaming_config_manager.h"

namespace cast_receiver {

StreamingConfigManager::ConfigObserver::~ConfigObserver() = default;

StreamingConfigManager::StreamingConfigManager() = default;

StreamingConfigManager::~StreamingConfigManager() = default;

void StreamingConfigManager::AddConfigObserver(ConfigObserver& observer) {
  observers_.AddObserver(&observer);
}

void StreamingConfigManager::RemoveConfigbserver(ConfigObserver& observer) {
  observers_.RemoveObserver(&observer);
}

void StreamingConfigManager::OnStreamingConfigSet(
    cast_streaming::ReceiverConfig config) {
  config_ = std::move(config);
  for (auto& observer : observers_) {
    observer.OnStreamingConfigSet(config_.value());
  }
}

}  // namespace cast_receiver
