// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chromeos/dbus/dlp/dlp_client.h"

#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/strings/strcat.h"
#include "base/task/single_thread_task_runner.h"
#include "chromeos/dbus/dlp/dlp_service.pb.h"
#include "chromeos/dbus/dlp/fake_dlp_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/dlp/dbus-constants.h"

namespace chromeos {
namespace {

DlpClient* g_instance = nullptr;

const char kDbusCallFailure[] = "Failed to call dlp.";
const char kProtoMessageParsingFailure[] =
    "Failed to parse response message from dlp.";

// Tries to parse a proto message from |response| into |proto| and returns null
// if successful. If |response| is nullptr or the message cannot be parsed it
// will return an appropriate error message.
const char* DeserializeProto(dbus::Response* response,
                             google::protobuf::MessageLite* proto) {
  if (!response) {
    return kDbusCallFailure;
  }

  dbus::MessageReader reader(response);
  if (!reader.PopArrayOfBytesAsProto(proto)) {
    return kProtoMessageParsingFailure;
  }

  return nullptr;
}

// "Real" implementation of DlpClient talking to the Dlp daemon
// on the Chrome OS side.
class DlpClientImpl : public DlpClient {
 public:
  DlpClientImpl() = default;
  DlpClientImpl(const DlpClientImpl&) = delete;
  DlpClientImpl& operator=(const DlpClientImpl&) = delete;
  ~DlpClientImpl() override = default;

  void Init(dbus::Bus* bus) {
    proxy_ = bus->GetObjectProxy(dlp::kDlpServiceName,
                                 dbus::ObjectPath(dlp::kDlpServicePath));

    proxy_->SetNameOwnerChangedCallback(base::BindRepeating(
        &DlpClientImpl::NameOwnerChangedReceived, weak_factory_.GetWeakPtr()));
  }

