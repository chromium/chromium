// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <wrl/module.h>

#include <utility>

#include "base/check.h"
#include "base/containers/heap_array.h"
#include "base/logging.h"
#include "base/process/process.h"
#include "base/threading/platform_thread.h"
#include "base/types/expected.h"
#include "base/win/win_util.h"
#include "chrome/common/win/eventlog_messages.h"
#include "chrome/windows_services/service_program/factory_and_clsid.h"
#include "chrome/windows_services/service_program/get_calling_process.h"
#include "chrome/windows_services/service_program/scoped_client_impersonation.h"
#include "chrome/windows_services/service_program/service_delegate.h"
#include "chrome/windows_services/service_program/service_program_main.h"
#include "chrome/windows_services/service_program/test_service_idl.h"

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
