// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/network_sms_handler.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/containers/circular_deque.h"
#include "base/logging.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/shill/modem_messaging_client.h"
#include "chromeos/ash/components/dbus/shill/shill_device_client.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/dbus/shill/sms_client.h"
#include "components/device_event_log/device_event_log.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace {

// Maximum number of messages stored for RequestUpdate(true).
const size_t kMaxReceivedMessages = 100;

}  // namespace

namespace ash {

// static
const char NetworkSmsHandler::kNumberKey[] = "number";
const char NetworkSmsHandler::kTextKey[] = "text";
const char NetworkSmsHandler::kTimestampKey[] = "timestamp";

class NetworkSmsHandler::NetworkSmsDeviceHandler {
 public:
  NetworkSmsDeviceHandler() = default;
  virtual ~NetworkSmsDeviceHandler() = default;
};

class NetworkSmsHandler::ModemManager1NetworkSmsDeviceHandler
    : public NetworkSmsHandler::NetworkSmsDeviceHandler {
 public:
  ModemManager1NetworkSmsDeviceHandler(NetworkSmsHandler* host,
                                       const std::string& service_name,
                                       const dbus::ObjectPath& object_path);

  ModemManager1NetworkSmsDeviceHandler(
      const ModemManager1NetworkSmsDeviceHandler&) = delete;
  ModemManager1NetworkSmsDeviceHandler& operator=(
      const ModemManager1NetworkSmsDeviceHandler&) = delete;

  ~ModemManager1NetworkSmsDeviceHandler() override;

 private:
  void ListCallback(absl::optional<std::vector<dbus::ObjectPath>> paths);
  void SmsReceivedCallback(const dbus::ObjectPath& path, bool complete);
  void GetCallback(const base::Value& dictionary);
  void DeleteMessages();
  void DeleteCallback(bool success);
  void GetMessages();
  void MessageReceived(const base::Value& dictionary);

  NetworkSmsHandler* host_;
  std::string service_name_;
  dbus::ObjectPath object_path_;
  bool deleting_messages_;
  bool retrieving_messages_;
  std::vector<dbus::ObjectPath> delete_queue_;
  base::circular_deque<dbus::ObjectPath> retrieval_queue_;
  base::WeakPtrFactory<ModemManager1NetworkSmsDeviceHandler> weak_ptr_factory_{
      this};
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
      base::BindRepeating(
          &NetworkSmsHandler::ModemManager1NetworkSmsDeviceHandler::
              SmsReceivedCallback,
          weak_ptr_factory_.GetWeakPtr()));

  // List the existing messages.
  ModemMessagingClient::Get()->List(
      service_name_, object_path_,
      base::BindOnce(&NetworkSmsHandler::ModemManager1NetworkSmsDeviceHandler::
                         ListCallback,
                     weak_ptr_factory_.GetWeakPtr()));
}

NetworkSmsHandler::ModemManager1NetworkSmsDeviceHandler::
    ~ModemManager1NetworkSmsDeviceHandler() {
  ModemMessagingClient::Get()->ResetSmsReceivedHandler(service_name_,
                                                       object_path_);
}