  void SetDlpFilesPolicy(const dlp::SetDlpFilesPolicyRequest request,
                         SetDlpFilesPolicyCallback callback) override {
    dbus::MethodCall method_call(dlp::kDlpInterface,
                                 dlp::kSetDlpFilesPolicyMethod);
    dbus::MessageWriter writer(&method_call);

    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      dlp::SetDlpFilesPolicyResponse response;
      response.set_error_message(base::StrCat(
          {"Failure to call d-bus method: ", dlp::kSetDlpFilesPolicyMethod}));
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), response));
      return;
    }

    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DlpClientImpl::HandleSetDlpFilesPolicyResponse,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }

  void AddFiles(const dlp::AddFilesRequest request,
                AddFilesCallback callback) override {
    dbus::MethodCall method_call(dlp::kDlpInterface, dlp::kAddFilesMethod);
    dbus::MessageWriter writer(&method_call);

    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      dlp::AddFilesResponse response;
      response.set_error_message(base::StrCat(
          {"Failure to call d-bus method: ", dlp::kAddFilesMethod}));
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), response));
      return;
    }

    proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                       base::BindOnce(&DlpClientImpl::HandleAddFilesResponse,
                                      weak_factory_.GetWeakPtr(),
                                      std::move(request), std::move(callback)));
  }

  void GetFilesSources(const dlp::GetFilesSourcesRequest request,
                       GetFilesSourcesCallback callback) override {
    dbus::MethodCall method_call(dlp::kDlpInterface,
                                 dlp::kGetFilesSourcesMethod);
    dbus::MessageWriter writer(&method_call);

    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      dlp::GetFilesSourcesResponse response;
      response.set_error_message(base::StrCat(
          {"Failure to call d-bus method: ", dlp::kGetFilesSourcesMethod}));
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), response));
      return;
    }

    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DlpClientImpl::HandleGetFilesSourcesResponse,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }

  void CheckFilesTransfer(const dlp::CheckFilesTransferRequest request,
                          CheckFilesTransferCallback callback) override {
    dbus::MethodCall method_call(dlp::kDlpInterface,
                                 dlp::kCheckFilesTransferMethod);
    dbus::MessageWriter writer(&method_call);

    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      dlp::CheckFilesTransferResponse response;
      response.set_error_message(base::StrCat(
          {"Failure to call d-bus method: ", dlp::kCheckFilesTransferMethod}));
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), response));
      return;
    }

    proxy_->CallMethod(
        &method_call, base::Minutes(6).InMilliseconds(),
        base::BindOnce(&DlpClientImpl::HandleCheckFilesTransferResponse,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }

  void RequestFileAccess(const dlp::RequestFileAccessRequest request,
                         RequestFileAccessCallback callback) override {
    dbus::MethodCall method_call(dlp::kDlpInterface,
                                 dlp::kRequestFileAccessMethod);
    dbus::MessageWriter writer(&method_call);

    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      dlp::RequestFileAccessResponse response;
      response.set_error_message(base::StrCat(
          {"Failure to call d-bus method: ", dlp::kRequestFileAccessMethod}));
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(callback), response, base::ScopedFD()));
      return;
    }

    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DlpClientImpl::HandleRequestFileAccessResponse,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }

  void GetDatabaseEntries(GetDatabaseEntriesCallback callback) override {
    dbus::MethodCall method_call(dlp::kDlpInterface,
                                 dlp::kGetDatabaseEntriesMethod);
    dbus::MessageWriter writer(&method_call);

    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DlpClientImpl::HandleGetDatabaseEntriesResponse,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }

  bool IsAlive() const override { return is_alive_; }

  void AddObserver(Observer* observer) override {
    observers_.AddObserver(observer);
  }

  void RemoveObserver(Observer* observer) override {
    observers_.RemoveObserver(observer);
  }

  bool HasObserver(const Observer* observer) const override {
    return observers_.HasObserver(observer);
  }

 private:
  TestInterface* GetTestInterface() override { return nullptr; }

  void HandleSetDlpFilesPolicyResponse(SetDlpFilesPolicyCallback callback,
                                       dbus::Response* response) {
    dlp::SetDlpFilesPolicyResponse response_proto;
    const char* error_message = DeserializeProto(response, &response_proto);
    if (error_message) {
      response_proto.set_error_message(error_message);
    }
    if (!response_proto.has_error_message()) {
      is_alive_ = true;
    }
    std::move(callback).Run(response_proto);
  }

  void HandleAddFilesResponse(const dlp::AddFilesRequest request,
                              AddFilesCallback callback,
                              dbus::Response* response) {
    dlp::AddFilesResponse response_proto;
    const char* error_message = DeserializeProto(response, &response_proto);
    if (error_message) {
      response_proto.set_error_message(error_message);
    } else {
      std::vector<base::FilePath> added_files;
      for (const auto& add_file_request : request.add_file_requests()) {
        added_files.emplace_back(add_file_request.file_path());
      }
      for (auto& observer : observers_) {
        observer.OnFilesAddedToDlpDaemon(added_files);
      }
    }
    std::move(callback).Run(response_proto);
  }

  void HandleGetFilesSourcesResponse(GetFilesSourcesCallback callback,
                                     dbus::Response* response) {
    dlp::GetFilesSourcesResponse response_proto;
    const char* error_message = DeserializeProto(response, &response_proto);
    if (error_message) {
      response_proto.set_error_message(error_message);
    }
    std::move(callback).Run(response_proto);
  }

  void HandleCheckFilesTransferResponse(CheckFilesTransferCallback callback,
                                        dbus::Response* response) {
    dlp::CheckFilesTransferResponse response_proto;
    const char* error_message = DeserializeProto(response, &response_proto);
    if (error_message) {
      response_proto.set_error_message(error_message);
    }
    std::move(callback).Run(response_proto);
  }

  void HandleRequestFileAccessResponse(RequestFileAccessCallback callback,
                                       dbus::Response* response) {
    dlp::RequestFileAccessResponse response_proto;
    base::ScopedFD fd;
    if (!response) {
      response_proto.set_error_message(kDbusCallFailure);
      std::move(callback).Run(response_proto, std::move(fd));
      return;
    }

    dbus::MessageReader reader(response);
    if (!reader.PopArrayOfBytesAsProto(&response_proto)) {
      response_proto.set_error_message(kProtoMessageParsingFailure);
      std::move(callback).Run(response_proto, std::move(fd));
      return;
    }
    if (!reader.PopFileDescriptor(&fd)) {
      response_proto.set_error_message(kProtoMessageParsingFailure);
      std::move(callback).Run(response_proto, std::move(fd));
      return;
    }
    std::move(callback).Run(response_proto, std::move(fd));
  }

  void HandleGetDatabaseEntriesResponse(GetDatabaseEntriesCallback callback,
                                        dbus::Response* response) {
    dlp::GetDatabaseEntriesResponse response_proto;
    const char* error_message = DeserializeProto(response, &response_proto);
    if (error_message) {
      response_proto.set_error_message(error_message);
    }
    std::move(callback).Run(response_proto);
  }

  void NameOwnerChangedReceived(const std::string& old_owner,
                                const std::string& new_owner) {
    is_alive_ = false;
    // Do not notify if the service was shut down, only if a new one is started.
    if (new_owner.empty()) {
      return;
    }
    for (auto& observer : observers_) {
      observer.DlpDaemonRestarted();
    }
  }

  // D-Bus proxy for the Dlp daemon, not owned.
  raw_ptr<dbus::ObjectProxy> proxy_ = nullptr;

  // Indicates whether the daemon was started and DLP Files rules are enforced.
  bool is_alive_ = false;

  base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<DlpClientImpl> weak_factory_{this};
};

}  // namespace

DlpClient::DlpClient() {
  CHECK(!g_instance);
  g_instance = this;
}

DlpClient::~DlpClient() {
  CHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
void DlpClient::Initialize(dbus::Bus* bus) {
  CHECK(bus);
  (new DlpClientImpl())->Init(bus);
}

// static
void DlpClient::InitializeFake() {
  new FakeDlpClient();
}

// static
void DlpClient::Shutdown() {
  CHECK(g_instance);
  delete g_instance;
}

// static
DlpClient* DlpClient::Get() {
  return g_instance;
}

}  // namespace chromeos
