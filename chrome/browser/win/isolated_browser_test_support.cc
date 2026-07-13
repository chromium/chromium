// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/isolated_browser_test_support.h"

#include <objbase.h>

#include <shlobj.h>
#include <wrl/client.h>

#include <array>

#include "base/base_switches.h"
#include "base/logging.h"
#include "base/types/expected.h"
#include "base/win/elevation_util.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_com_initializer.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/elevation_service/elevation_service_idl.h"
#include "chrome/elevation_service/elevator.h"
#include "chrome/windows_services/service_program/test_support/service_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome {

IsolatedBrowserTestEnvironment::IsolatedBrowserTestEnvironment() {
  if (!::IsUserAnAdmin()) {
    return;
  }

  service_environment_ = std::make_unique<ServiceEnvironment>(
      L"Test Elevation Service", FILE_PATH_LITERAL("elevation_service.exe"),
      std::array<std::string_view, 3>{
          elevation_service::switches::kAllowUntrustedPathForTesting,
          elevation_service::switches::kElevatorClsIdForTestingSwitch,
          elevation_service::switches::kAllowUntrustedSwitchesForTesting},
      elevation_service::kTestElevatorClsid, __uuidof(IElevator2));
  EXPECT_TRUE(service_environment_->is_valid());
}

IsolatedBrowserTestEnvironment::~IsolatedBrowserTestEnvironment() = default;

bool IsolatedBrowserTestEnvironment::is_valid() const {
  return service_environment_ && service_environment_->is_valid();
}

base::Process SpawnIsolatedMultiProcessTestChild(
    const std::string& procname,
    const base::CommandLine& base_command_line) {
  base::win::ScopedCOMInitializer com_initializer(
      base::win::ScopedCOMInitializer::kMTA);

  Microsoft::WRL::ComPtr<IElevator2> elevator;
  HRESULT hr =
      ::CoCreateInstance(elevation_service::kTestElevatorClsid, nullptr,
                         CLSCTX_LOCAL_SERVER, IID_PPV_ARGS(&elevator));

  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to create IElevator2 instance: " << hr;
    return base::Process();
  }

  hr = ::CoSetProxyBlanket(
      elevator.Get(), RPC_C_AUTHN_DEFAULT, RPC_C_AUTHZ_DEFAULT,
      COLE_DEFAULT_PRINCIPAL, RPC_C_AUTHN_LEVEL_PKT_PRIVACY,
      RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_DYNAMIC_CLOAKING);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to create security blanket.";
    return base::Process();
  }

  base::CommandLine cmd = base_command_line;
  cmd.AppendSwitchASCII(switches::kTestChildProcess, procname);
  const auto command_line = cmd.GetCommandLineString();

  DWORD last_error = 0;
  ULONG_PTR proc_handle;
  base::win::ScopedBstr log;
  auto res = elevator->RunIsolatedChrome(
      /*flags=*/0, command_line.c_str(), log.Receive(), &proc_handle,
      &last_error);
  if (FAILED(res)) {
    LOG(ERROR) << "RunIsolatedChrome failed: " << res;
    return base::Process();
  }
  if (last_error != ERROR_SUCCESS) {
    LOG(ERROR) << "RunIsolatedChrome failed with error: " << last_error;
    return base::Process();
  }

  return base::Process(reinterpret_cast<base::ProcessHandle>(proc_handle));
}

}  // namespace chrome
