// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_RUNTIME_BROWSER_CORE_STREAMING_CONFIG_MANAGER_H_
#define CHROMECAST_CAST_CORE_RUNTIME_BROWSER_CORE_STREAMING_CONFIG_MANAGER_H_

#include <memory>
#include <string_view>

#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "components/cast/message_port/message_port.h"
#include "components/cast_receiver/browser/public/runtime_application.h"
#include "components/cast_receiver/browser/public/streaming_config_manager.h"

namespace cast_receiver {
class MessagePortService;
}  // namespace cast_receiver

namespace chromecast {

// This class implements retrieving of a cast_streaming::RuntimeConfig from an
// AV Settings MessagePort query.
class CoreStreamingConfigManager
    : public cast_receiver::StreamingConfigManager,
      public cast_api_bindings::MessagePort::Receiver {
 public:
  // Creates a new instance of this class. |message_port_service| must persist
  // for the lifetime of this instance.
  CoreStreamingConfigManager(
      cast_receiver::MessagePortService& message_port_service,
      cast_receiver::RuntimeApplication::StatusCallback error_cb);
  ~CoreStreamingConfigManager() override;

 private:
  friend class CoreStreamingConfigManagerTest;

  // Internal ctor that does not query for AV Settings. Intended to be used only
  // for testing purposes.
  CoreStreamingConfigManager(
      cast_receiver::RuntimeApplication::StatusCallback error_cb);

  // cast_api_bindings::MessagePort::Receiver overrides.
  bool OnMessage(std::string_view message,
                 std::vector<std::unique_ptr<cast_api_bindings::MessagePort>>
                     ports) override;
  void OnPipeError() override;

  // MessagePort responsible for receiving AV Settings Bindings Messages.
  std::unique_ptr<cast_api_bindings::MessagePort> message_port_;

  cast_receiver::RuntimeApplication::StatusCallback error_callback_;
};

}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_RUNTIME_BROWSER_CORE_STREAMING_CONFIG_MANAGER_H_
