// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_TEXT_MESSAGE_PROVIDER_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_TEXT_MESSAGE_PROVIDER_H_

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_sms_handler.h"

namespace ash {

// Provides non-suppressed text messages to its listeners.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) TextMessageProvider
    : NetworkSmsHandler::Observer {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    // Called when a new message arrives.
    virtual void MessageReceived(const TextMessageData& message_data) {}
  };

  TextMessageProvider();
  TextMessageProvider(const TextMessageProvider&) = delete;
  TextMessageProvider& operator=(const TextMessageProvider&) = delete;
  ~TextMessageProvider() override;

  // NetworkSmsHandler::Observer:
  void MessageReceivedFromNetwork(const std::string& guid,
                                  const TextMessageData& message_data) override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void Init(NetworkSmsHandler* network_sms_handler);

 private:
  bool ShouldAllowTextMessages(const std::string& guid);
  base::ScopedObservation<NetworkSmsHandler, NetworkSmsHandler::Observer>
      network_sms_handler_observer_{this};

  base::ObserverList<TextMessageProvider::Observer> observers_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_TEXT_MESSAGE_PROVIDER_H_
