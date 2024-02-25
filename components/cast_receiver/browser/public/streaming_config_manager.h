// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_RECEIVER_BROWSER_PUBLIC_STREAMING_CONFIG_MANAGER_H_
#define COMPONENTS_CAST_RECEIVER_BROWSER_PUBLIC_STREAMING_CONFIG_MANAGER_H_

#include <optional>

#include "base/check.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "components/cast_streaming/browser/public/receiver_config.h"

namespace cast_receiver {

// Handles access of the streaming config.
class StreamingConfigManager {
 public:
  // Observer for changes in the streaming config.
  class ConfigObserver : public base::CheckedObserver {
   public:
    ~ConfigObserver() override;

    // Called when the associated window is shown.
    virtual void OnStreamingConfigSet(
        const cast_streaming::ReceiverConfig& config) = 0;
  };

  StreamingConfigManager();
  virtual ~StreamingConfigManager();

  // Returns whether any config has been received thus far.
  bool has_config() const { return config_.has_value(); }

  // Returns the current config. May only be called if |has_config()| is true.
  const cast_streaming::ReceiverConfig& config() const {
    DCHECK(has_config());
    return config_.value();
  }

  // Adds or removes a ConfigObserver. A callback will be fired for all future
  // updates of the receiver config.
  void AddConfigObserver(ConfigObserver& observer);
  void RemoveConfigbserver(ConfigObserver& observer);

 protected:
  // Called when the streaming config has been set or changed.
  void OnStreamingConfigSet(cast_streaming::ReceiverConfig config);

 private:
  std::optional<cast_streaming::ReceiverConfig> config_ = std::nullopt;

  base::ObserverList<ConfigObserver> observers_;
};

}  // namespace cast_receiver

#endif  // COMPONENTS_CAST_RECEIVER_BROWSER_PUBLIC_STREAMING_CONFIG_MANAGER_H_
