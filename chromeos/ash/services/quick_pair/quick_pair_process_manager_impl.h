// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_QUICK_PAIR_QUICK_PAIR_PROCESS_MANAGER_IMPL_H_
#define CHROMEOS_ASH_SERVICES_QUICK_PAIR_QUICK_PAIR_PROCESS_MANAGER_IMPL_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "chromeos/ash/services/quick_pair/public/mojom/fast_pair_data_parser.mojom.h"
#include "chromeos/ash/services/quick_pair/public/mojom/quick_pair_service.mojom.h"
#include "chromeos/ash/services/quick_pair/quick_pair_process_manager.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/shared_remote.h"

namespace ash {
namespace quick_pair {

class QuickPairProcessShutdownController;

class QuickPairProcessManagerImpl : public QuickPairProcessManager {
 public:
  class ProcessReferenceImpl
      : public QuickPairProcessManager::ProcessReference {
   public:
    ProcessReferenceImpl(const mojo::SharedRemote<mojom::FastPairDataParser>&
                             fast_pair_data_parser,
                         base::OnceClosure destructor_callback);
    ~ProcessReferenceImpl() override;

   private:
    // QuickPairProcessManager::ProcessReference
    const mojo::SharedRemote<mojom::FastPairDataParser>& GetFastPairDataParser()
        const override;

    mojo::SharedRemote<mojom::FastPairDataParser> fast_pair_data_parser_;
    base::OnceClosure destructor_callback_;
  };

  QuickPairProcessManagerImpl();
  explicit QuickPairProcessManagerImpl(
      std::unique_ptr<QuickPairProcessShutdownController> shutdown_controller);
  QuickPairProcessManagerImpl(const QuickPairProcessManagerImpl&) = delete;
  QuickPairProcessManagerImpl& operator=(const QuickPairProcessManagerImpl&) =
      delete;
  ~QuickPairProcessManagerImpl() override;

  // QuickPairProcessManager:
  std::unique_ptr<ProcessReference> GetProcessReference(
      ProcessStoppedCallback on_process_stopped_callback) override;

 private:
  void BindToProcess();
  void OnReferenceDeleted(base::UnguessableToken id);
  void ShutdownProcess(ShutdownReason shutdown_reason);

  std::unique_ptr<QuickPairProcessShutdownController>
      process_shutdown_controller_;

  base::flat_map<base::UnguessableToken, ProcessStoppedCallback>
      id_to_process_stopped_callback_map_;

  mojo::Remote<mojom::QuickPairService> service_;

  mojo::SharedRemote<mojom::FastPairDataParser> fast_pair_data_parser_;

  base::WeakPtrFactory<QuickPairProcessManagerImpl> weak_ptr_factory_{this};
};

}  // namespace quick_pair
}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_QUICK_PAIR_QUICK_PAIR_PROCESS_MANAGER_IMPL_H_
