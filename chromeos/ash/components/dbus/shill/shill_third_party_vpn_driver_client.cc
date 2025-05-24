// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chromeos/ash/components/dbus/shill/shill_third_party_vpn_driver_client.h"

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <set>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/shill/fake_shill_third_party_vpn_driver_client.h"
#include "chromeos/ash/components/dbus/shill/shill_third_party_vpn_observer.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

namespace {

const char* kSetParametersKeyList[] = {
    shill::kAddressParameterThirdPartyVpn,
    shill::kBroadcastAddressParameterThirdPartyVpn,
    shill::kExclusionListParameterThirdPartyVpn,
    shill::kInclusionListParameterThirdPartyVpn,
    shill::kSubnetPrefixParameterThirdPartyVpn,
    shill::kMtuParameterThirdPartyVpn,
    shill::kDomainSearchParameterThirdPartyVpn,
    shill::kDnsServersParameterThirdPartyVpn,
    shill::kReconnectParameterThirdPartyVpn};

ShillThirdPartyVpnDriverClient* g_instance = nullptr;

// The ShillThirdPartyVpnDriverClient implementation.
class ShillThirdPartyVpnDriverClientImpl
    : public ShillThirdPartyVpnDriverClient {
 public:
  explicit ShillThirdPartyVpnDriverClientImpl(dbus::Bus* bus);

  ShillThirdPartyVpnDriverClientImpl(
      const ShillThirdPartyVpnDriverClientImpl&) = delete;
  ShillThirdPartyVpnDriverClientImpl& operator=(
      const ShillThirdPartyVpnDriverClientImpl&) = delete;

  ~ShillThirdPartyVpnDriverClientImpl() override;

  // ShillThirdPartyVpnDriverClient overrides
  void AddShillThirdPartyVpnObserver(
      const std::string& object_path_value,
      ShillThirdPartyVpnObserver* observer) override;

  void RemoveShillThirdPartyVpnObserver(
      const std::string& object_path_value) override;

  void SetParameters(const std::string& object_path_value,
                     const base::Value::Dict& parameters,
                     StringCallback callback,
                     ErrorCallback error_callback) override;

  void UpdateConnectionState(const std::string& object_path_value,
                             const uint32_t connection_state,
                             base::OnceClosure callback,
                             ErrorCallback error_callback) override;

  void SendPacket(const std::string& object_path_value,
                  const std::vector<char>& ip_packet,
                  base::OnceClosure callback,
                  ErrorCallback error_callback) override;

  TestInterface* GetTestInterface() override { return nullptr; }

 private:
  class HelperInfo {
   public:
    explicit HelperInfo(dbus::ObjectProxy* object_proxy);

    ShillClientHelper* helper() { return &helper_; }
    ShillThirdPartyVpnObserver* observer() { return observer_; }

    void set_observer(ShillThirdPartyVpnObserver* observer) {
      observer_ = observer;
    }

    base::WeakPtr<HelperInfo> GetWeakPtr() {
      return weak_ptr_factory_.GetWeakPtr();
    }

   private:
    ShillClientHelper helper_;
    raw_ptr<ShillThirdPartyVpnObserver> observer_;

    base::WeakPtrFactory<HelperInfo> weak_ptr_factory_{this};
  };
  using HelperMap = std::map<std::string, raw_ptr<HelperInfo, CtnExperimental>>;

  static void OnPacketReceived(base::WeakPtr<HelperInfo> helper_info,
                               dbus::Signal* signal);
  static void OnPlatformMessage(base::WeakPtr<HelperInfo> helper_info,
                                dbus::Signal* signal);
  static void OnSignalConnected(const std::string& interface,
                                const std::string& signal,
                                bool success);

  // Returns or creates the corresponding ShillClientHelper for the
  // |object_path_value|.
  ShillClientHelper* GetHelper(const std::string& object_path_value);

  // Returns or creates the corresponding HelperInfo for the
  // |object_path_value|.
  HelperInfo* GetHelperInfo(const std::string& object_path_value);

  // Returns the corresponding HelperInfo for the |object_path| if exists,
  // nullptr if not.
  HelperInfo* FindHelperInfo(const dbus::ObjectPath& object_path);

  // Deletes the helper object corresponding to |object_path|.
  void DeleteHelper(const dbus::ObjectPath& object_path);

  raw_ptr<dbus::Bus> bus_;
  HelperMap helpers_;
  std::set<std::string> valid_keys_;
};

ShillThirdPartyVpnDriverClientImpl::HelperInfo::HelperInfo(
    dbus::ObjectProxy* object_proxy)
    : helper_(object_proxy), observer_(nullptr) {}

ShillThirdPartyVpnDriverClientImpl::ShillThirdPartyVpnDriverClientImpl(
    dbus::Bus* bus)
    : bus_(bus) {
  for (uint32_t i = 0; i < std::size(kSetParametersKeyList); ++i) {
    valid_keys_.insert(kSetParametersKeyList[i]);
  }
}

ShillThirdPartyVpnDriverClientImpl::~ShillThirdPartyVpnDriverClientImpl() {
  for (auto& iter : helpers_) {
    HelperInfo* helper_info = iter.second;
    bus_->RemoveObjectProxy(
        shill::kFlimflamServiceName,
        helper_info->helper()->object_proxy()->object_path(),
        base::DoNothing());
    delete helper_info;
  }
}

void ShillThirdPartyVpnDriverClientImpl::AddShillThirdPartyVpnObserver(
    const std::string& object_path_value,
    ShillThirdPartyVpnObserver* observer) {
  HelperInfo* helper_info = GetHelperInfo(object_path_value);
  if (helper_info->observer()) {
    LOG(ERROR) << "Observer exists for " << object_path_value;
    return;
  }

  // TODO(kaliamoorthi): Remove the const_cast.
  helper_info->set_observer(observer);
  dbus::ObjectProxy* proxy =
      const_cast<dbus::ObjectProxy*>(helper_info->helper()->object_proxy());

  proxy->ConnectToSignal(
      shill::kFlimflamThirdPartyVpnInterface, shill::kOnPlatformMessageFunction,
      base::BindRepeating(
          &ShillThirdPartyVpnDriverClientImpl::OnPlatformMessage,
          helper_info->GetWeakPtr()),
      base::BindOnce(&ShillThirdPartyVpnDriverClientImpl::OnSignalConnected));

  proxy->ConnectToSignal(
      shill::kFlimflamThirdPartyVpnInterface, shill::kOnPacketReceivedFunction,
      base::BindRepeating(&ShillThirdPartyVpnDriverClientImpl::OnPacketReceived,
                          helper_info->GetWeakPtr()),
      base::BindOnce(&ShillThirdPartyVpnDriverClientImpl::OnSignalConnected));
}

void ShillThirdPartyVpnDriverClientImpl::RemoveShillThirdPartyVpnObserver(
    const std::string& object_path_value) {
  HelperInfo* helper_info = FindHelperInfo(dbus::ObjectPath(object_path_value));
  if (!helper_info) {
    LOG(ERROR) << "Unknown object_path_value " << object_path_value;
    return;
  }

  CHECK(helper_info->observer());
  helper_info->set_observer(nullptr);
  DeleteHelper(dbus::ObjectPath(object_path_value));
}

void ShillThirdPartyVpnDriverClientImpl::DeleteHelper(
    const dbus::ObjectPath& object_path) {
  HelperInfo* helper_info = FindHelperInfo(dbus::ObjectPath(object_path));
  if (!helper_info) {
    LOG(ERROR) << "Unknown object_path " << object_path.value();
    return;
  }

  bus_->RemoveObjectProxy(shill::kFlimflamServiceName, object_path,
                          base::DoNothing());
  helpers_.erase(helpers_.find(object_path.value()));
  delete helper_info;
}

void ShillThirdPartyVpnDriverClientImpl::SetParameters(
    const std::string& object_path_value,
    const base::Value::Dict& parameters,
    StringCallback callback,
    ErrorCallback error_callback) {
  dbus::MethodCall method_call(shill::kFlimflamThirdPartyVpnInterface,
                               shill::kSetParametersFunction);
  dbus::MessageWriter writer(&method_call);
  dbus::MessageWriter array_writer(nullptr);
  writer.OpenArray("{ss}", &array_writer);
  for (auto it : parameters) {
    if (!base::Contains(valid_keys_, it.first)) {
      LOG(WARNING) << "Unknown key " << it.first;
      continue;
    }
    if (!it.second.is_string()) {
      LOG(WARNING) << "Non string value " << it.second;
      continue;
    }
    dbus::MessageWriter entry_writer(nullptr);
    array_writer.OpenDictEntry(&entry_writer);
    entry_writer.AppendString(it.first);
    entry_writer.AppendString(it.second.GetString());
    array_writer.CloseContainer(&entry_writer);
  }
  writer.CloseContainer(&array_writer);
  GetHelper(object_path_value)
      ->CallStringMethodWithErrorCallback(&method_call, std::move(callback),
                                          std::move(error_callback));
}

void ShillThirdPartyVpnDriverClientImpl::UpdateConnectionState(
    const std::string& object_path_value,
    const uint32_t connection_state,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  dbus::MethodCall method_call(shill::kFlimflamThirdPartyVpnInterface,
                               shill::kUpdateConnectionStateFunction);
  dbus::MessageWriter writer(&method_call);
  writer.AppendUint32(connection_state);
  GetHelper(object_path_value)
      ->CallVoidMethodWithErrorCallback(&method_call, std::move(callback),
                                        std::move(error_callback));
}

void ShillThirdPartyVpnDriverClientImpl::SendPacket(
    const std::string& object_path_value,
    const std::vector<char>& ip_packet,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  dbus::MethodCall method_call(shill::kFlimflamThirdPartyVpnInterface,
                               shill::kSendPacketFunction);
  dbus::MessageWriter writer(&method_call);
  static_assert(sizeof(uint8_t) == sizeof(char),
                "Can't reinterpret ip_packet if char is not 8 bit large.");
  writer.AppendArrayOfBytes(base::as_byte_span(ip_packet));
  GetHelper(object_path_value)
      ->CallVoidMethodWithErrorCallback(&method_call, std::move(callback),
                                        std::move(error_callback));
}

// static
void ShillThirdPartyVpnDriverClientImpl::OnPacketReceived(
    base::WeakPtr<HelperInfo> helper_info,
    dbus::Signal* signal) {
  if (!helper_info || !helper_info->observer())
    return;

  dbus::MessageReader reader(signal);
  const uint8_t* data = nullptr;
  size_t length = 0;
  if (reader.PopArrayOfBytes(&data, &length)) {
    helper_info->observer()->OnPacketReceived(
        std::vector<char>(data, data + length));
  }
}

// static
void ShillThirdPartyVpnDriverClientImpl::OnPlatformMessage(
    base::WeakPtr<HelperInfo> helper_info,
    dbus::Signal* signal) {
  if (!helper_info || !helper_info->observer())
    return;

  dbus::MessageReader reader(signal);
  uint32_t platform_message = 0;
  if (reader.PopUint32(&platform_message))
    helper_info->observer()->OnPlatformMessage(platform_message);
}

// static
void ShillThirdPartyVpnDriverClientImpl::OnSignalConnected(
    const std::string& interface,
    const std::string& signal,
    bool success) {
  LOG_IF(ERROR, !success) << "Connect to " << interface << " " << signal
                          << " failed.";
}

ShillClientHelper* ShillThirdPartyVpnDriverClientImpl::GetHelper(
    const std::string& object_path_value) {
  return GetHelperInfo(object_path_value)->helper();
}

ShillThirdPartyVpnDriverClientImpl::HelperInfo*
ShillThirdPartyVpnDriverClientImpl::FindHelperInfo(
    const dbus::ObjectPath& object_path) {
  HelperMap::iterator it = helpers_.find(object_path.value());
  return (it != helpers_.end()) ? it->second : nullptr;
}

ShillThirdPartyVpnDriverClientImpl::HelperInfo*
ShillThirdPartyVpnDriverClientImpl::GetHelperInfo(
    const std::string& object_path_value) {
  dbus::ObjectPath object_path(object_path_value);
  HelperInfo* helper_info = FindHelperInfo(object_path);
  if (helper_info)
    return helper_info;

  // There is no helper for the profile, create it.
  dbus::ObjectProxy* object_proxy =
      bus_->GetObjectProxy(shill::kFlimflamServiceName, object_path);
  helper_info = new HelperInfo(object_proxy);
  helpers_[object_path_value] = helper_info;
  return helper_info;
}

}  // namespace

ShillThirdPartyVpnDriverClient::ShillThirdPartyVpnDriverClient() {
  DCHECK(!g_instance);
  g_instance = this;
}

ShillThirdPartyVpnDriverClient::~ShillThirdPartyVpnDriverClient() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
void ShillThirdPartyVpnDriverClient::Initialize(dbus::Bus* bus) {
  DCHECK(bus);
  new ShillThirdPartyVpnDriverClientImpl(bus);
}

// static
void ShillThirdPartyVpnDriverClient::InitializeFake() {
  // If a browser test creates a custom fake, don't override it.
  if (!FakeShillThirdPartyVpnDriverClient::Get())
    new FakeShillThirdPartyVpnDriverClient();
}

// static
void ShillThirdPartyVpnDriverClient::Shutdown() {
  DCHECK(g_instance);
  delete g_instance;
}

// static
ShillThirdPartyVpnDriverClient* ShillThirdPartyVpnDriverClient::Get() {
  return g_instance;
}

}  // namespace ash
