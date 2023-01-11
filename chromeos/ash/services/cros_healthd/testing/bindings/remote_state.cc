// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/cros_healthd/testing/bindings/remote_state.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash::cros_healthd::connectivity {

class RemoteStateImpl : public RemoteState {
 public:
  explicit RemoteStateImpl(mojo::PendingRemote<mojom::State> remote)
      : remote_(std::move(remote)) {}
  RemoteStateImpl(const RemoteStateImpl&) = delete;
  RemoteStateImpl& operator=(const RemoteStateImpl&) = delete;
  virtual ~RemoteStateImpl() = default;

 public:
  // Overrides.
  base::OnceCallback<void(base::OnceCallback<void(bool)>)>
  GetLastCallHasNextClosure() override {
    return base::BindOnce(&RemoteStateImpl::LastCallHasNext,
                          weak_factory_.GetWeakPtr());
  }

  void WaitLastCall(base::OnceClosure callback) override {
    remote_->WaitLastCall(std::move(callback));
  }

  base::OnceClosure GetFulfillLastCallCallbackClosure() override {
    return base::BindOnce(&RemoteStateImpl::FulfillLastCallCallback,
                          weak_factory_.GetWeakPtr());
  }

 private:
  void LastCallHasNext(base::OnceCallback<void(bool)> callback) {
    remote_->LastCallHasNext(std::move(callback));
  }

  void FulfillLastCallCallback() { remote_->FulfillLastCallCallback(); }

 private:
  mojo::Remote<mojom::State> remote_;

  // Must be the last member of the class.
  base::WeakPtrFactory<RemoteStateImpl> weak_factory_{this};
};

std::unique_ptr<RemoteState> RemoteState::Create(
    mojo::PendingRemote<mojom::State> remote) {
  return std::make_unique<RemoteStateImpl>(std::move(remote));
}

}  // namespace ash::cros_healthd::connectivity
