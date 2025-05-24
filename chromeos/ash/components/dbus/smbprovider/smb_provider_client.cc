// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/smbprovider/smb_provider_client.h"

#include <memory>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/dbus/smbprovider/fake_smb_provider_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"

namespace ash {

// Extend the timeout for the `GetShares` D-Bus method as share discovery
// has been observed to take longer than 25s (the default D-Bus timeout).
constexpr base::TimeDelta kGetSharesTimeout = base::Minutes(1);

namespace {

SmbProviderClient* g_instance = nullptr;

smbprovider::ErrorType GetErrorFromReader(dbus::MessageReader* reader) {
  int32_t int_error;
  if (!reader->PopInt32(&int_error) ||
      !smbprovider::ErrorType_IsValid(int_error)) {
    DLOG(ERROR)
        << "SmbProviderClient: Failed to get an error from the response";
    return smbprovider::ERROR_DBUS_PARSE_FAILED;
  }
  return static_cast<smbprovider::ErrorType>(int_error);
}

smbprovider::ErrorType GetErrorAndProto(
    dbus::Response* response,
    google::protobuf::MessageLite* protobuf_out) {
  if (!response) {
    DLOG(ERROR) << "Failed to call smbprovider";
    return smbprovider::ERROR_DBUS_PARSE_FAILED;
  }
  dbus::MessageReader reader(response);
  smbprovider::ErrorType error = GetErrorFromReader(&reader);
  if (error != smbprovider::ERROR_OK) {
    return error;
  }
  if (!reader.PopArrayOfBytesAsProto(protobuf_out)) {
    DLOG(ERROR) << "Failed to parse protobuf.";
    return smbprovider::ERROR_DBUS_PARSE_FAILED;
  }
  return smbprovider::ERROR_OK;
}

class SmbProviderClientImpl final : public SmbProviderClient {
 public:
  SmbProviderClientImpl() = default;

  SmbProviderClientImpl(const SmbProviderClientImpl&) = delete;
  SmbProviderClientImpl& operator=(const SmbProviderClientImpl&) = delete;

  ~SmbProviderClientImpl() override {}

  void GetShares(const base::FilePath& server_url,
                 ReadDirectoryCallback callback) override {
    smbprovider::GetSharesOptionsProto options;
    options.set_server_url(server_url.value());
    CallMethod(smbprovider::kGetSharesMethod, options,
               kGetSharesTimeout.InMilliseconds(),
               &SmbProviderClientImpl::HandleProtoCallback<
                   smbprovider::DirectoryEntryListProto>,
               &callback);
  }

