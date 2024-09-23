// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/hermes/hermes_profile_client.h"

#include "ash/constants/ash_features.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/dbus/hermes/constants.h"
#include "chromeos/ash/components/dbus/hermes/fake_hermes_profile_client.h"
#include "chromeos/ash/components/dbus/hermes/hermes_response_status.h"
#include "components/device_event_log/device_event_log.h"
#include "dbus/bus.h"
#include "dbus/object_manager.h"
#include "dbus/property.h"
#include "third_party/cros_system_api/dbus/hermes/dbus-constants.h"

namespace dbus {

// dbus::Property specialization to read and write
// hermes::profile::State enum.
template <>
Property<hermes::profile::State>::Property()
    : value_(hermes::profile::State::kPending) {}

template <>
bool Property<hermes::profile::State>::PopValueFromReader(
    MessageReader* reader) {
  int32_t int_value;
  if (!reader->PopVariantOfInt32(&int_value)) {
    NET_LOG(ERROR) << "Unable to pop value for eSIM profile state.";
    return false;
  }
  switch (int_value) {
    case hermes::profile::State::kActive:
    case hermes::profile::State::kInactive:
    case hermes::profile::State::kPending:
      value_ = static_cast<hermes::profile::State>(int_value);
      return true;
  }
  NOTREACHED_IN_MIGRATION()
      << "Received invalid hermes profile state " << int_value;
  return false;
}

template <>
void Property<hermes::profile::State>::AppendSetValueToWriter(
    MessageWriter* writer) {
  writer->AppendVariantOfInt32(set_value_);
}

// dbus::Property specialization to read and write
// hermes::profile::ProfileClass enum.
template <>
Property<hermes::profile::ProfileClass>::Property()
    : value_(hermes::profile::ProfileClass::kOperational) {}

template <>
bool Property<hermes::profile::ProfileClass>::PopValueFromReader(
    MessageReader* reader) {
  int32_t int_value;
  if (!reader->PopVariantOfInt32(&int_value)) {
    NET_LOG(ERROR) << "Unable to pop value for eSIM profile class";
    return false;
  }
  switch (int_value) {
    case hermes::profile::ProfileClass::kTesting:
    case hermes::profile::ProfileClass::kProvisioning:
    case hermes::profile::ProfileClass::kOperational:
      value_ = static_cast<hermes::profile::ProfileClass>(int_value);
      return true;
  }
  NOTREACHED_IN_MIGRATION()
      << "Received invalid hermes profile class " << int_value;
  return false;
}

template <>
void Property<hermes::profile::ProfileClass>::AppendSetValueToWriter(
    MessageWriter* writer) {
  writer->AppendVariantOfInt32(set_value_);
}

}  // namespace dbus

namespace ash {

namespace {
HermesProfileClient* g_instance = nullptr;
}  // namespace

HermesProfileClient::Properties::Properties(
    dbus::ObjectProxy* object_proxy,
    const PropertyChangedCallback& callback)
    : dbus::PropertySet(object_proxy,
                        hermes::kHermesProfileInterface,
                        callback) {
  RegisterProperty(hermes::profile::kIccidProperty, &iccid_);
  RegisterProperty(hermes::profile::kServiceProviderProperty,
                   &service_provider_);
  RegisterProperty(hermes::profile::kMccMncProperty, &mcc_mnc_);
  RegisterProperty(hermes::profile::kActivationCodeProperty, &activation_code_);
  RegisterProperty(hermes::profile::kNameProperty, &name_);
  RegisterProperty(hermes::profile::kNicknameProperty, &nick_name_);
  RegisterProperty(hermes::profile::kStateProperty, &state_);
  RegisterProperty(hermes::profile::kProfileClassProperty, &profile_class_);
}

HermesProfileClient::Properties::~Properties() = default;

class HermesProfileClientImpl : public HermesProfileClient {
 public:
  explicit HermesProfileClientImpl(dbus::Bus* bus) : bus_(bus) {}
  explicit HermesProfileClientImpl(const HermesProfileClient&) = delete;
  HermesProfileClient& operator=(const HermesProfileClient&) = delete;
  ~HermesProfileClientImpl() override = default;

  using Object = std::pair<dbus::ObjectProxy*, std::unique_ptr<Properties>>;
  using ObjectMap = std::map<dbus::ObjectPath, Object>;

