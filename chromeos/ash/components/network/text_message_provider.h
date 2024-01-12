// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_TEXT_MESSAGE_PROVIDER_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_TEXT_MESSAGE_PROVIDER_H_

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_metadata_store.h"
#include "chromeos/ash/components/network/network_policy_observer.h"
#include "chromeos/ash/components/network/network_sms_handler.h"
#include "chromeos/ash/components/network/text_message_suppression_state.h"

namespace ash {

// Provides non-suppressed text messages to its listeners.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) TextMessageProvider
    : NetworkSmsHandler::Observer,
      NetworkPolicyObserver {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    // Called when a new message arrives.
    virtual void MessageReceived(const std::string& guid,
                                 const TextMessageData& message_data) {}
  };

  TextMessageProvider();
  TextMessageProvider(const TextMessageProvider&) = delete;
  TextMessageProvider& operator=(const TextMessageProvider&) = delete;
  ~TextMessageProvider() override;

  // NetworkSmsHandler::Observer:
  void MessageReceivedFromNetwork(const std::string& guid,
                                  const TextMessageData& message_data) override;

  // NetworkPolicyObserver:
  void PoliciesChanged(const std::string& userhash) override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void Init(NetworkSmsHandler* network_sms_handler,
            ManagedNetworkConfigurationHandler*
                managed_network_configuration_handler);
  void SetNetworkMetadataStore(NetworkMetadataStore* network_metadata_store);

  // Logs the success metrics for the AllowTextMessage feature. Called after a
  // notification is received.
  void LogTextMessageNotificationMetrics(const std::string& guid);

 private:
  friend class TextMessageProviderTest;

  bool ShouldAllowTextMessages(const std::string& guid);
  bool IsAllowTextMessagesPolicySet();
  bool IsMessageSuppressedByUser(const std::string& guid);

  std::optional<PolicyTextMessageSuppressionState> policy_suppression_state_;

  base::ScopedObservation<NetworkSmsHandler, NetworkSmsHandler::Observer>
      network_sms_handler_observer_{this};

  base::ScopedObservation<ManagedNetworkConfigurationHandler,
                          NetworkPolicyObserver>
      network_policy_observer_{this};

  raw_ptr<ManagedNetworkConfigurationHandler>
      managed_network_configuration_handler_ = nullptr;

  raw_ptr<NetworkMetadataStore, DanglingUntriaged> network_metadata_store_ =
      nullptr;

  base::ObserverList<TextMessageProvider::Observer> observers_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_TEXT_MESSAGE_PROVIDER_H_
