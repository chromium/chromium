// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_CROS_HEALTHD_PUBLIC_CPP_FAKE_ROUTINE_CONTROL_H_
#define CHROMEOS_ASH_SERVICES_CROS_HEALTHD_PUBLIC_CPP_FAKE_ROUTINE_CONTROL_H_

#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_routines.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash::cros_healthd {

class FakeRoutineControl : public mojom::RoutineControl {
 public:
  explicit FakeRoutineControl(
      mojo::PendingReceiver<mojom::RoutineControl> pending_receiver,
      mojo::PendingRemote<mojom::RoutineObserver> observer);
  ~FakeRoutineControl() override;

  FakeRoutineControl(const FakeRoutineControl&) = delete;
  FakeRoutineControl& operator=(const FakeRoutineControl&) = delete;

  // `RoutineControl`:
  void GetState(GetStateCallback callback) override;
  void Start() override;
  void ReplyInquiry(mojom::RoutineInquiryReplyPtr reply) override;

  void SetGetStateResponse(mojom::RoutineStatePtr& state);
  bool has_start_been_called() { return start_called_; }
  mojom::RoutineInquiryReplyPtr GetLastInquiryReply();

  mojo::Remote<mojom::RoutineObserver>* GetObserver();
  mojo::Receiver<mojom::RoutineControl>* GetReceiver();

 private:
  // Returned on a call to `GetState`.
  mojom::RoutineStatePtr get_state_response_{mojom::RoutineState::New()};
  // Set to true when `Start` is called.
  bool start_called_ = false;
  // Set to the last `ReplyInquiry` input.
  mojom::RoutineInquiryReplyPtr last_inquiry_reply_;

  mojo::Remote<mojom::RoutineObserver> routine_observer_;
  mojo::Receiver<mojom::RoutineControl> receiver_;
};

}  // namespace ash::cros_healthd

#endif  // CHROMEOS_ASH_SERVICES_CROS_HEALTHD_PUBLIC_CPP_FAKE_ROUTINE_CONTROL_H_
