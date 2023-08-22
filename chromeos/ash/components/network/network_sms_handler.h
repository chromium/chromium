// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_SMS_HANDLER_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_SMS_HANDLER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/shill/shill_property_changed_observer.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/dbus/common/dbus_method_call_status.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

// Struct used for passing around text message data like number, text and
// timestamp.
struct COMPONENT_EXPORT(CHROMEOS_NETWORK) TextMessageData {
  TextMessageData(absl::optional<const std::string> number,
                  absl::optional<const std::string> text,
                  absl::optional<const std::string> timestamp);
  TextMessageData(TextMessageData&& other);
  TextMessageData& operator=(TextMessageData&& other);
  ~TextMessageData();

  absl::optional<std::string> number;
  absl::optional<std::string> text;
  absl::optional<std::string> timestamp;
};

// Class to watch sms without Libcros.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) NetworkSmsHandler
    : public ShillPropertyChangedObserver {
 public:
  static const char kNumberKey[];
  static const char kTextKey[];
  static const char kTimestampKey[];

  class Observer {
   public:
    virtual ~Observer() = default;

    // Called when a new message arrives. |message| contains the message which
    // is a dictionary value containing entries for kNumberKey, kTextKey, and
    // kTimestampKey.
    virtual void MessageReceived(const base::Value::Dict& message) {}

    // Called when a new message arrives from a network with |guid|.
    virtual void MessageReceivedFromNetwork(const std::string& guid,
                                            const TextMessageData& message) {}
  };

  NetworkSmsHandler(const NetworkSmsHandler&) = delete;
  NetworkSmsHandler& operator=(const NetworkSmsHandler&) = delete;

  ~NetworkSmsHandler() override;

  // Requests an immediate check for new messages.
  void RequestUpdate();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // ShillPropertyChangedObserver
  void OnPropertyChanged(const std::string& name,
                         const base::Value& value) override;

 private:
  friend class NetworkHandler;
  friend class NetworkSmsHandlerTest;
  friend class TextMessageProviderTest;

  class NetworkSmsDeviceHandler;
  class ModemManagerNetworkSmsDeviceHandler;
  class ModemManager1NetworkSmsDeviceHandler;

  // Timeout for waiting to fetch SMS details.
  static const base::TimeDelta kFetchSmsDetailsTimeout;

  NetworkSmsHandler();

  // Requests the devices from the network manager, sets up observers, and
  // requests the initial list of messages.
  void Init();

  // Adds |message| to the list of received messages. If the length of the
  // list exceeds the maximum number of retained messages, erase the least
  // recently received message.
  void AddReceivedMessage(const base::Value::Dict& message);

  // Notify observers that |message| was received.
  void NotifyMessageReceived(const base::Value::Dict& message);

  // Called from NetworkSmsDeviceHandler when a message is received.
  void MessageReceived(const base::Value::Dict& message);

  // Callback to handle the manager properties with the list of devices.
  void ManagerPropertiesCallback(absl::optional<base::Value::Dict> properties);

  // Requests properties for each entry in |devices|.
  void UpdateDevices(const base::Value::List& devices);

  // Callback to handle the device properties for |device_path|.
  // A NetworkSmsDeviceHandler will be instantiated for each cellular device.
  void DevicePropertiesCallback(const std::string& device_path,
                                absl::optional<base::Value::Dict> properties);

  // Called when the cellular device's object path changes. This means that
  // there has been an update to the device's SIM (removed or inserted) and that
  // a new handler should be created for the device's new object path.
  void OnObjectPathChanged(const base::Value& object_path);

  base::ObserverList<Observer, true>::Unchecked observers_;
  std::unique_ptr<NetworkSmsDeviceHandler> device_handler_;
  std::vector<base::Value::Dict> received_messages_;
  std::string cellular_device_path_;
  base::WeakPtrFactory<NetworkSmsHandler> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_SMS_HANDLER_H_
