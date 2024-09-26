// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/elevation_service/elevation_service_delegate.h"

#include <wrl/module.h>

#include <utility>

#include "base/command_line.h"
#include "base/containers/heap_array.h"
#include "base/logging.h"
#include "base/types/expected.h"
#include "chrome/common/win/eventlog_messages.h"
#include "chrome/elevation_service/elevated_recovery_impl.h"
#include "chrome/elevation_service/elevator.h"
#include "chrome/install_static/install_util.h"

namespace elevation_service {

uint16_t Delegate::GetLogEventCategory() {
  return ELEVATION_SERVICE_CATEGORY;
}

uint32_t Delegate::GetLogEventMessageId() {
  return MSG_ELEVATION_SERVICE_LOG_MESSAGE;
}

base::expected<base::HeapArray<FactoryAndClsid>, HRESULT>
Delegate::CreateClassFactories() {
  unsigned int flags = Microsoft::WRL::ModuleType::OutOfProc;

  auto result = base::HeapArray<FactoryAndClsid>::WithSize(1);
  Microsoft::WRL::ComPtr<IUnknown> unknown;
  HRESULT hr = Microsoft::WRL::Details::CreateClassFactory<
      Microsoft::WRL::SimpleClassFactory<Elevator>>(
      &flags, nullptr, __uuidof(IClassFactory), &unknown);
  if (SUCCEEDED(hr)) {
    hr = unknown.As(&result[0].factory);
  }
  if (FAILED(hr)) {
    return base::unexpected(hr);
  }

  result[0].clsid = base::CommandLine::ForCurrentProcess()->HasSwitch(
                        switches::kElevatorClsIdForTestingSwitch)
                        ? kTestElevatorClsid
                        : install_static::GetElevatorClsid();
  return base::ok(std::move(result));
}

void Delegate::PreRun() {
  if (auto hresult = CleanupChromeRecoveryDirectory(); FAILED(hresult)) {
    LOG(WARNING) << "Failed to clean Chrome recovery directory; hresult = "
                 << std::hex << hresult;
  }
}

}  // namespace elevation_service
