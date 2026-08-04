// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/isolated_browser_support.h"

#include <objbase.h>

#include <windows.h>

#include <winerror.h>

#include <optional>
#include <utility>

#include "base/check.h"
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
#include "chrome/browser/os_crypt/app_bound_encryption_provider_win.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/elevation_service/elevation_service_idl.h"
#include "chrome/elevation_service/elevator.h"
#include "chrome/install_static/install_util.h"
#include "chrome/installer/util/isolation_support.h"
#include "components/os_crypt/async/browser/key_provider.h"
#include "content/public/browser/browser_thread.h"

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

void CompleteRegistryPersistence(
    IsolationState state,
    base::OnceCallback<void(base::expected<IsolationState, HRESULT>)>
        completed) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto regkey = GetIsolatedBrowserRegistryKey(KEY_READ | KEY_WRITE);
  if (!regkey.has_value()) {
    std::move(completed).Run(
        base::unexpected(HRESULT_FROM_WIN32(regkey.error())));
    return;
  }

  LONG result = ERROR_SUCCESS;
  if (state == IsolationState::kIsolationDisabled) {
    result = regkey->DeleteValue(kIsolationStateValue);
    if (result == ERROR_FILE_NOT_FOUND) {
      // Value never existed in the first place.
      result = ERROR_SUCCESS;
    }
  } else {
    result =
        regkey->WriteValue(kIsolationStateValue, static_cast<DWORD>(state));
  }

  if (result != ERROR_SUCCESS) {
    std::move(completed).Run(base::unexpected(HRESULT_FROM_WIN32(result)));
    return;
  }

  std::move(completed).Run(state);
}

}  // namespace

IsolatedBrowserProcess::IsolatedBrowserProcess(base::Process process,
                                               base::win::ScopedHandle job,
                                               base::win::ScopedHandle iocp)
    : process_(std::move(process)),
      job_(std::move(job)),
      iocp_(std::move(iocp)) {
  CHECK(process_.IsValid());
  CHECK(job_.is_valid());
  CHECK(iocp_.is_valid());
}

IsolatedBrowserProcess::~IsolatedBrowserProcess() = default;

IsolatedBrowserProcess::IsolatedBrowserProcess(IsolatedBrowserProcess&&) =
    default;
IsolatedBrowserProcess& IsolatedBrowserProcess::operator=(
    IsolatedBrowserProcess&&) = default;

std::optional<int> IsolatedBrowserProcess::WaitForExit() const {
  int exit_code = 0;
  // Stage 1: Wait for the primary isolated browser process handle to signal.
  // During normal browsing operations (which may last for hours or days), the
  // stub thread remains blocked in this kernel wait state with zero CPU
  // overhead.
  if (!process_.WaitForExit(&exit_code)) {
    return std::nullopt;
  }

  // Stage 2: The main isolated browser process handle has signaled ExitProcess.
  // Now drain process exit completion messages until all isolated processes
  // inside the job object have completed kernel process termination.
  //
  // Note: The stub process itself is assigned to the Job Object (to guarantee
  // that if the stub process is killed or crashes,
  // JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE terminates all isolated child
  // processes). Therefore, the stub process counts as 1 active process inside
  // the job.
  //
  // Checking `accounting_info.ActiveProcesses == 1` guarantees that ONLY the
  // stub process remains inside the Job Object.
  constexpr DWORD kRecheckIntervalMs = 1000;
  for (;;) {
    JOBOBJECT_BASIC_ACCOUNTING_INFORMATION accounting_info = {};
    if (!::QueryInformationJobObject(
            job_.get(), JobObjectBasicAccountingInformation, &accounting_info,
            sizeof(accounting_info), nullptr)) {
      DPLOG(ERROR) << "QueryInformationJobObject failed";
      return std::nullopt;
    }

    if (accounting_info.ActiveProcesses == 1) {
      return exit_code;
    }

    if (accounting_info.ActiveProcesses == 0) {
      DLOG(ERROR) << "Job reported zero active processes while stub is alive";
      return std::nullopt;
    }

    DWORD completion_code = 0;
    ULONG_PTR completion_key = 0;
    LPOVERLAPPED message_value = nullptr;

    // Note: message delivery is not guaranteed, see
    // https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-jobobject_associate_completion_port
    // so wait for maximum of 1 second between each message and re-check the
    // active process count, this ensures that even if the message is dropped,
    // the stub always terminates in a timely manner.
    if (!::GetQueuedCompletionStatus(iocp_.get(), &completion_code,
                                     &completion_key, &message_value,
                                     kRecheckIntervalMs)) {
      const DWORD error = ::GetLastError();
      if (error == WAIT_TIMEOUT) {
        // Timeout expired; re-query authoritative accounting state.
        continue;
      }
      DPLOG(ERROR) << "GetQueuedCompletionStatus failed";
      return std::nullopt;
    }
  }
}