void NetworkSmsHandler::ModemManager1NetworkSmsDeviceHandler::ListCallback(
    absl::optional<std::vector<dbus::ObjectPath>> paths) {
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

void NetworkSmsHandler::ModemManager1NetworkSmsDeviceHandler::
    SmsReceivedCallback(const dbus::ObjectPath& sms_path, bool complete) {
  // Only handle complete messages.
  if (!complete)
    return;
  retrieval_queue_.push_back(sms_path);
  if (!retrieving_messages_)
    GetMessages();
}

void NetworkSmsHandler::ModemManager1NetworkSmsDeviceHandler::GetCallback(
    const base::Value& dictionary) {
  MessageReceived(dictionary);
  GetMessages();
}

void NetworkSmsHandler::ModemManager1NetworkSmsDeviceHandler::MessageReceived(
    const base::Value& dictionary) {
  // The keys of the ModemManager1.SMS interface do not match the
  // exported keys, so a new dictionary is created with the expected
  // key namaes.
  base::Value new_dictionary(base::Value::Type::DICTIONARY);
  const std::string* number =
      dictionary.FindStringKey(SMSClient::kSMSPropertyNumber);
  if (number)
    new_dictionary.SetStringKey(kNumberKey, *number);
  const std::string* text =
      dictionary.FindStringKey(SMSClient::kSMSPropertyText);
  if (text)
    new_dictionary.SetStringKey(kTextKey, *text);
  const std::string* timestamp =
      dictionary.FindStringKey(SMSClient::kSMSPropertyTimestamp);
  if (timestamp)
    new_dictionary.SetStringKey(kTimestampKey, *timestamp);
  host_->MessageReceived(new_dictionary);
}

///////////////////////////////////////////////////////////////////////////////
// NetworkSmsHandler

NetworkSmsHandler::NetworkSmsHandler() {}

NetworkSmsHandler::~NetworkSmsHandler() {
  ShillManagerClient::Get()->RemovePropertyChangedObserver(this);
  if (!cellular_device_path_.empty()) {
    ShillDeviceClient::Get()->RemovePropertyChangedObserver(
        dbus::ObjectPath(cellular_device_path_), this);
  }
}

void NetworkSmsHandler::Init() {
  // Add as an observer here so that new devices added after this call are
  // recognized.
  ShillManagerClient::Get()->AddPropertyChangedObserver(this);
  // Request network manager properties so that we can get the list of devices.
  ShillManagerClient::Get()->GetProperties(
      base::BindOnce(&NetworkSmsHandler::ManagerPropertiesCallback,
                     weak_ptr_factory_.GetWeakPtr()));
}

void NetworkSmsHandler::RequestUpdate() {
  for (const auto& message : received_messages_) {
    NotifyMessageReceived(message);
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
  // Device property change
  if (name == shill::kDBusObjectProperty) {
    OnObjectPathChanged(value);
    return;
  }

  // Manager property change
  if (name == shill::kDevicesProperty && value.is_list()) {
    UpdateDevices(value);
  }
}

// Private methods

void NetworkSmsHandler::AddReceivedMessage(const base::Value& message) {
  if (received_messages_.size() >= kMaxReceivedMessages)
    received_messages_.erase(received_messages_.begin());
  received_messages_.push_back(message.Clone());
}

void NetworkSmsHandler::NotifyMessageReceived(const base::Value& message) {
  for (auto& observer : observers_)
    observer.MessageReceived(message);
}

void NetworkSmsHandler::MessageReceived(const base::Value& message) {
  AddReceivedMessage(message);
  NotifyMessageReceived(message);
}

void NetworkSmsHandler::ManagerPropertiesCallback(
    absl::optional<base::Value> properties) {
  if (!properties) {
    NET_LOG(ERROR) << "NetworkSmsHandler: Failed to get manager properties.";
    return;
  }
  const base::Value* value = properties->FindListKey(shill::kDevicesProperty);
  if (!value) {
    NET_LOG(EVENT) << "NetworkSmsHandler: No list value for: "
                   << shill::kDevicesProperty;
    return;
  }
  UpdateDevices(*value);
}

void NetworkSmsHandler::UpdateDevices(const base::Value& devices) {
  for (const auto& item : devices.GetList()) {
    if (!item.is_string())
      continue;

    std::string device_path = item.GetString();
    if (!device_path.empty()) {
      // Request device properties.
      NET_LOG(DEBUG) << "GetDeviceProperties: " << device_path;
      ShillDeviceClient::Get()->GetProperties(
          dbus::ObjectPath(device_path),
          base::BindOnce(&NetworkSmsHandler::DevicePropertiesCallback,
                         weak_ptr_factory_.GetWeakPtr(), device_path));
    }
  }
}

void NetworkSmsHandler::DevicePropertiesCallback(
    const std::string& device_path,
    absl::optional<base::Value> properties) {
  if (!properties) {
    NET_LOG(ERROR) << "NetworkSmsHandler error for: " << device_path;
    return;
  }

  const std::string* device_type =
      properties->FindStringKey(shill::kTypeProperty);
  if (!device_type) {
    NET_LOG(ERROR) << "NetworkSmsHandler: No type for: " << device_path;
    return;
  }
  if (*device_type != shill::kTypeCellular)
    return;

  const std::string* service_name =
      properties->FindStringKey(shill::kDBusServiceProperty);
  if (!service_name) {
    NET_LOG(ERROR) << "Device has no DBusService Property: " << device_path;
    return;
  }

  if (*service_name != modemmanager::kModemManager1ServiceName)
    return;

  const std::string* object_path_string =
      properties->FindStringKey(shill::kDBusObjectProperty);
  if (!object_path_string || object_path_string->empty()) {
    NET_LOG(ERROR) << "Device has no or empty DBusObject Property: "
                   << device_path;
    return;
  }
  dbus::ObjectPath object_path(*object_path_string);

  // Destroy |device_handler_| first to reset the current SmsReceivedHandler.
  // Only one active handler is supported. TODO(crbug.com/1239418): Fix.
  device_handler_.reset();
  device_handler_ = std::make_unique<ModemManager1NetworkSmsDeviceHandler>(
      this, *service_name, object_path);

  if (!cellular_device_path_.empty()) {
    ShillDeviceClient::Get()->RemovePropertyChangedObserver(
        dbus::ObjectPath(cellular_device_path_), this);
  }
  cellular_device_path_ = device_path;
  ShillDeviceClient::Get()->AddPropertyChangedObserver(
      dbus::ObjectPath(cellular_device_path_), this);
}

void NetworkSmsHandler::OnObjectPathChanged(const base::Value& object_path) {
  // Remove the old handler.
  device_handler_.reset();

  std::string object_path_string =
      object_path.is_string() ? object_path.GetString() : "";
  // If the new object path is empty, there is no SIM. Don't create a new
  // handler.
  if (object_path_string.empty() || object_path_string == "/") {
    return;
  }

  // Recreate handler for the new object path.
  ShillManagerClient::Get()->GetProperties(
      base::BindOnce(&NetworkSmsHandler::ManagerPropertiesCallback,
                     weak_ptr_factory_.GetWeakPtr()));
}

}  // namespace ash
