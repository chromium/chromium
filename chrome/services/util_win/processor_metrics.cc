// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/util_win/processor_metrics.h"

#include <objbase.h>

#include <sysinfoapi.h>
#include <wbemidl.h>
#include <winbase.h>
#include <wrl/client.h>

#include <string_view>

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/win/com_init_util.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/scoped_variant.h"
#include "base/win/windows_version.h"
#include "base/win/wmi.h"
#include "chrome/services/util_win/public/mojom/util_win.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

using base::win::ScopedBstr;
using base::win::ScopedVariant;
using Microsoft::WRL::ComPtr;

namespace {

HRESULT GetClassObject(ComPtr<IWbemClassObject> class_object,
                       const wchar_t* const name,
                       ScopedVariant* variant) {
  return class_object->Get(name, 0, variant->Receive(), 0, 0);
}

void RecordHypervStatusFromWMI(const ComPtr<IWbemServices>& services) {
  static constexpr wchar_t kHypervPresent[] = L"HypervisorPresent";
  static constexpr wchar_t kQueryProcessor[] =
      L"SELECT HypervisorPresent FROM Win32_ComputerSystem";

  ComPtr<IEnumWbemClassObject> enumerator_computer_system;
  HRESULT hr =
      services->ExecQuery(ScopedBstr(L"WQL").Get(), ScopedBstr(kQueryProcessor).Get(),
                          WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
                          nullptr, &enumerator_computer_system);
  if (FAILED(hr) || !enumerator_computer_system.Get())
    return;

  ComPtr<IWbemClassObject> class_object;
  ULONG items_returned = 0;
  hr = enumerator_computer_system->Next(WBEM_INFINITE, 1, &class_object,
                                        &items_returned);
  if (FAILED(hr) || !items_returned)
    return;

  ScopedVariant hyperv_present;
  hr = GetClassObject(class_object, kHypervPresent, &hyperv_present);
  if (SUCCEEDED(hr) && hyperv_present.type() == VT_BOOL) {
    base::UmaHistogramBoolean("Windows.HypervPresent",
                              V_BOOL(hyperv_present.ptr()));
  }
}

void RecordProcessorMetricsFromWMI(const ComPtr<IWbemServices>& services) {
  static constexpr wchar_t kProcessorFamily[] = L"Family";
  static constexpr wchar_t kProcessorVirtualizationFirmwareEnabled[] =
      L"VirtualizationFirmwareEnabled";
  static constexpr wchar_t kQueryProcessor[] =
      L"SELECT Family,VirtualizationFirmwareEnabled FROM Win32_Processor";

  ComPtr<IEnumWbemClassObject> enumerator_processor;
  HRESULT hr =
      services->ExecQuery(ScopedBstr(L"WQL").Get(), ScopedBstr(kQueryProcessor).Get(),
                          WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
                          nullptr, &enumerator_processor);
  if (FAILED(hr) || !enumerator_processor.Get())
    return;

  ComPtr<IWbemClassObject> class_object;
  ULONG items_returned = 0;
  hr = enumerator_processor->Next(WBEM_INFINITE, 1, &class_object,
                                  &items_returned);
  if (FAILED(hr) || !items_returned)
    return;

  ScopedVariant processor_family;
  hr = GetClassObject(class_object, kProcessorFamily, &processor_family);
  if (SUCCEEDED(hr) && processor_family.type() == VT_I4) {
    base::UmaHistogramSparse("Windows.ProcessorFamily",
                             V_I4(processor_family.ptr()));
  }

  ScopedVariant enabled;
  hr = GetClassObject(class_object, kProcessorVirtualizationFirmwareEnabled,
                      &enabled);
  if (SUCCEEDED(hr) && enabled.type() == VT_BOOL) {
    base::UmaHistogramBoolean("Windows.ProcessorVirtualizationFirmwareEnabled",
                              V_BOOL(enabled.ptr()));
  }
}

// TODO(crbug.com/40152192) Can be removed once CET support is stable.
void RecordCetAvailability() {
  bool available = false;
  auto is_user_cet_available_in_environment =
      reinterpret_cast<decltype(&IsUserCetAvailableInEnvironment)>(
          ::GetProcAddress(::GetModuleHandleW(L"kernel32.dll"),
                           "IsUserCetAvailableInEnvironment"));

  if (is_user_cet_available_in_environment) {
    available = is_user_cet_available_in_environment(
        USER_CET_ENVIRONMENT_WIN32_PROCESS);
  }
  base::UmaHistogramBoolean("Windows.CetAvailable", available);

  if (available) {
    PROCESS_MITIGATION_USER_SHADOW_STACK_POLICY policy = {0};
    if (::GetProcessMitigationPolicy(GetCurrentProcess(),
                                     ProcessUserShadowStackPolicy, &policy,
                                     sizeof(policy))) {
      base::UmaHistogramBoolean("Windows.CetEnabled",
                                policy.EnableUserShadowStack);
    }
  }
}

void RecordEnclaveAvailabilityInternal(std::string_view type,
                                       DWORD enclave_type) {
  // This API does not appear to be exported from kernel32.dll on
  // Windows 10.0.10240.
  static auto is_enclave_type_supported_func =
      reinterpret_cast<decltype(&IsEnclaveTypeSupported)>(::GetProcAddress(
          ::GetModuleHandleW(L"kernel32.dll"), "IsEnclaveTypeSupported"));

  bool is_supported = false;

  if (is_enclave_type_supported_func) {
    is_supported = is_enclave_type_supported_func(enclave_type);
  }

  base::UmaHistogramBoolean(
      base::StrCat({"Windows.Enclave.", type, ".Available"}), is_supported);
}

void RecordEnclaveAvailability() {
  RecordEnclaveAvailabilityInternal("SGX", ENCLAVE_TYPE_SGX);
  RecordEnclaveAvailabilityInternal("SGX2", ENCLAVE_TYPE_SGX2);
  RecordEnclaveAvailabilityInternal("VBS", ENCLAVE_TYPE_VBS);
  RecordEnclaveAvailabilityInternal("VBSBasic", ENCLAVE_TYPE_VBS_BASIC);
}

void RecordProcessorMetrics() {
  // These metrics do not require a WMI connection.
  RecordCetAvailability();
  RecordEnclaveAvailability();

  {
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);
    ComPtr<IWbemServices> wmi_services;
    if (!base::win::CreateLocalWmiConnection(true, &wmi_services)) {
      return;
    }
    RecordProcessorMetricsFromWMI(wmi_services);
    RecordHypervStatusFromWMI(wmi_services);
  }
}

}  // namespace

void RecordProcessorMetricsForTesting() {
  RecordProcessorMetrics();
}

ProcessorMetricsImpl::ProcessorMetricsImpl(
    mojo::PendingReceiver<chrome::mojom::ProcessorMetrics> receiver)
    : receiver_(this, std::move(receiver)) {}

ProcessorMetricsImpl::~ProcessorMetricsImpl() = default;

void ProcessorMetricsImpl::RecordProcessorMetrics(
    RecordProcessorMetricsCallback callback) {
  // TODO(sebmarchand): Check if we should move the ScopedCOMInitializer to the
  // ProcessorMetrics class.
  base::win::ScopedCOMInitializer scoped_com_initializer;
  ::RecordProcessorMetrics();
  std::move(callback).Run();
}
