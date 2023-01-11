// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_QUICK_PAIR_QUICK_PAIR_PROCESS_MANAGER_H_
#define CHROMEOS_ASH_SERVICES_QUICK_PAIR_QUICK_PAIR_PROCESS_MANAGER_H_

#include <memory>
#include <ostream>

#include "base/functional/callback_forward.h"
#include "chromeos/ash/services/quick_pair/public/mojom/fast_pair_data_parser.mojom-forward.h"
#include "chromeos/ash/services/quick_pair/public/mojom/fast_pair_data_parser.mojom.h"
#include "chromeos/ash/services/quick_pair/public/mojom/quick_pair_service.mojom-forward.h"
#include "mojo/public/cpp/bindings/shared_remote.h"

namespace ash {
namespace quick_pair {

// Manages the life cycle of the Quick Pair utility process, which hosts
// functionality for the Quick Pair system.
class QuickPairProcessManager {
 public:
  class ProcessReference {
   public:
    virtual ~ProcessReference() = default;

    virtual const mojo::SharedRemote<mojom::FastPairDataParser>&
    GetFastPairDataParser() const = 0;
  };

  enum class ShutdownReason {
    kNormal = 0,
    kCrash = 1,
    kFastPairDataParserMojoPipeDisconnection = 2,
    kMaxValue = kFastPairDataParserMojoPipeDisconnection,
  };

  virtual ~QuickPairProcessManager() = default;

  using ProcessStoppedCallback = base::OnceCallback<void(ShutdownReason)>;

  // Returns a reference which allows clients to invoke functions implemented by
  // the Quick Pair utility process. If at least one reference is
  // held, we keep the process alive.
  //
  // Note that it is possible that the process could crash and shut down
  // while a reference is still held; if this occurs,
  // |on_process_stopped_callback| will be invoked, and the client should no
  // longer use the invalidated reference.
  //
  // Clients should delete their reference object when they are no
  // longer using it; when there are no remaining references
  // we shut down the utility process. Note that
  // once clients delete the returned reference, they will no
  // longer receive a callback once the process has stopped.
  virtual std::unique_ptr<ProcessReference> GetProcessReference(
      ProcessStoppedCallback on_process_stopped_callback) = 0;
};

std::ostream& operator<<(std::ostream& os,
                         const QuickPairProcessManager::ShutdownReason& reason);

}  // namespace quick_pair
}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_QUICK_PAIR_QUICK_PAIR_PROCESS_MANAGER_H_
