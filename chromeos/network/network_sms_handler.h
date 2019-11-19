// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_NETWORK_SMS_HANDLER_H_
#define CHROMEOS_NETWORK_NETWORK_SMS_HANDLER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "chromeos/dbus/shill/shill_property_changed_observer.h"

namespace base {
class DictionaryValue;
class ListValue;
class Value;
}

namespace chromeos {

// Class to watch sms without Libcros.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) NetworkSmsHandler
    : public ShillPropertyChangedObserver {
 public:
  static const char kNumberKey[];
  static const char kTextKey[];
  static const char kTimestampKey[];

  class Observer {
   public:
    virtual ~Observer() {}

    // Called when a new message arrives. |message| contains the message.
    // The contents of the dictionary include the keys listed above.
    virtual void MessageReceived(const base::DictionaryValue& message) = 0;
  };

  ~NetworkSmsHandler() override;

  // Requests an immediate check for new messages. If |request_existing| is
  // true then also requests to be notified for any already received messages.
  void RequestUpdate(bool request_existing);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // ShillPropertyChangedObserver
  void OnPropertyChanged(const std::string& name,
                         const base::Value& value) override;

 private:
  friend class NetworkHandler;
  friend class NetworkSmsHandlerTest;

  class NetworkSmsDeviceHandler;
  class ModemManagerNetworkSmsDeviceHandler;
  class ModemManager1NetworkSmsDeviceHandler;

  NetworkSmsHandler();

  // Requests the devices from the network manager, sets up observers, and
  // requests the initial list of messages.
  void Init();

  // Adds |message| to the list of received messages. If the length of the
  // list exceeds the maximum number of retained messages, erase the least
  // recently received message.
  void AddReceivedMessage(const base::DictionaryValue& message);

  // Notify observers that |message| was received.
  void NotifyMessageReceived(const base::DictionaryValue& message);

    // Called from NetworkSmsDeviceHandler when a message is received.
  void MessageReceived(const base::DictionaryValue& message);

  // Callback to handle the manager properties with the list of devices.
  void ManagerPropertiesCallback(DBusMethodCallStatus call_status,
                                 const base::DictionaryValue& properties);

  // Requests properties for each entry in |devices|.
  void UpdateDevices(const base::ListValue* devices);

  // Callback to handle the device properties for |device_path|.
  // A NetworkSmsDeviceHandler will be instantiated for each cellular device.
  void DevicePropertiesCallback(const std::string& device_path,
                                DBusMethodCallStatus call_status,
                                const base::DictionaryValue& properties);

  base::ObserverList<Observer, true>::Unchecked observers_;
  std::vector<std::unique_ptr<NetworkSmsDeviceHandler>> device_handlers_;
  std::vector<std::unique_ptr<base::DictionaryValue>> received_messages_;
  base::WeakPtrFactory<NetworkSmsHandler> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(NetworkSmsHandler);
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_NETWORK_SMS_HANDLER_H_
