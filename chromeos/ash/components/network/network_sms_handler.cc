// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chromeos/ash/components/network/network_sms_handler.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <memory>
#include <vector>

#include "base/check.h"
#include "base/containers/circular_deque.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/shill/modem_messaging_client.h"
#include "chromeos/ash/components/dbus/shill/shill_device_client.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/dbus/shill/sms_client.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_type_pattern.h"
#include "components/device_event_log/device_event_log.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace {

// Key for the device GUID in message data received from the
// NetworkSmsDeviceHandler.
const char kNetworkGuidKey[] = "GUID";

// Maximum number of messages stored for RequestUpdate(true).
const size_t kMaxReceivedMessages = 100;

std::optional<const std::string> GetStringOptional(
    const base::Value::Dict& dict,
    const std::string& key) {
  if (!dict.FindString(key)) {
    return std::nullopt;
  }
  return *dict.FindString(key);
}

}  // namespace

namespace ash {

// static
const char NetworkSmsHandler::kNumberKey[] = "number";
const char NetworkSmsHandler::kTextKey[] = "text";
const char NetworkSmsHandler::kTimestampKey[] = "timestamp";
const base::TimeDelta NetworkSmsHandler::kFetchSmsDetailsTimeout =
    base::Seconds(60);

TextMessageData::TextMessageData(std::optional<const std::string> number,
                                 std::optional<const std::string> text,
                                 std::optional<const std::string> timestamp)
    : number(number), text(text), timestamp(timestamp) {}

TextMessageData::~TextMessageData() = default;

TextMessageData::TextMessageData(TextMessageData&& other) {
  number = std::move(other.number);
  text = std::move(other.text);
  timestamp = std::move(other.timestamp);
}

TextMessageData& TextMessageData::operator=(TextMessageData&& other) {
  number = std::move(other.number);
  text = std::move(other.text);
  timestamp = std::move(other.timestamp);
  return *this;
}

class NetworkSmsHandler::NetworkSmsDeviceHandler {
 public:
  NetworkSmsDeviceHandler() = default;
  virtual ~NetworkSmsDeviceHandler() = default;

  // Updates the last active network's GUID for the current device handler.
  virtual void SetLastActiveNetwork(const NetworkState* state) {}
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
  void ListCallback(std::optional<std::vector<dbus::ObjectPath>> paths);
  void SmsReceivedCallback(const dbus::ObjectPath& path, bool complete);
  void GetCallback(const dbus::ObjectPath& sms_path,
                   const base::Value::Dict& dictionary);
  void DeleteMessages();
  void DeleteCallback(const dbus::ObjectPath& sms_path, bool success);
  void GetMessages();
  void MessageReceived(const base::Value::Dict& dictionary);
  void OnFetchSmsDetailsTimeout(const dbus::ObjectPath& sms_path);
  void SetLastActiveNetwork(const NetworkState* state) override;

