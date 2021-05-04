// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_RMAD_FAKE_RMAD_CLIENT_H_
#define CHROMEOS_DBUS_RMAD_FAKE_RMAD_CLIENT_H_

#include <vector>

#include "base/component_export.h"
#include "chromeos/dbus/rmad/rmad.pb.h"
#include "chromeos/dbus/rmad/rmad_client.h"

namespace chromeos {

class COMPONENT_EXPORT(RMAD) FakeRmadClient : public RmadClient {
 public:
  FakeRmadClient();
  FakeRmadClient(const FakeRmadClient&) = delete;
  FakeRmadClient& operator=(const FakeRmadClient&) = delete;
  ~FakeRmadClient() override;

  void GetCurrentState(
      DBusMethodCallback<rmad::GetStateReply> callback) override;
  void TransitionNextState(
      const rmad::RmadState& state,
      DBusMethodCallback<rmad::GetStateReply> callback) override;
  void TransitionPreviousState(
      DBusMethodCallback<rmad::GetStateReply> callback) override;

  void AbortRma(DBusMethodCallback<rmad::AbortRmaReply> callback) override;

  void SetFakeStateReplies(std::vector<rmad::GetStateReply> fake_states);

  void SetAbortable(bool abortable);

 private:
  const rmad::GetStateReply& GetStateReply() const;
  const rmad::RmadState& GetState() const;
  rmad::RmadState::StateCase GetStateCase() const;
  size_t NumStates() const;

  std::vector<rmad::GetStateReply> state_replies_;
  size_t state_index_;
  rmad::AbortRmaReply abort_rma_reply_;
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_RMAD_FAKE_RMAD_CLIENT_H_
