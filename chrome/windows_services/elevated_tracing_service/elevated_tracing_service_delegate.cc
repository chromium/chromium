// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/windows_services/elevated_tracing_service/elevated_tracing_service_delegate.h"

#include <wrl/client.h>
#include <wrl/implements.h>
#include <wrl/module.h>

#include <utility>

#include "base/command_line.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "chrome/common/win/eventlog_messages.h"
#include "chrome/install_static/install_util.h"
#include "chrome/windows_services/elevated_tracing_service/session_registry.h"
#include "chrome/windows_services/elevated_tracing_service/system_tracing_session.h"
#include "services/tracing/public/cpp/trace_startup.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"

namespace elevated_tracing_service {

namespace {

// A class factory for SystemTracingSession.
class SystemTracingSessionClassFactory : public Microsoft::WRL::ClassFactory<> {
 public:
  SystemTracingSessionClassFactory() = default;
  SystemTracingSessionClassFactory(const SystemTracingSessionClassFactory&) =
      delete;
  SystemTracingSessionClassFactory& operator=(
      const SystemTracingSessionClassFactory&) = delete;

  // Sets the task runner to be used for general main-thread processing.
  void set_task_runner(scoped_refptr<base::SequencedTaskRunner> task_runner) {
    task_runner_ = std::move(task_runner);
  }

  // IClassFactory:
  IFACEMETHODIMP CreateInstance(IUnknown* pUnkOuter,
                                REFIID riid,
                                void** ppvObject) override {
    // Bump the object count for the duration of this call and reduce it upon
    // exit to ensure that the service terminates if the factory fails to
    // produce an instance and there is no pre-existing instance.
    auto& module =
        Microsoft::WRL::Module<Microsoft::WRL::OutOfProc>::GetModule();
    module.IncrementObjectCount();
    absl::Cleanup count_decrementer = [&module] {
      module.DecrementObjectCount();
    };

    *ppvObject = nullptr;

    if (pUnkOuter != nullptr) {
      return CLASS_E_NOAGGREGATION;
    }

    Microsoft::WRL::ComPtr<IUnknown> unknown;
    HRESULT hr = Microsoft::WRL::MakeAndInitialize<SystemTracingSession>(
        &unknown, task_runner_);
    return SUCCEEDED(hr) ? unknown.CopyTo(riid, ppvObject) : hr;
  }

 private:
  // The task runner to be used for general main-thread processing.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

}  // namespace

Delegate::Delegate() = default;

Delegate::~Delegate() = default;

uint16_t Delegate::GetLogEventCategory() {
  return TRACING_SERVICE_CATEGORY;
}

uint32_t Delegate::GetLogEventMessageId() {
  return MSG_TRACING_SERVICE_LOG_MESSAGE;
}

base::expected<base::HeapArray<FactoryAndClsid>, HRESULT>
Delegate::CreateClassFactories() {
  unsigned int flags = Microsoft::WRL::ModuleType::OutOfProc;

  auto result = base::HeapArray<FactoryAndClsid>::WithSize(1);
  Microsoft::WRL::ComPtr<IUnknown> unknown;
  HRESULT hr = Microsoft::WRL::Details::CreateClassFactory<
      SystemTracingSessionClassFactory>(&flags, nullptr,
                                        __uuidof(IClassFactory), &unknown);
  if (SUCCEEDED(hr)) {
    hr = unknown.As(&result[0].factory);
    // CreateClassFactory doesn't support passing arguments while constructing
    // the factory, so pass the main thread's task runner to the factory ex post
    // facto.
    static_cast<SystemTracingSessionClassFactory*>(result[0].factory.Get())
        ->set_task_runner(base::SequencedTaskRunner::GetCurrentDefault());
  }
  if (FAILED(hr)) {
    return base::unexpected(hr);
  }

  result[0].clsid = base::CommandLine::ForCurrentProcess()->HasSwitch(
                        switches::kSystemTracingClsIdForTestingSwitch)
                        ? kTestSystemTracingSessionClsid
                        : install_static::GetTracingServiceClsid();
  return base::ok(std::move(result));
}

void Delegate::PreRun() {
  // Run a ThreadPool.
  base::ThreadPoolInstance::CreateAndStartWithDefaultParams(
      "elevated_tracing_service");

  // Initialize tracing in the process.
  tracing::InitTracingPostThreadPoolStartAndFeatureList(
      /*enable_consumer=*/false);

  // Create the global SessionRegistry.
  session_registry_ = base::MakeRefCounted<SessionRegistry>();
}

void Delegate::PostRun() {
  base::ThreadPoolInstance::Get()->Shutdown();

  // Destroy the global SessionRegistry.
  session_registry_.reset();
}

}  // namespace elevated_tracing_service
