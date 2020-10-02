// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PHONEHUB_DO_NOT_DISTURB_CONTROLLER_IMPL_H_
#define CHROMEOS_COMPONENTS_PHONEHUB_DO_NOT_DISTURB_CONTROLLER_IMPL_H_

#include "chromeos/components/phonehub/do_not_disturb_controller.h"

namespace chromeos {
namespace phonehub {

class MessageSender;

// Responsible for sending and receiving states in regards to the DoNotDisturb
// feature of the user's remote phone.
class DoNotDisturbControllerImpl : public DoNotDisturbController {
 public:
  DoNotDisturbControllerImpl(MessageSender* message_sender);
  ~DoNotDisturbControllerImpl() override;

 private:
  friend class DoNotDisturbControllerImplTest;

  // DoNotDisturbController:
  bool IsDndEnabled() const override;
  void SetDoNotDisturbStateInternal(bool is_dnd_enabled) override;
  void RequestNewDoNotDisturbState(bool enabled) override;

  bool is_dnd_enabled_ = false;
  MessageSender* message_sender_;
};

}  // namespace phonehub
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_PHONEHUB_DO_NOT_DISTURB_CONTROLLER_IMPL_H_
