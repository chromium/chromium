// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/shill/sms_client.h"

#include <map>
#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/shill/fake_sms_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "dbus/property.h"
#include "dbus/values_util.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

namespace {

// See "enum MMSMSState" definition in ModemManager.
constexpr uint32_t kSMSStateReceived = 3;  // MM_SMS_STATE_RECEIVED

SMSClient* g_instance = nullptr;

class SMSReceiveHandler {
 public:
  SMSReceiveHandler(dbus::ObjectProxy* object_proxy,
                    SMSClient::GetAllCallback callback)
      : callback_(std::move(callback)) {
    property_set_ = std::make_unique<dbus::PropertySet>(
        object_proxy, modemmanager::kModemManager1SmsInterface,
        base::BindRepeating(&SMSReceiveHandler::OnPropertyChanged,
                            weak_ptr_factory_.GetWeakPtr()));
    property_set_->RegisterProperty(SMSClient::kSMSPropertyState, &state_);
    property_set_->ConnectSignals();
    property_set_->Get(&state_, dbus::PropertySet::GetCallback());
  }

  SMSReceiveHandler(const SMSReceiveHandler&) = delete;
  SMSReceiveHandler& operator=(const SMSReceiveHandler&) = delete;

  ~SMSReceiveHandler() = default;

 private:
  void OnPropertyChanged(const std::string& property_name) {
    // Initially, we monitor only the "State" property of the SMS object. When
    // the "State" property is updated as a result of the initial
    // PropertySet::Get() call or subsequent D-Bus signals on property changes,
    // we check if the "State" property indicates that the SMS is fully
    // received. If the SMS is fully received, we fetch all properties via
    // PropertySet::GetAll(). When the properties of interest are updated,
    // |callback_| is invoked to notify the observer of the received SMS.
    if (callback_.is_null())
      return;

    if (number_.is_valid() && text_.is_valid() && timestamp_.is_valid()) {
      base::Value::Dict sms;
      sms.Set(SMSClient::kSMSPropertyNumber, number_.value());
      sms.Set(SMSClient::kSMSPropertyText, text_.value());
      sms.Set(SMSClient::kSMSPropertyTimestamp, timestamp_.value());
      // Move |callback_| to the task to ensure that |callback_| is only called
      // once. Since |callback_| may destruct this object, schedule it to the
      // task runner to run after this method returns.
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback_), std::move(sms)));
      return;
    }

    if (state_.is_valid() && state_.value() == kSMSStateReceived &&
        !sms_received_) {
      sms_received_ = true;
      property_set_->RegisterProperty(SMSClient::kSMSPropertyNumber, &number_);
      property_set_->RegisterProperty(SMSClient::kSMSPropertyText, &text_);
      property_set_->RegisterProperty(SMSClient::kSMSPropertyTimestamp,
                                      &timestamp_);
      property_set_->GetAll();
    }
  }

  SMSClient::GetAllCallback callback_;
  bool sms_received_ = false;
  dbus::Property<uint32_t> state_;
  dbus::Property<std::string> number_;
  dbus::Property<std::string> text_;
  dbus::Property<std::string> timestamp_;
  std::unique_ptr<dbus::PropertySet> property_set_;
  base::WeakPtrFactory<SMSReceiveHandler> weak_ptr_factory_{this};
};

// SMSClient is used to communicate with the
// org.freedesktop.ModemManager1.SMS service.  All methods should be
// called from the origin thread (UI thread) which initializes the
// DBusThreadManager instance.
class SMSClientImpl : public SMSClient {
 public:
  explicit SMSClientImpl(dbus::Bus* bus) : bus_(bus) {}

  SMSClientImpl(const SMSClientImpl&) = delete;
  SMSClientImpl& operator=(const SMSClientImpl&) = delete;

  ~SMSClientImpl() override = default;

  // Calls GetAll method.  |callback| is called after the method call succeeds.
  void GetAll(const std::string& service_name,
              const dbus::ObjectPath& object_path,
              GetAllCallback callback) override {
    dbus::ObjectProxy* proxy = bus_->GetObjectProxy(service_name, object_path);
    sms_receive_handlers_[object_path] = std::make_unique<SMSReceiveHandler>(
        proxy, base::BindOnce(&SMSClientImpl::OnSMSReceived,
                              weak_ptr_factory_.GetWeakPtr(), object_path,
                              std::move(callback)));
  }

 private:
  void OnSMSReceived(const dbus::ObjectPath& object_path,
                     GetAllCallback callback,
                     const base::Value::Dict& sms) {
    sms_receive_handlers_.erase(object_path);
    std::move(callback).Run(sms);
  }

  raw_ptr<dbus::Bus> bus_;

  std::map<dbus::ObjectPath, std::unique_ptr<SMSReceiveHandler>>
      sms_receive_handlers_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<SMSClientImpl> weak_ptr_factory_{this};
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// SMSClient

// Properties of org.freedesktop.ModemManager1.Sms object:
const char SMSClient::kSMSPropertyState[] = "State";
const char SMSClient::kSMSPropertyNumber[] = "Number";
const char SMSClient::kSMSPropertyText[] = "Text";
const char SMSClient::kSMSPropertyTimestamp[] = "Timestamp";

SMSClient::SMSClient() {
  DCHECK(!g_instance);
  g_instance = this;
}

SMSClient::~SMSClient() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
void SMSClient::Initialize(dbus::Bus* bus) {
  DCHECK(bus);
  new SMSClientImpl(bus);
}

// static
void SMSClient::InitializeFake() {
  new FakeSMSClient();
}

// static
void SMSClient::Shutdown() {
  DCHECK(g_instance);
  delete g_instance;
}

// static
SMSClient* SMSClient::Get() {
  return g_instance;
}

}  // namespace ash
