// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/printscanmgr/printscanmgr_client.h"

#include <stdint.h>

#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/dbus/printscanmgr/fake_printscanmgr_client.h"
#include "chromeos/dbus/common/dbus_library_error.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

namespace {

PrintscanmgrClient* g_instance = nullptr;

// The PrintscanmgrClient implementation used in production.
class PrintscanmgrClientImpl : public PrintscanmgrClient {
 public:
  PrintscanmgrClientImpl() = default;
  PrintscanmgrClientImpl(const PrintscanmgrClientImpl&) = delete;
  PrintscanmgrClientImpl& operator=(const PrintscanmgrClientImpl&) = delete;
  ~PrintscanmgrClientImpl() override = default;

  // DBusClient overrides:
  void Init(dbus::Bus* bus) override {
    printscanmgr_proxy_ = bus->GetObjectProxy(
        printscanmgr::kPrintscanmgrServiceName,
        dbus::ObjectPath(printscanmgr::kPrintscanmgrServicePath));
  }

  // PrintscanmgrClient overrides:
  void CupsAddManuallyConfiguredPrinter(
      const std::string& name,
      const std::string& uri,
      const std::string& ppd_contents,
      PrintscanmgrClient::CupsAddPrinterCallback callback) override {
    dbus::MethodCall method_call(
        printscanmgr::kPrintscanmgrInterface,
        printscanmgr::kCupsAddManuallyConfiguredPrinter);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(name);
    writer.AppendString(uri);
    writer.AppendArrayOfBytes(
        reinterpret_cast<const uint8_t*>(ppd_contents.data()),
        ppd_contents.size());

    printscanmgr_proxy_->CallMethodWithErrorResponse(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&PrintscanmgrClientImpl::OnPrinterAdded,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void CupsAddAutoConfiguredPrinter(
      const std::string& name,
      const std::string& uri,
      PrintscanmgrClient::CupsAddPrinterCallback callback) override {
    dbus::MethodCall method_call(printscanmgr::kPrintscanmgrInterface,
                                 printscanmgr::kCupsAddAutoConfiguredPrinter);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(name);
    writer.AppendString(uri);

    printscanmgr_proxy_->CallMethodWithErrorResponse(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&PrintscanmgrClientImpl::OnPrinterAdded,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void CupsRemovePrinter(const std::string& name,
                         PrintscanmgrClient::CupsRemovePrinterCallback callback,
                         base::OnceClosure error_callback) override {
    dbus::MethodCall method_call(printscanmgr::kPrintscanmgrInterface,
                                 printscanmgr::kCupsRemovePrinter);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(name);

    printscanmgr_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&PrintscanmgrClientImpl::OnPrinterRemoved,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                       std::move(error_callback)));
  }

  void CupsRetrievePrinterPpd(
      const std::string& name,
      PrintscanmgrClient::CupsRetrievePrinterPpdCallback callback,
      base::OnceClosure error_callback) override {
    dbus::MethodCall method_call(printscanmgr::kPrintscanmgrInterface,
                                 printscanmgr::kCupsRetrievePpd);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(name);

    printscanmgr_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&PrintscanmgrClientImpl::OnRetrievedPrinterPpd,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                       std::move(error_callback)));
  }

 private:
  void OnPrinterAdded(CupsAddPrinterCallback callback,
                      dbus::Response* response,
                      dbus::ErrorResponse* err_response) {
    if (!response) {
      SendErrorResponse(std::move(callback), err_response);
      return;
    }

    int32_t result;
    if (!dbus::MessageReader(response).PopInt32(&result)) {
      SendErrorResponse(std::move(callback), err_response);
      return;
    }

    DCHECK_GE(result, 0);
    std::move(callback).Run(result);
  }

  void OnPrinterRemoved(CupsRemovePrinterCallback callback,
                        base::OnceClosure error_callback,
                        dbus::Response* response) {
    if (!response) {
      std::move(error_callback).Run();
      return;
    }

    bool result;
    if (!dbus::MessageReader(response).PopBool(&result)) {
      std::move(error_callback).Run();
      return;
    }

    std::move(callback).Run(result);
  }

  void OnRetrievedPrinterPpd(CupsRetrievePrinterPpdCallback callback,
                             base::OnceClosure error_callback,
                             dbus::Response* response) {
    size_t length = 0;
    const uint8_t* bytes = nullptr;

    if (!(response &&
          dbus::MessageReader(response).PopArrayOfBytes(&bytes, &length)) ||
        length == 0 || bytes == nullptr) {
      LOG(ERROR) << "Failed to retrieve printer PPD";
      std::move(error_callback).Run();
      return;
    }

    std::vector<uint8_t> data(bytes, bytes + length);
    std::move(callback).Run(data);
  }

  void SendErrorResponse(CupsAddPrinterCallback callback,
                         dbus::ErrorResponse* err_response) {
    chromeos::DBusLibraryError dbus_error =
        chromeos::DBusLibraryError::kGenericError;
    if (err_response) {
      dbus::MessageReader err_reader(err_response);
      std::string err_str = err_response->GetErrorName();
      dbus_error = chromeos::DBusLibraryErrorFromString(err_str);
    }

    std::move(callback).Run(dbus_error);
  }

  raw_ptr<dbus::ObjectProxy, ExperimentalAsh> printscanmgr_proxy_ = nullptr;
  base::WeakPtrFactory<PrintscanmgrClientImpl> weak_ptr_factory_{this};
};

}  // namespace

// static
PrintscanmgrClient* PrintscanmgrClient::Get() {
  return g_instance;
}

// static
void PrintscanmgrClient::Initialize(dbus::Bus* bus) {
  CHECK(bus);
  CHECK(!g_instance);
  g_instance = new PrintscanmgrClientImpl();
  g_instance->Init(bus);
}

// static
void PrintscanmgrClient::InitializeFake() {
  CHECK(!g_instance);
  g_instance = new FakePrintscanmgrClient();
  g_instance->Init(nullptr);
}

// static
void PrintscanmgrClient::Shutdown() {
  CHECK(g_instance);
  delete g_instance;
  g_instance = nullptr;
}

PrintscanmgrClient::PrintscanmgrClient() = default;
PrintscanmgrClient::~PrintscanmgrClient() = default;

}  // namespace ash