  void SetupKerberos(const std::string& account_id,
                     SetupKerberosCallback callback) override {
    dbus::MethodCall method_call(smbprovider::kSmbProviderInterface,
                                 smbprovider::kSetupKerberosMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(account_id);
    CallMethod(&method_call,
               &SmbProviderClientImpl::HandleSetupKerberosCallback, &callback);
  }

  void ParseNetBiosPacket(const std::vector<uint8_t>& packet,
                          uint16_t transaction_id,
                          ParseNetBiosPacketCallback callback) override {
    dbus::MethodCall method_call(smbprovider::kSmbProviderInterface,
                                 smbprovider::kParseNetBiosPacketMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendArrayOfBytes(packet);
    writer.AppendUint16(transaction_id);
    CallMethod(&method_call,
               &SmbProviderClientImpl::HandleParseNetBiosPacketCallback,
               &callback);
  }

  base::WeakPtr<SmbProviderClient> AsWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // chromeos::DBusClient override.
  void Init(dbus::Bus* bus) override {
    proxy_ = bus->GetObjectProxy(
        smbprovider::kSmbProviderServiceName,
        dbus::ObjectPath(smbprovider::kSmbProviderServicePath));
    DCHECK(proxy_);
  }

 private:
  // Calls the DBUS method |name|, passing the |protobuf| as an argument.
  // |handler| is the member function in this class that receives
  // the response and then passes the processed response to |callback|.
  template <typename CallbackHandler, typename Callback>
  void CallMethod(const char* name,
                  const google::protobuf::MessageLite& protobuf,
                  int timeout_ms,
                  CallbackHandler handler,
                  Callback callback) {
    dbus::MethodCall method_call(smbprovider::kSmbProviderInterface, name);
    dbus::MessageWriter writer(&method_call);
    writer.AppendProtoAsArrayOfBytes(protobuf);
    proxy_->CallMethod(&method_call, timeout_ms,
                       base::BindOnce(handler, weak_ptr_factory_.GetWeakPtr(),
                                      std::move(*callback)));
  }

  // Calls the method specified in |method_call|. |handler| is the member
  // function in this class that receives the response and then passes the
  // processed response to |callback|.
  template <typename CallbackHandler, typename Callback>
  void CallMethod(dbus::MethodCall* method_call,
                  CallbackHandler handler,
                  Callback callback) {
    proxy_->CallMethod(method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                       base::BindOnce(handler, weak_ptr_factory_.GetWeakPtr(),
                                      std::move(*callback)));
  }

  // Calls the D-Bus method |name|, passing the |protobuf| as an argument.
  // Uses the default callback handler to process |callback|.
  template <typename Callback>
  void CallDefaultMethod(const char* name,
                         const google::protobuf::MessageLite& protobuf,
                         Callback callback) {
    dbus::MethodCall method_call(smbprovider::kSmbProviderInterface, name);
    dbus::MessageWriter writer(&method_call);
    writer.AppendProtoAsArrayOfBytes(protobuf);
    CallDefaultMethod(&method_call, callback);
  }

  // Calls the method specified in |method_call|. Uses the default callback
  // handler to process |callback|.
  template <typename Callback>
  void CallDefaultMethod(dbus::MethodCall* method_call, Callback callback) {
    proxy_->CallMethod(
        method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&SmbProviderClientImpl::HandleDefaultCallback,
                       weak_ptr_factory_.GetWeakPtr(), method_call->GetMember(),
                       std::move(*callback)));
  }

  // Handles D-Bus callback for SetupKerberos.
  void HandleSetupKerberosCallback(SetupKerberosCallback callback,
                                   dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "SetupKerberos: failed to call smbprovider";
      std::move(callback).Run(false /* success */);
      return;
    }

    dbus::MessageReader reader(response);
    bool result;
    if (!reader.PopBool(&result)) {
      LOG(ERROR) << "SetupKerberos: parse failure.";
      std::move(callback).Run(false /* success */);
      return;
    }

    std::move(callback).Run(result);
  }

  void HandleParseNetBiosPacketCallback(ParseNetBiosPacketCallback callback,
                                        dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "ParseNetBiosPacket: failed to call smbprovider";
      std::move(callback).Run(std::vector<std::string>());
      return;
    }

    dbus::MessageReader reader(response);
    smbprovider::HostnamesProto proto;

    if (!reader.PopArrayOfBytesAsProto(&proto)) {
      LOG(ERROR) << "ParseNetBiosPacket: Failed to parse protobuf.";
      std::move(callback).Run(std::vector<std::string>());
      return;
    }

    std::vector<std::string> hostnames(proto.hostnames().begin(),
                                       proto.hostnames().end());
    std::move(callback).Run(hostnames);
  }

  // Default callback handler for D-Bus calls.
  void HandleDefaultCallback(const std::string& method_name,
                             StatusCallback callback,
                             dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << method_name << ": failed to call smbprovider";
      std::move(callback).Run(smbprovider::ERROR_DBUS_PARSE_FAILED);
      return;
    }
    dbus::MessageReader reader(response);
    std::move(callback).Run(GetErrorFromReader(&reader));
  }

  // Handles D-Bus responses for methods that return an error and a protobuf
  // object.
  template <class T>
  void HandleProtoCallback(base::OnceCallback<void(smbprovider::ErrorType error,
                                                   const T& response)> callback,
                           dbus::Response* response) {
    T proto;
    smbprovider::ErrorType error(GetErrorAndProto(response, &proto));
    std::move(callback).Run(error, proto);
  }

  raw_ptr<dbus::ObjectProxy> proxy_ = nullptr;

  base::WeakPtrFactory<SmbProviderClientImpl> weak_ptr_factory_{this};
};

}  // namespace

// static
SmbProviderClient* SmbProviderClient::Get() {
  return g_instance;
}

// static
void SmbProviderClient::Initialize(dbus::Bus* bus) {
  CHECK(bus);
  (new SmbProviderClientImpl())->Init(bus);
}

// static
void SmbProviderClient::InitializeFake() {
  (new FakeSmbProviderClient())->Init(nullptr);
}

// static
void SmbProviderClient::Shutdown() {
  CHECK(g_instance);
  delete g_instance;
}

SmbProviderClient::SmbProviderClient() {
  CHECK(!g_instance);
  g_instance = this;
}

SmbProviderClient::~SmbProviderClient() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

}  // namespace ash