  // HermesProfileClient:
  void EnableCarrierProfile(const dbus::ObjectPath& carrier_profile_path,
                            HermesResponseCallback callback) override {
    dbus::MethodCall method_call(hermes::kHermesProfileInterface,
                                 hermes::profile::kEnable);
    dbus::ObjectProxy* object_proxy = GetObject(carrier_profile_path).first;
    object_proxy->CallMethodWithErrorResponse(
        &method_call, hermes_constants::kHermesNetworkOperationTimeoutMs,
        base::BindOnce(&HermesProfileClientImpl::OnHermesStatusResponse,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void DisableCarrierProfile(const dbus::ObjectPath& carrier_profile_path,
                             HermesResponseCallback callback) override {
    dbus::MethodCall method_call(hermes::kHermesProfileInterface,
                                 hermes::profile::kDisable);
    dbus::ObjectProxy* object_proxy = GetObject(carrier_profile_path).first;
    object_proxy->CallMethodWithErrorResponse(
        &method_call, hermes_constants::kHermesNetworkOperationTimeoutMs,
        base::BindOnce(&HermesProfileClientImpl::OnHermesStatusResponse,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void RenameProfile(const dbus::ObjectPath& carrier_profile_path,
                     const std::string& new_name,
                     HermesResponseCallback callback) override {
    dbus::MethodCall method_call(hermes::kHermesProfileInterface,
                                 hermes::profile::kRename);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(new_name);
    dbus::ObjectProxy* object_proxy = GetObject(carrier_profile_path).first;
    object_proxy->CallMethodWithErrorResponse(
        &method_call, hermes_constants::kHermesNetworkOperationTimeoutMs,
        base::BindOnce(&HermesProfileClientImpl::OnHermesStatusResponse,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  Properties* GetProperties(
      const dbus::ObjectPath& carrier_profile_path) override {
    return GetObject(carrier_profile_path).second.get();
  }

  TestInterface* GetTestInterface() override { return nullptr; }

 private:
  Object& GetObject(const dbus::ObjectPath& object_path) {
    ObjectMap::iterator it = object_map_.find(object_path);
    if (it != object_map_.end())
      return it->second;

    dbus::ObjectProxy* object_proxy =
        bus_->GetObjectProxy(hermes::kHermesServiceName, object_path);

    auto properties = std::make_unique<Properties>(
        object_proxy,
        base::BindRepeating(&HermesProfileClientImpl::OnPropertyChanged,
                            weak_ptr_factory_.GetWeakPtr(), object_path));
    properties->ConnectSignals();
    properties->GetAll();

    Object object = std::make_pair(object_proxy, std::move(properties));
    object_map_[object_path] = std::move(object);
    return object_map_[object_path];
  }

  void OnPropertyChanged(const dbus::ObjectPath& object_path,
                         const std::string& property_name) {
    for (auto& observer : observers()) {
      observer.OnCarrierProfilePropertyChanged(object_path, property_name);
    }
  }

  void OnHermesStatusResponse(HermesResponseCallback callback,
                              dbus::Response* response,
                              dbus::ErrorResponse* error_response) {
    if (error_response) {
      NET_LOG(ERROR) << "Hermes Profile operation failed with error: "
                     << error_response->GetErrorName();
      std::move(callback).Run(
          HermesResponseStatusFromErrorName(error_response->GetErrorName()));
      return;
    }
    std::move(callback).Run(HermesResponseStatus::kSuccess);
  }

  raw_ptr<dbus::Bus> bus_;
  ObjectMap object_map_;
  base::WeakPtrFactory<HermesProfileClientImpl> weak_ptr_factory_{this};
};

HermesProfileClient::HermesProfileClient() {
  DCHECK(!g_instance);
  g_instance = this;
}

HermesProfileClient::~HermesProfileClient() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

void HermesProfileClient::AddObserver(Observer* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void HermesProfileClient::RemoveObserver(Observer* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

// static
void HermesProfileClient::Initialize(dbus::Bus* bus) {
  DCHECK(bus);
  DCHECK(!g_instance);
  new HermesProfileClientImpl(bus);
}

// static
void HermesProfileClient::InitializeFake() {
  new FakeHermesProfileClient();
}

// static
void HermesProfileClient::Shutdown() {
  DCHECK(g_instance);
  delete g_instance;
}

// static
HermesProfileClient* HermesProfileClient::Get() {
  return g_instance;
}

}  // namespace ash
