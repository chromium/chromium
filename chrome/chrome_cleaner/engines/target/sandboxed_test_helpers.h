// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_ENGINES_TARGET_SANDBOXED_TEST_HELPERS_H_
#define CHROME_CHROME_CLEANER_ENGINES_TARGET_SANDBOXED_TEST_HELPERS_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/run_loop.h"
#include "chrome/chrome_cleaner/engines/broker/cleaner_engine_requests_impl.h"
#include "chrome/chrome_cleaner/engines/broker/engine_cleanup_results_impl.h"
#include "chrome/chrome_cleaner/engines/broker/engine_file_requests_impl.h"
#include "chrome/chrome_cleaner/engines/broker/engine_requests_impl.h"
#include "chrome/chrome_cleaner/engines/broker/engine_scan_results_impl.h"
#include "chrome/chrome_cleaner/engines/broker/interface_metadata_observer.h"
#include "chrome/chrome_cleaner/engines/common/engine_digest_verifier.h"
#include "chrome/chrome_cleaner/engines/target/cleaner_engine_requests_proxy.h"
#include "chrome/chrome_cleaner/engines/target/engine_commands_impl.h"
#include "chrome/chrome_cleaner/engines/target/engine_file_requests_proxy.h"
#include "chrome/chrome_cleaner/engines/target/engine_requests_proxy.h"
#include "chrome/chrome_cleaner/ipc/ipc_test_util.h"
#include "chrome/chrome_cleaner/mojom/engine_sandbox.mojom.h"
#include "chrome/chrome_cleaner/os/file_remover.h"
#include "chrome/chrome_cleaner/os/layered_service_provider_wrapper.h"
#include "chrome/chrome_cleaner/pup_data/pup_data.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chrome_cleaner {

// |BaseClass| should be ParentProcess or SandboxedParentProcess.
template <typename BaseClass>
class MaybeSandboxedParentProcess : public BaseClass {
 public:
  enum class CallbacksToSetup {
    kFileRequests,
    kScanAndCleanupRequests,
    kCleanupRequests
  };

  MaybeSandboxedParentProcess(
      scoped_refptr<MojoTaskRunner> mojo_task_runner,
      CallbacksToSetup requests_to_setup,
      InterfaceMetadataObserver* metadata_observer = nullptr)
      : BaseClass(std::move(mojo_task_runner)),
        requests_to_setup_(requests_to_setup),
        engine_commands_(
            std::make_unique<mojo::Remote<mojom::EngineCommands>>()),
        file_requests_impl_(
            std::make_unique<EngineFileRequestsImpl>(this->mojo_task_runner(),
                                                     metadata_observer)) {
    if (requests_to_setup_ == CallbacksToSetup::kScanAndCleanupRequests ||
        requests_to_setup_ == CallbacksToSetup::kCleanupRequests) {
      scanner_impl_ = std::make_unique<EngineRequestsImpl>(
          this->mojo_task_runner(), metadata_observer);
      scan_results_impl_ =
          std::make_unique<EngineScanResultsImpl>(metadata_observer);
    }
    if (requests_to_setup_ == CallbacksToSetup::kCleanupRequests) {
      scoped_refptr<chrome_cleaner::DigestVerifier> verifier =
          GetDigestVerifier();
      std::unique_ptr<chrome_cleaner::FileRemoverAPI> file_remover =
          std::make_unique<chrome_cleaner::FileRemover>(
              verifier, /*archiver=*/nullptr,
              chrome_cleaner::LayeredServiceProviderWrapper(),
              base::DoNothing::Repeatedly());
      cleaner_impl_ = std::make_unique<CleanerEngineRequestsImpl>(
          this->mojo_task_runner(), metadata_observer, std::move(file_remover));
      cleanup_results_impl_ =
          std::make_unique<EngineCleanupResultsImpl>(metadata_observer);
    }
  }

  ~MaybeSandboxedParentProcess() override = default;

