// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_CROS_HEALTHD_TESTING_BINDINGS_REMOTE_STATE_H_
#define CHROMEOS_ASH_SERVICES_CROS_HEALTHD_TESTING_BINDINGS_REMOTE_STATE_H_

#include <memory>

#include "base/functional/callback.h"
#include "chromeos/ash/services/cros_healthd/testing/bindings/mojom/state.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace ash::cros_healthd::connectivity {

// RemoteState provides interface to get the remote internal state of
// connectivity test between two context object in each processes.
class RemoteState {
 public:
  RemoteState(const RemoteState&) = delete;
  RemoteState& operator=(const RemoteState&) = delete;
  virtual ~RemoteState() = default;

  static std::unique_ptr<RemoteState> Create(
      mojo::PendingRemote<mojom::State> remote);

 public:
  // Used by TestConsumer to get the LastCallHasNext state. Returns
  // a callback for async call.
  virtual base::OnceCallback<void(base::OnceCallback<void(bool)>)>
  GetLastCallHasNextClosure() = 0;
  // Used by TestConsumer to wait the last function call finished.
  // This is used before each call to a function without callback (no response
  // parameters). For recursive interface checking, the callback will be
  // stacked.
  virtual void WaitLastCall(base::OnceClosure callback) = 0;
  // Used by TestConsumer as the disconnect handler to fulfill the callback of
  // |WaitRemoteLastCall|. When connection error ocurrs (e.g. Interfaces
  // mismatch), the connection will be reset. In this case, the callback of
  // |WaitRemoteLastCall| won't be called. We cannot drop the callback because
  // the |State| interface is still connected. Instead, this function is used to
  // call the remote callback from this side.
  virtual base::OnceClosure GetFulfillLastCallCallbackClosure() = 0;

 protected:
  RemoteState() = default;
};

}  // namespace ash::cros_healthd::connectivity

#endif  // CHROMEOS_ASH_SERVICES_CROS_HEALTHD_TESTING_BINDINGS_REMOTE_STATE_H_