  raw_ptr<NetworkSmsHandler> host_;
  std::string service_name_;
  dbus::ObjectPath object_path_;
  base::OneShotTimer fetch_sms_details_timer_;
  bool deleting_messages_ = false;
  bool retrieving_messages_ = false;
  std::vector<dbus::ObjectPath> delete_queue_;
  base::circular_deque<dbus::ObjectPath> retrieval_queue_;
  std::string last_active_network_guid_;
  base::WeakPtrFactory<ModemManager1NetworkSmsDeviceHandler> weak_ptr_factory_{
      this};
};

NetworkSmsHandler::ModemManager1NetworkSmsDeviceHandler::
    ModemManager1NetworkSmsDeviceHandler(NetworkSmsHandler* host,
                                         const std::string& service_name,
                                         const dbus::ObjectPath& object_path)
    : host_(host), service_name_(service_name), object_path_(object_path) {
  NET_LOG(DEBUG)
      << "SMS handler for " << object_path.value()
      << " created, setting SMS receive handler and fetching existing messages";

  // Set the handler for received Sms messages.
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
    std::optional<std::vector<dbus::ObjectPath>> paths) {
  // This receives all messages, so clear any pending gets and deletes.
  retrieval_queue_.clear();
  delete_queue_.clear();

  if (!paths.has_value()) {
    NET_LOG(DEBUG) << "No paths returned";
    return;
  }

  NET_LOG(EVENT) << "Bulk fetched [" << paths->size() << "] message(s)";
  retrieval_queue_.reserve(paths->size());
  retrieval_queue_.assign(std::make_move_iterator(paths->begin()),
                          std::make_move_iterator(paths->end()));
  if (retrieving_messages_) {
    NET_LOG(DEBUG) << "Already retrieving messages, not starting queue";
    return;
  }

  GetMessages();
}

// Messages must be deleted one at a time, since we can not guarantee
// the order the deletion will be executed in. Delete messages from
// the back of the list so that the indices are valid.
void NetworkSmsHandler::ModemManager1NetworkSmsDeviceHandler::DeleteMessages() {
  if (delete_queue_.empty()) {
    NET_LOG(DEBUG) << "Delete queue is empty, finished deleting messages";
    deleting_messages_ = false;
    return;
  }
  deleting_messages_ = true;
  dbus::ObjectPath sms_path = std::move(delete_queue_.back());
  delete_queue_.pop_back();
  NET_LOG(EVENT) << "Deleting " << sms_path.value() << ", ["
                 << delete_queue_.size()
                 << "] message(s) left in the deletion queue";
  ModemMessagingClient::Get()->Delete(
      service_name_, object_path_, sms_path,
      base::BindOnce(&NetworkSmsHandler::ModemManager1NetworkSmsDeviceHandler::
                         DeleteCallback,
                     weak_ptr_factory_.GetWeakPtr(), sms_path));
}

void NetworkSmsHandler::ModemManager1NetworkSmsDeviceHandler::DeleteCallback(
    const dbus::ObjectPath& sms_path,
    bool success) {
  if (!success) {
    // Add the SMS to the queue so that it can eventually get retried.
    delete_queue_.insert(delete_queue_.begin(), sms_path);
    NET_LOG(ERROR) << "Delete failed for " << sms_path.value()
                   << ", inserted message back into the deletion queue, ["
                   << delete_queue_.size()
                   << "] message(s) left in the deletion queue";

    // Set the flag back to false so that new deletion attempts can occur the
    // next time a message is received.
    deleting_messages_ = false;
    return;
  }

  NET_LOG(EVENT) << "Delete succeeded for " << sms_path.value();
  DeleteMessages();
}

// Messages must be fetched one at a time, so that we do not queue too
// many requests to a single threaded server.
void NetworkSmsHandler::ModemManager1NetworkSmsDeviceHandler::GetMessages() {
  if (retrieval_queue_.empty()) {
    NET_LOG(DEBUG) << "Retrieval queue is empty, finished retrieving messages";
    retrieving_messages_ = false;
    if (!deleting_messages_)
      DeleteMessages();
    return;
  }
  retrieving_messages_ = true;
  dbus::ObjectPath sms_path = retrieval_queue_.front();
  retrieval_queue_.pop_front();
  NET_LOG(EVENT) << "Fetching details for " << sms_path.value() << ", ["
                 << retrieval_queue_.size()
                 << "] message(s) left in the retrieval queue";
  fetch_sms_details_timer_.Start(
      FROM_HERE, kFetchSmsDetailsTimeout,
      base::BindOnce(&NetworkSmsHandler::ModemManager1NetworkSmsDeviceHandler::
                         OnFetchSmsDetailsTimeout,
                     weak_ptr_factory_.GetWeakPtr(), sms_path));
  SMSClient::Get()->GetAll(
      service_name_, sms_path,
      base::BindOnce(
          &NetworkSmsHandler::ModemManager1NetworkSmsDeviceHandler::GetCallback,
          weak_ptr_factory_.GetWeakPtr(), sms_path));
  delete_queue_.push_back(sms_path);
}

void NetworkSmsHandler::ModemManager1NetworkSmsDeviceHandler::
    SmsReceivedCallback(const dbus::ObjectPath& sms_path, bool complete) {
  NET_LOG(EVENT) << "Message received: " << sms_path.value();
  // Only handle complete messages.
  if (!complete) {
    NET_LOG(DEBUG) << "Message is not complete, not handling: "
                   << sms_path.value();
    return;
  }

  retrieval_queue_.push_back(sms_path);
  if (retrieving_messages_) {
    NET_LOG(DEBUG)
        << "SMS received but already retrieving messages, not starting queue";
    return;
  }

  GetMessages();
}

void NetworkSmsHandler::ModemManager1NetworkSmsDeviceHandler::GetCallback(
    const dbus::ObjectPath& sms_path,
    const base::Value::Dict& dictionary) {
  NET_LOG(EVENT) << "Message details fetched for: " << sms_path.value();
  fetch_sms_details_timer_.Stop();
  MessageReceived(dictionary);
  GetMessages();
}

void NetworkSmsHandler::ModemManager1NetworkSmsDeviceHandler::MessageReceived(
    const base::Value::Dict& dictionary) {
  // The keys of the ModemManager1.SMS interface do not match the exported keys,
  // so a new dictionary is created with the expected key names.
  base::Value::Dict new_dictionary;
  const std::string* number =
      dictionary.FindString(SMSClient::kSMSPropertyNumber);
  if (number) {
    new_dictionary.Set(kNumberKey, *number);
  }
  const std::string* text = dictionary.FindString(SMSClient::kSMSPropertyText);
  if (text) {
    new_dictionary.Set(kTextKey, *text);
  }
  const std::string* timestamp =
      dictionary.FindString(SMSClient::kSMSPropertyTimestamp);
  if (timestamp) {
    new_dictionary.Set(kTimestampKey, *timestamp);
  }

  new_dictionary.Set(kNetworkGuidKey, last_active_network_guid_);

  host_->MessageReceived(new_dictionary);
}

void NetworkSmsHandler::ModemManager1NetworkSmsDeviceHandler::
    OnFetchSmsDetailsTimeout(const dbus::ObjectPath& sms_path) {
  NET_LOG(ERROR) << "SMSClient::GetAll() timed out for " << sms_path.value()
                 << ", moving to next message.";
  GetMessages();
}

void NetworkSmsHandler::ModemManager1NetworkSmsDeviceHandler::
    SetLastActiveNetwork(const NetworkState* network_state) {
  if (!network_state) {
    return;
  }
  NET_LOG(DEBUG) << "Updating last seen network to network with GUID: "
                 << network_state->guid();
  last_active_network_guid_ = network_state->guid();
}

///////////////////////////////////////////////////////////////////////////////
// NetworkSmsHandler

NetworkSmsHandler::NetworkSmsHandler() = default;

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

void NetworkSmsHandler::Init(NetworkStateHandler* network_state_handler) {
  network_state_handler_ = network_state_handler;
  network_state_handler_observation_.Observe(network_state_handler_);
  Init();
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
    UpdateDevices(value.GetList());
  }