 protected:
  void CreateImpl(mojo::ScopedMessagePipeHandle mojo_pipe) override {
    engine_commands_->Bind(
        mojo::PendingRemote<mojom::EngineCommands>(std::move(mojo_pipe), 0));

    mojo::PendingAssociatedRemote<mojom::EngineFileRequests> file_requests;
    file_requests_impl_->Bind(&file_requests);

    // Bind to empty callbacks as we don't care about the result.
    mojo::PendingAssociatedRemote<mojom::EngineRequests> scanner;
    mojo::PendingAssociatedRemote<mojom::EngineScanResults> scanner_results;
    if (requests_to_setup_ == CallbacksToSetup::kScanAndCleanupRequests ||
        requests_to_setup_ == CallbacksToSetup::kCleanupRequests) {
      scanner_impl_->Bind(&scanner);
      scan_results_impl_->BindToCallbacks(
          &scanner_results,
          base::BindRepeating(
              base::DoNothing::Repeatedly<UwSId, const PUPData::PUP&>()),
          base::BindOnce(base::DoNothing::Once<uint32_t>()));
    }

    mojo::PendingAssociatedRemote<mojom::CleanerEngineRequests> cleaner;
    mojo::PendingAssociatedRemote<mojom::EngineCleanupResults> cleanup_results;
    if (requests_to_setup_ == CallbacksToSetup::kCleanupRequests) {
      cleaner_impl_->Bind(&cleaner);
      cleanup_results_impl_->BindToCallbacks(
          &cleanup_results, base::BindOnce(base::DoNothing::Once<uint32_t>()));
    }

    // Now call the target process to signal that setup is finished.
    auto operation_started = base::BindOnce([](uint32_t unused_result_code) {});
    if (requests_to_setup_ == CallbacksToSetup::kFileRequests) {
      (*engine_commands_)
          ->Initialize(std::move(file_requests), base::FilePath(),
                       std::move(operation_started));

    } else if (requests_to_setup_ ==
               CallbacksToSetup::kScanAndCleanupRequests) {
      (*engine_commands_)
          ->StartScan(/*enabled_uws=*/std::vector<UwSId>{},
                      /*enabled_locations=*/std::vector<UwS::TraceLocation>{},
                      /*include_details=*/false, std::move(file_requests),
                      std::move(scanner), std::move(scanner_results),
                      std::move(operation_started));

    } else if (requests_to_setup_ == CallbacksToSetup::kCleanupRequests) {
      (*engine_commands_)
          ->StartCleanup(/*enabled_uws=*/std::vector<UwSId>(),
                         std::move(file_requests), std::move(scanner),
                         std::move(cleaner), std::move(cleanup_results),
                         std::move(operation_started));
    }
  }

  void DestroyImpl() override {
    // Reset everything in the reverse order. Reset the associated pointers
    // first since they will error if they are closed after
    // |engine_commands_|.
    cleanup_results_impl_.reset();
    cleaner_impl_.reset();

    scan_results_impl_.reset();
    scanner_impl_.reset();

    file_requests_impl_.reset();
    engine_commands_.reset();
  }

  CallbacksToSetup requests_to_setup_;

  std::unique_ptr<mojo::Remote<mojom::EngineCommands>> engine_commands_;
  std::unique_ptr<EngineFileRequestsImpl> file_requests_impl_;

  std::unique_ptr<EngineRequestsImpl> scanner_impl_;
  std::unique_ptr<CleanerEngineRequestsImpl> cleaner_impl_;

  std::unique_ptr<EngineScanResultsImpl> scan_results_impl_;
  std::unique_ptr<EngineCleanupResultsImpl> cleanup_results_impl_;
};

class SandboxChildProcess : public ChildProcess {
 public:
  explicit SandboxChildProcess(scoped_refptr<MojoTaskRunner> mojo_task_runner);

  void BindEngineCommandsReceiver(
      mojo::PendingReceiver<mojom::EngineCommands> receiver,
      base::WaitableEvent* event);

  scoped_refptr<EngineFileRequestsProxy> GetFileRequestsProxy();
  scoped_refptr<EngineRequestsProxy> GetEngineRequestsProxy();
  scoped_refptr<CleanerEngineRequestsProxy> GetCleanerEngineRequestsProxy();

  void UnbindRequestsRemotes();

  // Exit code value to be used by the child process on connection errors.
  static const int kConnectionErrorExitCode;

 protected:
  ~SandboxChildProcess() override;

 private:
  class FakeEngineDelegate;

  scoped_refptr<FakeEngineDelegate> fake_engine_delegate_;
  std::unique_ptr<EngineCommandsImpl> engine_commands_impl_;
};

}  // namespace chrome_cleaner

#endif  //  CHROME_CHROME_CLEANER_ENGINES_TARGET_SANDBOXED_TEST_HELPERS_H_
