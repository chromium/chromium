// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/quick_pair/quick_pair_process_manager_impl.h"

#include <memory>

#include "ash/quick_pair/common/quick_pair_browser_delegate.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/unguessable_token.h"
#include "chromeos/ash/services/quick_pair/public/mojom/fast_pair_data_parser.mojom.h"
#include "chromeos/ash/services/quick_pair/quick_pair_process_shutdown_controller.h"
#include "components/cross_device/logging/logging.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace ash {
namespace quick_pair {

QuickPairProcessManagerImpl::ProcessReferenceImpl::ProcessReferenceImpl(
    const mojo::SharedRemote<mojom::FastPairDataParser>& fast_pair_data_parser,
    base::OnceClosure destructor_callback)
    : fast_pair_data_parser_(fast_pair_data_parser),
      destructor_callback_(std::move(destructor_callback)) {}

QuickPairProcessManagerImpl::ProcessReferenceImpl::~ProcessReferenceImpl() {
  // Reset the SharedRemotes before the destructor callback is run to ensure
  // that all connections to the utility process are destroyed before we attempt
  // to tear the process down.
  fast_pair_data_parser_.reset();

  std::move(destructor_callback_).Run();
}

const mojo::SharedRemote<mojom::FastPairDataParser>&
QuickPairProcessManagerImpl::ProcessReferenceImpl::GetFastPairDataParser()
    const {
  return fast_pair_data_parser_;
}

QuickPairProcessManagerImpl::QuickPairProcessManagerImpl()
    : QuickPairProcessManagerImpl(
          std::make_unique<QuickPairProcessShutdownController>()) {}

QuickPairProcessManagerImpl::QuickPairProcessManagerImpl(
    std::unique_ptr<QuickPairProcessShutdownController> shutdown_controller)
    : process_shutdown_controller_(std::move(shutdown_controller)) {}

QuickPairProcessManagerImpl::~QuickPairProcessManagerImpl() = default;

std::unique_ptr<QuickPairProcessManager::ProcessReference>
QuickPairProcessManagerImpl::GetProcessReference(
    QuickPairProcessManager::ProcessStoppedCallback
        on_process_stopped_callback) {
  // Start the process if we don't have valid bound Remotes.
  if (!service_ || !fast_pair_data_parser_)
    BindToProcess();

  CD_LOG(VERBOSE, Feature::FP)
      << __func__ << ": New process reference requested.";

  // Ensure the process isn't shutdown (it's possible the controller has started
  // their procedure by here.)
  process_shutdown_controller_->Stop();

  auto reference_id = base::UnguessableToken::Create();
  id_to_process_stopped_callback_map_[reference_id] =
      std::move(on_process_stopped_callback);

  return std::make_unique<ProcessReferenceImpl>(
      fast_pair_data_parser_,
      base::BindOnce(&QuickPairProcessManagerImpl::OnReferenceDeleted,
                     weak_ptr_factory_.GetWeakPtr(), reference_id));
}

void QuickPairProcessManagerImpl::BindToProcess() {
  DCHECK(!service_ && !fast_pair_data_parser_);

  CD_LOG(INFO, Feature::FP) << "Starting up QuickPair utility process";

  QuickPairBrowserDelegate::Get()->RequestService(
      service_.BindNewPipeAndPassReceiver());

  service_.set_disconnect_handler(
      base::BindOnce(&QuickPairProcessManagerImpl::ShutdownProcess,
                     weak_ptr_factory_.GetWeakPtr(), ShutdownReason::kCrash));

  mojo::PendingRemote<mojom::FastPairDataParser> fast_pair_data_parser;
  mojo::PendingReceiver<mojom::FastPairDataParser>
      fast_pair_data_parser_receiver =
          fast_pair_data_parser.InitWithNewPipeAndPassReceiver();
  fast_pair_data_parser_.Bind(std::move(fast_pair_data_parser),
                              /*bind_task_runner=*/nullptr);
  fast_pair_data_parser_.set_disconnect_handler(
      base::BindOnce(&QuickPairProcessManagerImpl::ShutdownProcess,
                     weak_ptr_factory_.GetWeakPtr(),
                     ShutdownReason::kFastPairDataParserMojoPipeDisconnection),
      base::SequencedTaskRunner::GetCurrentDefault());

  service_->Connect(std::move(fast_pair_data_parser_receiver));
}

void QuickPairProcessManagerImpl::OnReferenceDeleted(
    base::UnguessableToken id) {
  auto it = id_to_process_stopped_callback_map_.find(id);
  DCHECK(it != id_to_process_stopped_callback_map_.end());

  // Do not call the callback because its owner has already explicitly deleted
  // its reference.
  id_to_process_stopped_callback_map_.erase(it);

  // If there are still active references, the process should be kept alive, so
  // return early.
  if (!id_to_process_stopped_callback_map_.empty())
    return;

  CD_LOG(VERBOSE, Feature::FP)
      << "All process references have been released. Starting shutdown timer";

  process_shutdown_controller_->Start(
      base::BindOnce(&QuickPairProcessManagerImpl::ShutdownProcess,
                     weak_ptr_factory_.GetWeakPtr(), ShutdownReason::kNormal));
}

void QuickPairProcessManagerImpl::ShutdownProcess(
    ShutdownReason shutdown_reason) {
  if (!service_ && !fast_pair_data_parser_)
    return;

  CD_LOG(WARNING, Feature::FP) << __func__ << ": " << shutdown_reason;

  // Ensure that we don't try to stop the process again.
  process_shutdown_controller_->Stop();

  // Prevent the Remotes' disconnect handler and the OnReferenceDeleted
  // callbacks from firing.
  weak_ptr_factory_.InvalidateWeakPtrs();

  service_.reset();
  fast_pair_data_parser_.reset();

  // Move the map to a local variable to ensure that the instance field is
  // empty before any callbacks are made.
  auto old_map = std::move(id_to_process_stopped_callback_map_);
  id_to_process_stopped_callback_map_.clear();

  // Invoke the "process stopped" callback for each client.
  for (auto& it : old_map)
    std::move(it.second).Run(shutdown_reason);
}

}  // namespace quick_pair
}  // namespace ash
