// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/isolated_browser_support.h"

#include <objbase.h>

#include <windows.h>

#include <optional>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/process/process.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/expected.h"
#include "base/win/access_token.h"
#include "base/win/com_init_util.h"
#include "base/win/registry.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/security_descriptor.h"
#include "base/win/sid.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/elevation_service/elevation_service_idl.h"
#include "chrome/elevation_service/elevator.h"
#include "chrome/install_static/install_util.h"
#include "chrome/installer/util/isolation_support.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"

namespace chrome {

namespace {

constexpr wchar_t kIsolationStateValue[] = L"IsolationState";

base::expected<base::win::RegKey, LONG> GetIsolatedBrowserRegistryKey(
    REGSAM access) {
  base::win::RegKey regkey(HKEY_CURRENT_USER);

  auto result =
      regkey.OpenKey(install_static::GetRegistryPath().c_str(), access);

  // Create the key if it does not exist. This should not happen in production
  // since this key is used for many other values, but can happen in tests.
  if (result == ERROR_FILE_NOT_FOUND && access & KEY_WRITE) {
    result =
        regkey.CreateKey(install_static::GetRegistryPath().c_str(), access);
  }

  if (result != ERROR_SUCCESS) {
    return base::unexpected(result);
  }

  return regkey;
}

}  // namespace

base::expected<base::Process, HRESULT> LaunchIsolatedBrowser(
    const base::CommandLine& command_line) {
  base::win::ScopedCOMInitializer com_init;

  base::win::AssertComInitialized();

  Microsoft::WRL::ComPtr<IElevator2> elevator;
  HRESULT hr = ::CoCreateInstance(
      install_static::GetElevatorClsid(), nullptr, CLSCTX_LOCAL_SERVER,
      install_static::GetElevatorIid(), IID_PPV_ARGS_Helper(&elevator));

  if (FAILED(hr)) {
    return base::unexpected(hr);
  }

  hr = ::CoSetProxyBlanket(
      elevator.Get(), RPC_C_AUTHN_DEFAULT, RPC_C_AUTHZ_DEFAULT,
      COLE_DEFAULT_PRINCIPAL, RPC_C_AUTHN_LEVEL_PKT_PRIVACY,
      RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_DYNAMIC_CLOAKING);
  if (FAILED(hr)) {
    return base::unexpected(hr);
  }

  base::win::ScopedHandle job;

  job.Set(::CreateJobObjectW(nullptr, nullptr));
  if (!job.is_valid()) {
    return base::unexpected(HRESULT_FROM_WIN32(::GetLastError()));
  }

  JOBOBJECT_EXTENDED_LIMIT_INFORMATION limit_information = {};
  limit_information.BasicLimitInformation.LimitFlags =
      JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;

  if (!::SetInformationJobObject(job.get(), JobObjectExtendedLimitInformation,
                                 &limit_information,
                                 sizeof(limit_information))) {
    return base::unexpected(HRESULT_FROM_WIN32(::GetLastError()));
  }

  if (!::AssignProcessToJobObject(job.get(), ::GetCurrentProcess())) {
    return base::unexpected(HRESULT_FROM_WIN32(::GetLastError()));
  }

  // Leak the job handle. This is because closing the Job before the owning
  // process terminates will terminate all processes in the tree including this
  // one. The Job object will be closed when this process terminates,
  // immediately terminating all other processes.
  std::ignore = job.release();

  DWORD last_error = 0;
  ULONG_PTR proc_handle;
  base::win::ScopedBstr log;
  hr = elevator->RunIsolatedChrome(
      /*flags=*/0, command_line.GetCommandLineString().c_str(),
      /*log=*/log.Receive(), &proc_handle, &last_error);
  if (FAILED(hr)) {
    return base::unexpected(hr);
  }

  return base::Process(reinterpret_cast<base::ProcessHandle>(proc_handle));
}

bool IsIsolationEnabled(const base::CommandLine* command_line) {
  if (!install_static::IsSystemInstall()) {
    return false;
  }

  if (command_line) {
    // Set of switches that will never result in an attempt to launch an
    // isolated browser.
    const char* const kNoIsolationSwitches[] = {
        // Custom user data dir always runs uninsolated as it's not possible to
        // determine the isolation state of any cryptographic data.
        ::switches::kUserDataDir,
        // If this browser is running isolated, never attempt to launch isolated
        // again.
        ::switches::kIsolated,
    };

    for (const auto* no_isolation_switch : kNoIsolationSwitches) {
      if (command_line->HasSwitch(no_isolation_switch)) {
        return false;
      }
    }
  }

  auto regkey = GetIsolatedBrowserRegistryKey(KEY_READ);
  if (!regkey.has_value()) {
    return false;
  }

  DWORD out_value = 0;
  if (regkey->ReadValueDW(kIsolationStateValue, &out_value) != ERROR_SUCCESS) {
    return false;
  }

  if (out_value > static_cast<DWORD>(IsolationState::kMaxValue)) {
    return false;
  }

  IsolationState state = static_cast<IsolationState>(out_value);

  switch (state) {
    case IsolationState::kIsolationDisabled:
      return false;
    case IsolationState::kProcessIsolation:
      return true;
  }
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

void SetIsolationState(
    IsolationState state,
    base::OnceCallback<void(base::expected<IsolationState, HRESULT>)>
        completed) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::optional<base::expected<IsolationState, HRESULT>> return_value;

  absl::Cleanup fire_callback = [&return_value,
                                 callback = std::move(completed)]() mutable {
    // Make sure the return value has been set.
    CHECK(return_value.has_value());
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), *std::move(return_value)));
  };

  if (!install_static::IsSystemInstall()) {
    return_value.emplace(base::unexpected(E_NOTIMPL));
    return;
  }

  auto regkey = GetIsolatedBrowserRegistryKey(KEY_READ | KEY_WRITE);
  if (!regkey.has_value()) {
    return_value.emplace(base::unexpected(HRESULT_FROM_WIN32(regkey.error())));
    return;
  }

  LONG result = ERROR_SUCCESS;
  if (state == IsolationState::kIsolationDisabled) {
    result = regkey->DeleteValue(kIsolationStateValue);
  } else {
    result =
        regkey->WriteValue(kIsolationStateValue, static_cast<DWORD>(state));
  }

  if (result != ERROR_SUCCESS) {
    return_value.emplace(base::unexpected(HRESULT_FROM_WIN32(result)));
    return;
  }

  // TODO(crbug.com/433545123): Migrate any encrypted/secured data to/from
  // isolated environment here.
  return_value.emplace(state);
}

}  // namespace chrome
