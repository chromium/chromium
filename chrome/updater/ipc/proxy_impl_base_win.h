// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_IPC_PROXY_IMPL_BASE_WIN_H_
#define CHROME_UPDATER_IPC_PROXY_IMPL_BASE_WIN_H_

#include <wrl/client.h>

#include <ios>
#include <utility>

#include "base/check.h"
#include "base/debug/alias.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/strings/string_util.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/platform_thread.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/win/win_util.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/win_util.h"
#include "chrome/updater/win/win_constants.h"

namespace updater {

// `iid_user` and `iid_system` are the interface ids corresponding to
// `Interface` for user and system. These interface ids must be different for
// COM automation marshaling to work correctly.
template <typename Derived,
          typename Interface,
          REFIID iid_user,
          REFIID iid_system>
class ProxyImplBase {
 public:
  // Releases `impl` on `task_runner_`.
  static void Destroy(scoped_refptr<Derived> impl) {
    scoped_refptr<base::SequencedTaskRunner> task_runner = impl->task_runner_;
    task_runner->PostTask(FROM_HERE,
                          base::BindOnce([](scoped_refptr<Derived> /*impl*/) {},
                                         std::move(impl)));
  }

 protected:
  explicit ProxyImplBase(UpdaterScope scope) : scope_(scope) {
    DETACH_FROM_SEQUENCE(sequence_checker_);
    VLOG(3) << __func__ << ": Interface: " << typeid(Interface).name()
            << ": iid_user: " << StringFromGuid(iid_user)
            << ": iid_system: " << StringFromGuid(iid_system)
            << ": scope: " << scope;
  }

  ~ProxyImplBase() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    VLOG(2) << __func__;
  }

  void PostRPCTask(base::OnceClosure task) {
    task_runner_->PostTask(FROM_HERE, std::move(task));
  }

  HResultOr<Microsoft::WRL::ComPtr<Interface>> CreateInterface() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    // Retry creating the object if the call fails. Don't retry if
    // the error is `REGDB_E_CLASSNOTREG` because the error can occur during
    // normal operation and retrying on registration issues does not help.
    const auto create_server =
        [](REFCLSID clsid) -> HResultOr<Microsoft::WRL::ComPtr<IUnknown>> {
      constexpr int kNumTries = 2;
      HRESULT hr = E_FAIL;
      for (int i = 0; i != kNumTries; ++i) {
        Microsoft::WRL::ComPtr<IUnknown> server;
        hr = ::CoCreateInstance(clsid, nullptr, CLSCTX_LOCAL_SERVER,
                                IID_PPV_ARGS(&server));
        if (SUCCEEDED(hr)) {
          return server;
        }
        VLOG(2) << "::CoCreateInstance failed: " << StringFromGuid(clsid)
                << ": " << std::hex << hr;
        if (hr == REGDB_E_CLASSNOTREG) {
          return base::unexpected(hr);
        }

        // Sleep before trying again.
        base::PlatformThread::Sleep(kCreateUpdaterInstanceDelay);
      }
      return base::unexpected(hr);
    };
    ASSIGN_OR_RETURN(Microsoft::WRL::ComPtr<IUnknown> server,
                     create_server(Derived::GetClassGuid(scope_)));

    Microsoft::WRL::ComPtr<Interface> server_interface;
    REFIID iid = IsSystemInstall(scope_) ? iid_system : iid_user;
    const HRESULT hr =
        server.CopyTo(iid, IID_PPV_ARGS_Helper(&server_interface));
    if (SUCCEEDED(hr)) {
      return server_interface;
    }
    VLOG(2) << "Failed to query the interface: " << StringFromGuid(iid) << ": "
            << std::hex << hr;
    return base::unexpected(hr);
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

  HRESULT ConnectToServer() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (interface_.has_value()) {
      return S_OK;
    }
    interface_ = CreateInterface();
    return interface_.has_value() ? S_OK : interface_.error();
  }

  // Bound to the `task_runner_` sequence.
  SEQUENCE_CHECKER(sequence_checker_);

 private:
  // Sequences the outbound calls so that the main sequence is not blocked on an
  // RPC call.
  scoped_refptr<base::SequencedTaskRunner> task_runner_ =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::USER_BLOCKING,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN,
           base::WithBaseSyncPrimitives(), base::MayBlock()});

  const UpdaterScope scope_;

  HResultOr<Microsoft::WRL::ComPtr<Interface>> interface_ =
      base::unexpected(S_OK);
};

}  // namespace updater

#endif  // CHROME_UPDATER_IPC_PROXY_IMPL_BASE_WIN_H_
