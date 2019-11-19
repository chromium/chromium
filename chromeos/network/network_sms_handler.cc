// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_sms_handler.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "base/values.h"
#include "chromeos/dbus/shill/modem_messaging_client.h"
#include "chromeos/dbus/shill/shill_device_client.h"
#include "chromeos/dbus/shill/shill_manager_client.h"
#include "chromeos/dbus/shill/sms_client.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace {

// Maximum number of messages stored for RequestUpdate(true).
const size_t kMaxReceivedMessages = 100;

}  // namespace

namespace chromeos {

// static
const char NetworkSmsHandler::kNumberKey[] = "number";
const char NetworkSmsHandler::kTextKey[] = "text";
const char NetworkSmsHandler::kTimestampKey[] = "timestamp";

class NetworkSmsHandler::NetworkSmsDeviceHandler {
 public:
  NetworkSmsDeviceHandler() = default;
  virtual ~NetworkSmsDeviceHandler() = default;

  virtual void RequestUpdate() = 0;
};

class NetworkSmsHandler::ModemManager1NetworkSmsDeviceHandler
    : public NetworkSmsHandler::NetworkSmsDeviceHandler {
 public:
  ModemManager1NetworkSmsDeviceHandler(NetworkSmsHandler* host,
                                       const std::string& service_name,
                                       const dbus::ObjectPath& object_path);

  void RequestUpdate() override;

 private:
  void ListCallback(base::Optional<std::vector<dbus::ObjectPath>> paths);
  void SmsReceivedCallback(const dbus::ObjectPath& path, bool complete);
  void GetCallback(const base::DictionaryValue& dictionary);
  void DeleteMessages();
  void DeleteCallback(bool success);
  void GetMessages();
  void MessageReceived(const base::DictionaryValue& dictionary);

  NetworkSmsHandler* host_;
  std::string service_name_;
  dbus::ObjectPath object_path_;
  bool deleting_messages_;
  bool retrieving_messages_;
  std::vector<dbus::ObjectPath> delete_queue_;
  base::circular_deque<dbus::ObjectPath> retrieval_queue_;
  base::WeakPtrFactory<ModemManager1NetworkSmsDeviceHandler> weak_ptr_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(ModemManager1NetworkSmsDeviceHandler);
};

NetworkSmsHandler::ModemManager1NetworkSmsDeviceHandler::
    ModemManager1NetworkSmsDeviceHandler(NetworkSmsHandler* host,
                                         const std::string& service_name,
                                         const dbus::ObjectPath& object_path)
    : host_(host),
      service_name_(service_name),
      object_path_(object_path),
      deleting_messages_(false),
      retrieving_messages_(false) {
  // Set the handler for received Sms messaages.
  ModemMessagingClient::Get()->SetSmsReceivedHandler(
      service_name_, object_path_,
      base::Bind(&NetworkSmsHandler::ModemManager1NetworkSmsDeviceHandler::
                     SmsReceivedCallback,
                 weak_ptr_factory_.GetWeakPtr()));

  // List the existing messages.
  ModemMessagingClient::Get()->List(
      service_name_, object_path_,
      base::BindOnce(&NetworkSmsHandler::ModemManager1NetworkSmsDeviceHandler::
                         ListCallback,
                     weak_ptr_factory_.GetWeakPtr()));
}

void NetworkSmsHandler::ModemManager1NetworkSmsDeviceHandler::RequestUpdate() {
  // Calling List using the service "AddSMS" causes the stub
  // implementation to deliver new sms messages.
  ModemMessagingClient::Get()->List(
      std::string("AddSMS"), dbus::ObjectPath("/"),
      base::BindOnce(&NetworkSmsHandler::ModemManager1NetworkSmsDeviceHandler::
                         ListCallback,
                     weak_ptr_factory_.GetWeakPtr()));
}

void NetworkSmsHandler::ModemManager1NetworkSmsDeviceHandler::ListCallback(
    base::Optional<std::vector<dbus::ObjectPath>> paths) {
  // This receives all messages, so clear any pending gets and deletes.
  retrieval_queue_.clear();
  delete_queue_.clear();

  if (!paths.has_value())
    return;

  retrieval_queue_.reserve(paths->size());
  retrieval_queue_.assign(std::make_move_iterator(paths->begin()),
                          std::make_move_iterator(paths->end()));
  if (!retrieving_messages_)
    GetMessages();
}

// Messages must be deleted one at a time, since we can not guarantee
// the order the deletion will be executed in. Delete messages from
// the back of the list so that the indices are valid.
void NetworkSmsHandler::ModemManager1NetworkSmsDeviceHandler::DeleteMessages() {
  if (delete_queue_.empty()) {
    deleting_messages_ = false;
    return;
  }
  deleting_messages_ = true;
  dbus::ObjectPath sms_path = std::move(delete_queue_.back());
  delete_queue_.pop_back();
  ModemMessagingClient::Get()->Delete(
      service_name_, object_path_, sms_path,
      base::BindOnce(&NetworkSmsHandler::ModemManager1NetworkSmsDeviceHandler::
                         DeleteCallback,
                     weak_ptr_factory_.GetWeakPtr()));
}

void NetworkSmsHandler::ModemManager1NetworkSmsDeviceHandler::DeleteCallback(
    bool success) {
  if (!success)
    return;
  DeleteMessages();
}

// Messages must be fetched one at a time, so that we do not queue too
// many requests to a single threaded server.
void NetworkSmsHandler::ModemManager1NetworkSmsDeviceHandler::GetMessages() {
  if (retrieval_queue_.empty()) {
    retrieving_messages_ = false;
    if (!deleting_messages_)
      DeleteMessages();
    return;
  }
  retrieving_messages_ = true;
  dbus::ObjectPath sms_path = retrieval_queue_.front();
  retrieval_queue_.pop_front();
  SMSClient::Get()->GetAll(
      service_name_, sms_path,
      base::BindOnce(
          &NetworkSmsHandler::ModemManager1NetworkSmsDeviceHandler::GetCallback,
          weak_ptr_factory_.GetWeakPtr()));
  delete_queue_.push_back(sms_path);
}

void NetworkSmsHandler::
ModemManager1NetworkSmsDeviceHandler::SmsReceivedCallback(
    const dbus::ObjectPath& sms_path,
    bool complete) {
  // Only handle complete messages.
  if (!complete)
    return;
  retrieval_queue_.push_back(sms_path);
  if (!retrieving_messages_)
    GetMessages();
}

void NetworkSmsHandler::ModemManager1NetworkSmsDeviceHandler::GetCallback(
    const base::DictionaryValue& dictionary) {
  MessageReceived(dictionary);
  GetMessages();
}

void NetworkSmsHandler::
ModemManager1NetworkSmsDeviceHandler::MessageReceived(
    const base::DictionaryValue& dictionary) {
  // The keys of the ModemManager1.SMS interface do not match the
  // exported keys, so a new dictionary is created with the expected
  // key namaes.
  base::DictionaryValue new_dictionary;
  std::string text, number, timestamp;
  if (dictionary.GetStringWithoutPathExpansion(SMSClient::kSMSPropertyNumber,
                                               &number))
    new_dictionary.SetString(kNumberKey, number);
  if (dictionary.GetStringWithoutPathExpansion(SMSClient::kSMSPropertyText,
                                               &text))
    new_dictionary.SetString(kTextKey, text);
  // TODO(jglasgow): consider normalizing timestamp.
  if (dictionary.GetStringWithoutPathExpansion(SMSClient::kSMSPropertyTimestamp,
                                               &timestamp))
    new_dictionary.SetString(kTimestampKey, timestamp);
  host_->MessageReceived(new_dictionary);
}

///////////////////////////////////////////////////////////////////////////////
// NetworkSmsHandler

NetworkSmsHandler::NetworkSmsHandler() {}

NetworkSmsHandler::~NetworkSmsHandler() {
  ShillManagerClient::Get()->RemovePropertyChangedObserver(this);
}

void NetworkSmsHandler::Init() {
  // Add as an observer here so that new devices added after this call are
  // recognized.
  ShillManagerClient::Get()->AddPropertyChangedObserver(this);
  // Request network manager properties so that we can get the list of devices.
  ShillManagerClient::Get()->GetProperties(
      base::Bind(&NetworkSmsHandler::ManagerPropertiesCallback,
                 weak_ptr_factory_.GetWeakPtr()));
}

void NetworkSmsHandler::RequestUpdate(bool request_existing) {
  // If we already received messages and |request_existing| is true, send
  // updates for existing messages.
  for (const auto& message : received_messages_) {
    NotifyMessageReceived(*message);
  }
  // Request updates from each device.
  for (auto& handler : device_handlers_) {
    handler->RequestUpdate();
  }
}

void NetworkSmsHandler::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void NetworkSmsHandler::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void NetworkSmsHandler::OnPropertyChanged(const std::string& name,
                                          const base::Value& value) {
  if (name != shill::kDevicesProperty)
    return;
  const base::ListValue* devices = NULL;
  if (!value.GetAsList(&devices) || !devices)
    return;
  UpdateDevices(devices);
}

// Private methods

void NetworkSmsHandler::AddReceivedMessage(
    const base::DictionaryValue& message) {
  if (received_messages_.size() >= kMaxReceivedMessages)
    received_messages_.erase(received_messages_.begin());
  received_messages_.push_back(message.CreateDeepCopy());
}

void NetworkSmsHandler::NotifyMessageReceived(
    const base::DictionaryValue& message) {
  for (auto& observer : observers_)
    observer.MessageReceived(message);
}

void NetworkSmsHandler::MessageReceived(const base::DictionaryValue& message) {
  AddReceivedMessage(message);
  NotifyMessageReceived(message);
}

void NetworkSmsHandler::ManagerPropertiesCallback(
    DBusMethodCallStatus call_status,
    const base::DictionaryValue& properties) {
  if (call_status != DBUS_METHOD_CALL_SUCCESS) {
    LOG(ERROR) << "NetworkSmsHandler: Failed to get manager properties.";
    return;
  }
  const base::Value* value;
  if (!properties.GetWithoutPathExpansion(shill::kDevicesProperty, &value) ||
      !value->is_list()) {
    LOG(ERROR) << "NetworkSmsHandler: No list value for: "
               << shill::kDevicesProperty;
    return;
  }
  const base::ListValue* devices = static_cast<const base::ListValue*>(value);
  UpdateDevices(devices);
}

void NetworkSmsHandler::UpdateDevices(const base::ListValue* devices) {
  for (base::ListValue::const_iterator iter = devices->begin();
       iter != devices->end(); ++iter) {
    std::string device_path;
    iter->GetAsString(&device_path);
    if (!device_path.empty()) {
      // Request device properties.
      VLOG(1) << "GetDeviceProperties: " << device_path;
      ShillDeviceClient::Get()->GetProperties(
          dbus::ObjectPath(device_path),
          base::Bind(&NetworkSmsHandler::DevicePropertiesCallback,
                     weak_ptr_factory_.GetWeakPtr(), device_path));
    }
  }
}

void NetworkSmsHandler::DevicePropertiesCallback(
    const std::string& device_path,
    DBusMethodCallStatus call_status,
    const base::DictionaryValue& properties) {
  if (call_status != DBUS_METHOD_CALL_SUCCESS) {
    LOG(ERROR) << "NetworkSmsHandler: ERROR: " << call_status
               << " For: " << device_path;
    return;
  }

  std::string device_type;
  if (!properties.GetStringWithoutPathExpansion(
          shill::kTypeProperty, &device_type)) {
    LOG(ERROR) << "NetworkSmsHandler: No type for: " << device_path;
    return;
  }
  if (device_type != shill::kTypeCellular)
    return;

  std::string service_name;
  if (!properties.GetStringWithoutPathExpansion(
          shill::kDBusServiceProperty, &service_name)) {
    LOG(ERROR) << "Device has no DBusService Property: " << device_path;
    return;
  }

  std::string object_path_string;
  if (!properties.GetStringWithoutPathExpansion(
          shill::kDBusObjectProperty, &object_path_string)) {
    LOG(ERROR) << "Device has no DBusObject Property: " << device_path;
    return;
  }
  dbus::ObjectPath object_path(object_path_string);
  if (service_name == modemmanager::kModemManager1ServiceName) {
    device_handlers_.push_back(
        std::make_unique<ModemManager1NetworkSmsDeviceHandler>(
            this, service_name, object_path));
  }
}

}  // namespace chromeos