  if (name == shill::kIccidProperty && value.is_string()) {
    OnActiveDeviceIccidChanged(value.GetString());
  }
}

void NetworkSmsHandler::OnActiveDeviceIccidChanged(const std::string& iccid) {
  if (!device_handler_ || iccid.empty()) {
    return;
  }
  NetworkStateHandler::NetworkStateList active_networks;
  // We also look at non-active networks, to account for networks that are
  // disconnected as you can receive text messages on active devices with
  // disconnected networks.
  network_state_handler_->GetNetworkListByType(
      NetworkTypePattern::Cellular(), /*configured_only=*/false,
      /*visible_only=*/false, /*limit=*/0, &active_networks);
  for (auto* network : active_networks) {
    if (network->iccid() == iccid) {
      device_handler_->SetLastActiveNetwork(network);
      return;
    }
  }
}

void NetworkSmsHandler::ActiveNetworksChanged(
    const std::vector<const NetworkState*>& active_networks) {
  for (const NetworkState* network : active_networks) {
    if (network->type() == shill::kTypeCellular && device_handler_) {
      device_handler_->SetLastActiveNetwork(network);
      break;
    }
  }
}

// Private methods

void NetworkSmsHandler::AddReceivedMessage(const base::Value::Dict& message) {
  if (received_messages_.size() >= kMaxReceivedMessages)
    received_messages_.erase(received_messages_.begin());
  received_messages_.push_back(message.Clone());
}

void NetworkSmsHandler::NotifyMessageReceived(
    const base::Value::Dict& message) {
  TextMessageData message_data{GetStringOptional(message, kNumberKey),
                               GetStringOptional(message, kTextKey),
                               GetStringOptional(message, kTimestampKey)};

  const std::string network_guid =
      GetStringOptional(message, kNetworkGuidKey).value_or(std::string());
  if (network_guid.empty()) {
    NET_LOG(ERROR) << "Message received with an empty GUID";
  }

  for (auto& observer : observers_) {
    observer.MessageReceivedFromNetwork(network_guid, message_data);
  }
}

void NetworkSmsHandler::MessageReceived(const base::Value::Dict& message) {
  AddReceivedMessage(message);
  NotifyMessageReceived(message);
}

void NetworkSmsHandler::ManagerPropertiesCallback(
    std::optional<base::Value::Dict> properties) {
  if (!properties) {
    NET_LOG(ERROR) << "NetworkSmsHandler: Failed to get manager properties.";
    return;
  }
  const base::Value::List* value =
      properties->FindList(shill::kDevicesProperty);
  if (!value) {
    NET_LOG(EVENT) << "NetworkSmsHandler: No list value for: "
                   << shill::kDevicesProperty;
    return;
  }
  UpdateDevices(*value);
}

void NetworkSmsHandler::UpdateDevices(const base::Value::List& devices) {
  for (const auto& item : devices) {
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
    std::optional<base::Value::Dict> properties) {
  if (!properties) {
    NET_LOG(ERROR) << "NetworkSmsHandler error for: " << device_path;
    return;
  }

  const std::string* device_type = properties->FindString(shill::kTypeProperty);
  if (!device_type) {
    NET_LOG(ERROR) << "NetworkSmsHandler: No type for: " << device_path;
    return;
  }
  if (*device_type != shill::kTypeCellular)
    return;

  const std::string* service_name =
      properties->FindString(shill::kDBusServiceProperty);
  if (!service_name) {
    NET_LOG(ERROR) << "Device has no DBusService Property: " << device_path;
    return;
  }

  if (*service_name != modemmanager::kModemManager1ServiceName)
    return;

  const std::string* object_path_string =
      properties->FindString(shill::kDBusObjectProperty);
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

    OnActiveDeviceIccidChanged(
        GetStringOptional(*properties, shill::kIccidProperty)
            .value_or(std::string()));
    device_handler_->SetLastActiveNetwork(
        network_state_handler_->ConnectedNetworkByType(
            NetworkTypePattern::Cellular()));

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
