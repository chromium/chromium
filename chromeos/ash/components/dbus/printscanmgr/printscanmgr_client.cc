// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/printscanmgr/printscanmgr_client.h"

#include <stdint.h>

#include <dbus/dbus-protocol.h>

#include <map>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "chromeos/ash/components/dbus/printscanmgr/fake_printscanmgr_client.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

namespace {

PrintscanmgrClient* g_instance = nullptr;

printscanmgr::AddPrinterResult DBusErrorFromString(
    const std::string& dbus_error_string) {
  static const base::NoDestructor<
      std::map<std::string, printscanmgr::AddPrinterResult>>
      error_string_map({
          {DBUS_ERROR_NO_REPLY,
           printscanmgr::AddPrinterResult::ADD_PRINTER_RESULT_DBUS_NO_REPLY},
          {DBUS_ERROR_TIMEOUT,
           printscanmgr::AddPrinterResult::ADD_PRINTER_RESULT_DBUS_TIMEOUT},
      });

  auto it = error_string_map->find(dbus_error_string);
  return it != error_string_map->end()
             ? it->second
             : printscanmgr::AddPrinterResult::ADD_PRINTER_RESULT_DBUS_GENERIC;
}

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
      const printscanmgr::CupsAddManuallyConfiguredPrinterRequest& request,
      chromeos::DBusMethodCallback<
          printscanmgr::CupsAddManuallyConfiguredPrinterResponse> callback)
      override {
    dbus::MethodCall method_call(
        printscanmgr::kPrintscanmgrInterface,
        printscanmgr::kCupsAddManuallyConfiguredPrinter);
    dbus::MessageWriter writer(&method_call);
    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to encode CupsAddManuallyConfiguredPrinterRequest "
                    "protobuf.";
      printscanmgr::CupsAddManuallyConfiguredPrinterResponse response;
      response.set_result(printscanmgr::AddPrinterResult::
                              ADD_PRINTER_RESULT_DBUS_ENCODING_FAILURE);
      std::move(callback).Run(std::move(response));
      return;
    }

    printscanmgr_proxy_->CallMethodWithErrorResponse(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(
            &PrintscanmgrClientImpl::OnPrinterAdded<
                printscanmgr::CupsAddManuallyConfiguredPrinterResponse>,
            weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void CupsAddAutoConfiguredPrinter(
      const printscanmgr::CupsAddAutoConfiguredPrinterRequest& request,
      chromeos::DBusMethodCallback<
          printscanmgr::CupsAddAutoConfiguredPrinterResponse> callback)
      override {
    dbus::MethodCall method_call(printscanmgr::kPrintscanmgrInterface,
                                 printscanmgr::kCupsAddAutoConfiguredPrinter);
    dbus::MessageWriter writer(&method_call);
    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to encode CupsAddAutoConfiguredPrinterRequest "
                    "protobuf.";
      printscanmgr::CupsAddAutoConfiguredPrinterResponse response;
      response.set_result(printscanmgr::AddPrinterResult::
                              ADD_PRINTER_RESULT_DBUS_ENCODING_FAILURE);
      std::move(callback).Run(std::move(response));
      return;
    }

    printscanmgr_proxy_->CallMethodWithErrorResponse(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&PrintscanmgrClientImpl::OnPrinterAdded<
                           printscanmgr::CupsAddAutoConfiguredPrinterResponse>,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void CupsRemovePrinter(
      const printscanmgr::CupsRemovePrinterRequest& request,
      chromeos::DBusMethodCallback<printscanmgr::CupsRemovePrinterResponse>
          callback,
      base::OnceClosure error_callback) override {
    dbus::MethodCall method_call(printscanmgr::kPrintscanmgrInterface,
                                 printscanmgr::kCupsRemovePrinter);
    dbus::MessageWriter writer(&method_call);
    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to encode CupsRemovePrinterRequest protobuf.";
      std::move(error_callback).Run();
      return;
    }

    printscanmgr_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&PrintscanmgrClientImpl::OnPrinterRemoved,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                       std::move(error_callback)));
  }

  void CupsRetrievePrinterPpd(
      const printscanmgr::CupsRetrievePpdRequest& request,
      chromeos::DBusMethodCallback<printscanmgr::CupsRetrievePpdResponse>
          callback,
      base::OnceClosure error_callback) override {
    dbus::MethodCall method_call(printscanmgr::kPrintscanmgrInterface,
                                 printscanmgr::kCupsRetrievePpd);
    dbus::MessageWriter writer(&method_call);
    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to encode CupsRetrievePpdRequest protobuf.";
      std::move(error_callback).Run();
      return;
    }

    printscanmgr_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&PrintscanmgrClientImpl::OnRetrievedPrinterPpd,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                       std::move(error_callback)));
  }

 private:
  template <typename T>
  void OnPrinterAdded(chromeos::DBusMethodCallback<T> callback,
                      dbus::Response* response,
                      dbus::ErrorResponse* err_response) {
    if (!response) {
      SendErrorResponse(std::move(callback), err_response);
      return;
    }

    T result;
    if (!dbus::MessageReader(response).PopArrayOfBytesAsProto(&result)) {
      SendErrorResponse(std::move(callback), err_response);
      return;
    }

    DCHECK_GE(result.result(), 0);
    std::move(callback).Run(result);
  }

  void OnPrinterRemoved(chromeos::DBusMethodCallback<
                            printscanmgr::CupsRemovePrinterResponse> callback,
                        base::OnceClosure error_callback,
                        dbus::Response* response) {
    if (!response) {
      std::move(error_callback).Run();
      return;
    }

    printscanmgr::CupsRemovePrinterResponse result;
    if (!dbus::MessageReader(response).PopArrayOfBytesAsProto(&result)) {
      std::move(error_callback).Run();
      return;
    }

    std::move(callback).Run(result);
  }

  void OnRetrievedPrinterPpd(
      chromeos::DBusMethodCallback<printscanmgr::CupsRetrievePpdResponse>
          callback,
      base::OnceClosure error_callback,
      dbus::Response* response) {
    printscanmgr::CupsRetrievePpdResponse result;
    if (!(response &&
          dbus::MessageReader(response).PopArrayOfBytesAsProto(&result))) {
      LOG(ERROR) << "Failed to retrieve printer PPD";
      std::move(error_callback).Run();
      return;
    }

    std::move(callback).Run(result);
  }

  template <typename T>
  void SendErrorResponse(chromeos::DBusMethodCallback<T> callback,
                         dbus::ErrorResponse* err_response) {
    printscanmgr::AddPrinterResult dbus_error =
        printscanmgr::AddPrinterResult::ADD_PRINTER_RESULT_DBUS_GENERIC;
    if (err_response) {
      dbus::MessageReader err_reader(err_response);
      std::string err_str = err_response->GetErrorName();
      dbus_error = DBusErrorFromString(err_str);
    }

    T response;
    response.set_result(dbus_error);
    std::move(callback).Run(response);
  }

  raw_ptr<dbus::ObjectProxy, LeakedDanglingUntriaged> printscanmgr_proxy_ =
      nullptr;
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
void PrintscanmgrClient::InitializeFakeForTest() {
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