// static
base::expected<IsolatedBrowserProcess, HRESULT> IsolatedBrowserProcess::Launch(
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

  // Create an I/O Completion Port and associate it with the Job Object.
  // This allows the stub process to monitor process exit events
  // (`JOB_OBJECT_MSG_EXIT_PROCESS`) and wait until all isolated browser
  // processes in the job tree have fully terminated.
  base::win::ScopedHandle iocp;
  iocp.Set(::CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 1));
  if (!iocp.is_valid()) {
    return base::unexpected(HRESULT_FROM_WIN32(::GetLastError()));
  }

  JOBOBJECT_ASSOCIATE_COMPLETION_PORT port_assoc = {
      .CompletionKey = job.get(), .CompletionPort = iocp.get()};
  if (!::SetInformationJobObject(job.get(),
                                 JobObjectAssociateCompletionPortInformation,
                                 &port_assoc, sizeof(port_assoc))) {
    return base::unexpected(HRESULT_FROM_WIN32(::GetLastError()));
  }

  // Assign the stub process to the job object BEFORE launching the isolated
  // browser. The elevator creates Chrome with the stub as
  // PROC_THREAD_ATTRIBUTE_PARENT_PROCESS, so Chrome inherits this job. Chrome
  // descendants then inherit the job by default. Note: Stub's Crashpad was
  // initialized prior to this call during early startup, so Crashpad processes
  // remain outside this Job Object.
  if (!::AssignProcessToJobObject(job.get(), ::GetCurrentProcess())) {
    return base::unexpected(HRESULT_FROM_WIN32(::GetLastError()));
  }

  DWORD last_error = 0;
  ULONG_PTR proc_handle;
  base::win::ScopedBstr log;
  hr = elevator->RunIsolatedChrome(
      /*flags=*/0, command_line.GetCommandLineString().c_str(),
      /*log=*/log.Receive(), &proc_handle, &last_error);
  if (FAILED(hr)) {
    return base::unexpected(hr);
  }

  base::Process process(reinterpret_cast<base::ProcessHandle>(proc_handle));

  // Duplicate a query handle for IsolatedBrowserProcess before leaking the
  // primary handle.
  HANDLE query_job_handle = nullptr;
  if (!::DuplicateHandle(::GetCurrentProcess(), job.get(),
                         ::GetCurrentProcess(), &query_job_handle,
                         JOB_OBJECT_QUERY, FALSE, 0)) {
    return base::unexpected(HRESULT_FROM_WIN32(::GetLastError()));
  }
  base::win::ScopedHandle query_job(query_job_handle);

  // Intentionally leak the primary job handle so that ScopedHandle destruction
  // when IsolatedBrowserProcess goes out of scope does not close the final job
  // handle while the stub/test process is executing. When the stub/test process
  // terminates or crashes, Windows kernel automatically closes all process
  // handles, triggering JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE to kill any
  // surviving browser processes.
  std::ignore = job.release();

  return IsolatedBrowserProcess(std::move(process), std::move(query_job),
                                std::move(iocp));
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

std::optional<base::win::AccessToken> GetUnisolatedAccessToken() {
  std::optional<base::win::AccessToken> token;

  if (!IsRunningIsolated()) {
    token = base::win::AccessToken::FromCurrentProcess(
        /*impersonation=*/false, TOKEN_DUPLICATE | TOKEN_QUERY);
  } else {
    base::ProcessId parent_pid =
        base::GetParentProcessId(base::GetCurrentProcessHandle());
    if (parent_pid == base::kNullProcessId) {
      return std::nullopt;
    }

    // Note: there is no race here between obtaining the parent pid and the
    // opening the handle. This is because the isolated stub process (parent)
    // places the child process (browser) in a job object that ensures it always
    // outlives the browser, so if this code is executing, it means the parent
    // is still alive.
    auto parent_process =
        base::Process::OpenWithAccess(parent_pid, PROCESS_QUERY_INFORMATION);

    if (!parent_process.IsValid()) {
      return std::nullopt;
    }

    token = base::win::AccessToken::FromProcess(parent_process.Handle(),
                                                /*impersonation=*/false,
                                                TOKEN_DUPLICATE | TOKEN_QUERY);

    // The parent process should never be running isolated, if it is, this API
    // has been called from a child process, which should not be possible.
    CHECK(!token->GetSecurityAttribute(installer::GetIsolationAttributeName())
               .has_value());
  }

  if (!token) {
    return std::nullopt;
  }

  return token->DuplicatePrimary(MAXIMUM_ALLOWED);
}

void SetIsolationState(
    IsolationState state,
    PrefService* local_state,
    base::OnceCallback<void(base::expected<IsolationState, HRESULT>)>
        completed) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!install_static::IsSystemInstall()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(completed), base::unexpected(E_NOTIMPL)));
    return;
  }

  // Switching from isolated to non-isolated requires re-encrypting the
  // app-bound key into a non-isolated state.
  if (IsIsolationEnabled() && state == IsolationState::kIsolationDisabled) {
    // Force downgrade to PROTECTION_PATH_VALIDATION, which is not bound to the
    // isolation state of the browser.
    auto provider = std::make_unique<
        os_crypt_async::AppBoundEncryptionProviderWin>(
        local_state,
        /*force_protection_level=*/ProtectionLevel::PROTECTION_PATH_VALIDATION);

    // Obtaining the key triggers re-encryption of the stored encrypted key. If
    // there is no previously encrypted key, then there's nothing to do - it
    // will be stored encrypted correctly after restart.
    if (provider->IsKeyStored()) {
      provider->GetKey(base::BindOnce(
          [](std::unique_ptr<os_crypt_async::AppBoundEncryptionProviderWin>
                 provider,
             base::OnceCallback<void(base::expected<IsolationState, HRESULT>)>
                 completed,
             IsolationState state, const std::string& tag,
             base::expected<os_crypt_async::Encryptor::Key,
                            os_crypt_async::KeyProvider::KeyError> result) {
            if (!result.has_value()) {
              base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                  FROM_HERE, base::BindOnce(std::move(completed),
                                            base::unexpected(E_FAIL)));
              return;
            }
            CompleteRegistryPersistence(state, std::move(completed));
          },
          std::move(provider), std::move(completed), state));
      return;
    }
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&CompleteRegistryPersistence, state,
                                std::move(completed)));
}

}  // namespace chrome
