// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_ACTUATOR_INTERNAL_CONTROL_TRANSPORT_HANDLER_H_
#define COMPONENTS_BROWSER_ACTUATOR_INTERNAL_CONTROL_TRANSPORT_HANDLER_H_

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "components/browser_actuator/public/common.h"
#include "components/browser_actuator/public/transport_handler.h"
#include "components/browser_actuator/public/transport_handler_factory.h"

namespace browser_actuator {

// Implementation of TransportHandler specifically for processing channel and
// session control commands (PayloadType::kControl).
class ControlTransportHandler : public TransportHandler {
 public:
  using CloseChannelCallback = base::RepeatingClosure;
  using CloseSessionCallback = base::RepeatingCallback<void(std::string_view)>;

  ControlTransportHandler(std::string_view session_id,
                          CloseChannelCallback close_channel_cb,
                          CloseSessionCallback close_session_cb);
  ~ControlTransportHandler() override;

  ControlTransportHandler(const ControlTransportHandler&) = delete;
  ControlTransportHandler& operator=(const ControlTransportHandler&) = delete;

  // TransportHandler implementation:
  void OnMessage(std::string_view payload) override;

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  const std::string session_id_;
  CloseChannelCallback close_channel_cb_ GUARDED_BY_CONTEXT(sequence_checker_);
  CloseSessionCallback close_session_cb_ GUARDED_BY_CONTEXT(sequence_checker_);
};

// Factory for creating ControlTransportHandler instances for sessions.
class ControlTransportHandlerFactory : public TransportHandlerFactory {
 public:
  ControlTransportHandlerFactory(
      ControlTransportHandler::CloseChannelCallback close_channel_cb,
      ControlTransportHandler::CloseSessionCallback close_session_cb);
  ~ControlTransportHandlerFactory() override;

  ControlTransportHandlerFactory(const ControlTransportHandlerFactory&) =
      delete;
  ControlTransportHandlerFactory& operator=(
      const ControlTransportHandlerFactory&) = delete;

  // TransportHandlerFactory implementation:
  FactoryId GetFactoryId() const override;
  std::vector<PayloadType> GetSupportedPayloadTypes() const override;
  std::unique_ptr<TransportHandler> OnNewSession(
      TransportSession* session) override;

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  ControlTransportHandler::CloseChannelCallback close_channel_cb_
      GUARDED_BY_CONTEXT(sequence_checker_);
  ControlTransportHandler::CloseSessionCallback close_session_cb_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace browser_actuator

#endif  // COMPONENTS_BROWSER_ACTUATOR_INTERNAL_CONTROL_TRANSPORT_HANDLER_H_
