// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/cros_healthd/testing/bindings/local_state.h"

#include <utility>
#include <vector>

#include "mojo/public/cpp/bindings/receiver.h"

namespace ash::cros_healthd::connectivity {

class LocalStateImpl : public LocalState, public mojom::State {
 public:
  explicit LocalStateImpl(mojo::PendingReceiver<mojom::State> receiver)
      : receiver_(this, std::move(receiver)) {}
  LocalStateImpl(const LocalStateImpl&) = delete;
  LocalStateImpl& operator=(const LocalStateImpl&) = delete;
  virtual ~LocalStateImpl() = default;

 public:
  // Override State.
  void SetLastCallHasNext(bool has_next) override {
    last_call_has_next_ = has_next;
  }
  // Override mojom::State.
  void LastCallHasNext(LastCallHasNextCallback callback) override {
    std::move(callback).Run(last_call_has_next_);
  }
  // Override mojom::State.
  void WaitLastCall(WaitLastCallCallback callback) override {
    last_call_callback_stack_.push_back(std::move(callback));
  }
  // Override both State and mojom::State.
  void FulfillLastCallCallback() override {
    CHECK(last_call_callback_stack_.size());
    // Pop the callback before run into it.
    auto callback = std::move(last_call_callback_stack_.back());
    last_call_callback_stack_.pop_back();
    std::move(callback).Run();
  }

 private:
  bool last_call_has_next_ = false;
  // This stack is used to stack the LastCallCallback. This is necessary for
  // recursive interface checking.
  std::vector<base::OnceClosure> last_call_callback_stack_;

  mojo::Receiver<mojom::State> receiver_;
};

std::unique_ptr<LocalState> LocalState::Create(
    mojo::PendingReceiver<mojom::State> receiver) {
  return std::make_unique<LocalStateImpl>(std::move(receiver));
}

}  // namespace ash::cros_healthd::connectivity
