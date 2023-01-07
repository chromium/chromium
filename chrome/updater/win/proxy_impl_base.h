// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_PROXY_IMPL_BASE_H_
#define CHROME_UPDATER_WIN_PROXY_IMPL_BASE_H_

#include <wrl/client.h>

#include <ios>
#include <utility>

#include "base/callback.h"
#include "base/check.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/single_thread_task_runner_thread_mode.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/win/win_constants.h"
#include "chrome/updater/win/win_util.h"
#include "chrome/updater/win/wrl_module_initializer.h"

namespace updater {

template <typename Derived, typename Interface>
class ProxyImplBase {
 public:
  // Releases `impl` on `task_runner_`.
  static void Destroy(scoped_refptr<Derived>& impl) {
    scoped_refptr<Derived> this_impl;
    this_impl.swap(impl);
    scoped_refptr<base::SingleThreadTaskRunner> task_runner =
        this_impl->task_runner_;
    task_runner->PostTask(FROM_HERE, base::BindOnce(
                                         [](scoped_refptr<Derived> impl) {
                                           CHECK(impl);
                                           impl = nullptr;
                                         },
                                         std::move(this_impl)));
    CHECK(!this_impl);
  }

 protected:
  explicit ProxyImplBase(UpdaterScope scope) : scope_(scope) {
    DETACH_FROM_SEQUENCE(sequence_checker_);
    WRLModuleInitializer::Get();
  }

  ~ProxyImplBase() { VLOG(2) << __func__; }

  void PostRPCTask(base::OnceClosure task) {
    task_runner_->PostTask(FROM_HERE, std::move(task));
  }

  HResultOr<Microsoft::WRL::ComPtr<Interface>> CreateInterface() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    ::Sleep(kCreateUpdaterInstanceDelayMs);

    Microsoft::WRL::ComPtr<IUnknown> server;
    HRESULT hr = ::CoCreateInstance(Derived::GetClassGuid(scope_), nullptr,
                                    CLSCTX_LOCAL_SERVER, IID_PPV_ARGS(&server));
    if (FAILED(hr)) {
      VLOG(2) << "Failed to instantiate the update server: " << std::hex << hr;
      return base::unexpected(hr);
    }

    Microsoft::WRL::ComPtr<Interface> server_interface;
    hr = server.As(&server_interface);

    // TODO(crbug.com/1341471) - revert the CL that introduced the check after
    // the bug is resolved.
    CHECK(SUCCEEDED(hr)) << "Failed to query the interface: " << std::hex << hr;
    return server_interface;
  }

  HRESULT hresult() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK(!interface_.has_value());
    return interface_.error();
  }

  Microsoft::WRL::ComPtr<Interface> get_interface() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK(interface_.has_value());
    return interface_.value();
  }

  bool ConnectToServer() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (interface_.has_value()) {
      return true;
    }
    interface_ = CreateInterface();
    return interface_.has_value();
  }

  // Bound to the `task_runner_` sequence.
  SEQUENCE_CHECKER(sequence_checker_);

 private:
  // Runs the tasks which invoke outbound COM calls and receive inbound COM
  // callbacks. This task runner is thread-affine with the platform COM STA.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_ =
      base::ThreadPool::CreateCOMSTATaskRunner(
          {base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
          base::SingleThreadTaskRunnerThreadMode::DEDICATED);

  const UpdaterScope scope_;

  // Interface owned by the STA. It must be created and released by the STA.
  HResultOr<Microsoft::WRL::ComPtr<Interface>> interface_ =
      base::unexpected(S_OK);
};

}  // namespace updater

#endif  // CHROME_UPDATER_WIN_PROXY_IMPL_BASE_H_
