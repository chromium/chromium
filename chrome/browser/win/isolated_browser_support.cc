// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/isolated_browser_support.h"

#include <objbase.h>

#include <windows.h>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/process/process.h"
#include "base/strings/strcat.h"
#include "base/types/expected.h"
#include "base/win/access_token.h"
#include "base/win/com_init_util.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/security_descriptor.h"
#include "base/win/sid.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/elevation_service/elevation_service_idl.h"
#include "chrome/elevation_service/elevator.h"
#include "chrome/install_static/install_util.h"
#include "chrome/installer/util/isolation_support.h"

namespace chrome {

IsolatedBrowser::~IsolatedBrowser() = default;

IsolatedBrowser::IsolatedBrowser(base::Process process,
                                 base::win::ScopedHandle job)
    : job_(std::move(job)), process_(std::move(process)) {}

// static
base::expected<std::unique_ptr<IsolatedBrowser>, HRESULT>
IsolatedBrowser::Launch(const base::CommandLine& command_line) {
  base::win::ScopedCOMInitializer com_init;

  base::win::AssertComInitialized();

  Microsoft::WRL::ComPtr<IElevator2> elevator;
  HRESULT hr = ::CoCreateInstance(
      install_static::GetElevatorClsid(), nullptr, CLSCTX_LOCAL_SERVER,
      install_static::GetElevatorIid(), IID_PPV_ARGS_Helper(&elevator));

  if (FAILED(hr)) {
    PLOG(ERROR) << "Failed to create instance.";
    return base::unexpected(hr);
  }

  hr = ::CoSetProxyBlanket(
      elevator.Get(), RPC_C_AUTHN_DEFAULT, RPC_C_AUTHZ_DEFAULT,
      COLE_DEFAULT_PRINCIPAL, RPC_C_AUTHN_LEVEL_PKT_PRIVACY,
      RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_DYNAMIC_CLOAKING);
  if (FAILED(hr)) {
    PLOG(ERROR) << "Failed to create security blanket.";
    return base::unexpected(hr);
  }

  base::win::ScopedHandle job;

  job.Set(::CreateJobObjectW(nullptr, nullptr));
  if (!job.is_valid()) {
    PLOG(ERROR) << "Failed to create job object.";
    return base::unexpected(HRESULT_FROM_WIN32(::GetLastError()));
  }

  JOBOBJECT_EXTENDED_LIMIT_INFORMATION limit_information = {};
  limit_information.BasicLimitInformation.LimitFlags =
      JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;

  if (!::SetInformationJobObject(job.get(), JobObjectExtendedLimitInformation,
                                 &limit_information,
                                 sizeof(limit_information))) {
    PLOG(ERROR) << "Failed to set extended job limit information.";
    return base::unexpected(HRESULT_FROM_WIN32(::GetLastError()));
  }

  if (!::AssignProcessToJobObject(job.get(), ::GetCurrentProcess())) {
    PLOG(ERROR) << "Failed to place current process in job.";
    return base::unexpected(HRESULT_FROM_WIN32(::GetLastError()));
  }

  DWORD last_error = 0;
  ULONG_PTR proc_handle;
  base::win::ScopedBstr log;
  hr = elevator->RunIsolatedChrome(
      /*flags=*/0, command_line.GetCommandLineString().c_str(),
      /*log=*/log.Receive(), &proc_handle, &last_error);
  if (FAILED(hr)) {
    PLOG(ERROR) << "Failed to launch isolated browser.";
    return base::unexpected(hr);
  }

  return base::WrapUnique<IsolatedBrowser>(new IsolatedBrowser(
      base::Process(reinterpret_cast<base::ProcessHandle>(proc_handle)),
      std::move(job)));
}

int IsolatedBrowser::WaitForExit() const {
  int exit_code;
  process_.WaitForExit(&exit_code);
  return exit_code;
}

bool IsIsolationEnabled(const base::CommandLine& command_line) {
  if (!install_static::IsSystemInstall()) {
    return false;
  }
  // TODO(crbug.com/433545123): Replace with a persistent backed config.
  return command_line.HasSwitch(::switches::kLaunchIsolated);
}

bool IsRunningIsolated() {
  auto process_token = base::win::AccessToken::FromCurrentProcess();
  if (!process_token) {
    return false;
  }

  auto sa = process_token->GetSecurityAttribute(
      installer::GetIsolationAttributeName());
  // The value varies by channel, but existence of the SA means the current
  // process is isolated.
  if (sa.has_value()) {
    return true;
  }
  return false;
}

}  // namespace chrome
