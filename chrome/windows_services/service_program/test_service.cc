// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <wrl/module.h>

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/containers/heap_array.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/immediate_crash.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/process/process.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "base/types/expected.h"
#include "base/win/scoped_bstr.h"
#include "base/win/windows_handle_util.h"
#include "chrome/common/chrome_version.h"
#include "chrome/common/win/eventlog_messages.h"
#include "chrome/windows_services/service_program/crash_reporting.h"
#include "chrome/windows_services/service_program/factory_and_clsid.h"
#include "chrome/windows_services/service_program/get_calling_process.h"
#include "chrome/windows_services/service_program/is_running_unattended.h"
#include "chrome/windows_services/service_program/scoped_client_impersonation.h"
#include "chrome/windows_services/service_program/service_delegate.h"
#include "chrome/windows_services/service_program/service_program_main.h"
#include "chrome/windows_services/service_program/test_service_idl.h"
#include "chrome/windows_services/service_program/user_crash_state.h"
#include "components/crash/core/app/crash_export_thunks.h"

namespace {

class TestServiceImpl
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          ITestService> {
 public:
  TestServiceImpl() = default;
  TestServiceImpl(const TestServiceImpl&) = delete;
  TestServiceImpl& operator=(const TestServiceImpl&) = delete;

  // ITestService:
  IFACEMETHOD(GetProcessHandle)(unsigned long* handle) override {
    LOG(INFO) << __func__;
    base::Process client_process;
    // Get a handle to the calling process.
    {
      ScopedClientImpersonation impersonate;
      CHECK(impersonate.is_valid());
      client_process = GetCallingProcess();
      CHECK(client_process.IsValid());
    }
    // Duplicate it with permission to dup handles into it.
    HANDLE duplicate = nullptr;
    PCHECK(::DuplicateHandle(::GetCurrentProcess(), client_process.Release(),
                             ::GetCurrentProcess(), &duplicate,
                             PROCESS_DUP_HANDLE,
                             /*bInheritHandle=*/FALSE,
                             /*dwOptions=*/DUPLICATE_CLOSE_SOURCE));
    client_process = base::Process(std::exchange(duplicate, nullptr));

    // Allow the client to get the service's PID and wait for it to exit.
    PCHECK(::DuplicateHandle(::GetCurrentProcess(), ::GetCurrentProcess(),
                             client_process.Handle(), &duplicate,
                             PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE,
                             /*bInheritHandle=*/FALSE,
                             /*dwOptions=*/0));
    *handle = base::win::HandleToUint32(duplicate);
    return S_OK;
  }

  IFACEMETHOD(IsRunningUnattended)
  (VARIANT_BOOL* is_running_unattended) override {
    *is_running_unattended =
        internal::IsRunningUnattended() ? VARIANT_TRUE : VARIANT_FALSE;
    return S_OK;
  }

  IFACEMETHOD(GetCrashpadDatabasePath)(BSTR* database_path) override {
    StartCrashHandler();
    const wchar_t* path_str = GetCrashpadDatabasePath_ExportThunk();
    if (!path_str) {
      return E_FAIL;
    }
    *database_path = base::win::ScopedBstr(path_str).Release();
    return S_OK;
  }

  IFACEMETHOD(InduceCrash)() override {
    StartCrashHandler();
    base::ImmediateCrash();
  }

  IFACEMETHOD(InduceCrashSoon)() override {
    StartCrashHandler();
    CrashThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce([]() { base::ImmediateCrash(); }));
    return S_OK;
  }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> CrashThreadTaskRunner() {
    if (!crash_thread_.task_runner()) {
      base::Thread::Options crash_thread_options;
      crash_thread_options.joinable = false;
      CHECK(crash_thread_.StartWithOptions(std::move(crash_thread_options)));
    }
    return crash_thread_.task_runner();
  }

  void StartCrashHandler() {
    if (crash_handler_started_) {
      return;
    }

    // Get crash state for the client.
    std::unique_ptr<UserCrashState> user_crash_state;
    {
      ScopedClientImpersonation impersonate;
      CHECK(impersonate.is_valid());
      base::Process client_process = GetCallingProcess();
      CHECK(client_process.IsValid());
      user_crash_state = UserCrashState::Create(impersonate, client_process);
    }

    // Use the elevated tracing service's process type, to avoid tripping on the
    // allowlist in InitializeCrashpadImpl.
    windows_services::StartCrashHandler(
        std::move(user_crash_state),
        /*directory_name=*/
        FILE_PATH_LITERAL(PRODUCT_SHORTNAME_STRING)
            FILE_PATH_LITERAL("TestService"),
        /*process_type=*/"elevated-tracing-service", CrashThreadTaskRunner());

    crash_handler_started_ = true;
  }

  base::Thread crash_thread_{"CrashThread"};
  bool crash_handler_started_ = false;
};

class TestServiceDelegate : public ServiceDelegate {
 public:
  TestServiceDelegate() = default;
  TestServiceDelegate(const TestServiceDelegate&) = default;
  TestServiceDelegate& operator=(const TestServiceDelegate&) = default;

  // ServiceDelegate:
  uint16_t GetLogEventCategory() override { return BROWSER_CATEGORY; }
  uint32_t GetLogEventMessageId() override { return MSG_LOG_MESSAGE; }
  base::expected<base::HeapArray<FactoryAndClsid>, HRESULT>
  CreateClassFactories() override {
    unsigned int flags = Microsoft::WRL::ModuleType::OutOfProc;

    auto result = base::HeapArray<FactoryAndClsid>::WithSize(1);
    Microsoft::WRL::ComPtr<IUnknown> unknown;
    HRESULT hr = Microsoft::WRL::Details::CreateClassFactory<
        Microsoft::WRL::SimpleClassFactory<TestServiceImpl>>(
        &flags, nullptr, __uuidof(IClassFactory), &unknown);
    if (SUCCEEDED(hr)) {
      hr = unknown.As(&result[0].factory);
    }
    if (FAILED(hr)) {
      return base::unexpected(hr);
    }

    result[0].clsid = __uuidof(TestService);
    return base::ok(std::move(result));
  }
};

}  // namespace

extern "C" int WINAPI wWinMain(HINSTANCE /*instance*/,
                               HINSTANCE /*prev_instance*/,
                               wchar_t* /*command_line*/,
                               int /*show_command*/) {
  TestServiceDelegate delegate;
  return ServiceProgramMain(delegate);
}
