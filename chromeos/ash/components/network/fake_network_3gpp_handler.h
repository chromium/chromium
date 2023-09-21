// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_FAKE_NETWORK_3GPP_HANDLER_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_FAKE_NETWORK_3GPP_HANDLER_H_

#include "chromeos/ash/components/network/network_3gpp_handler.h"

namespace ash {

// Class to use 3gpp SetCarrierLock method.
class FakeNetwork3gppHandler : public Network3gppHandler {
 public:
  FakeNetwork3gppHandler();
  ~FakeNetwork3gppHandler() override;

  void SetCarrierLock(const std::string& config,
                      Modem3gppClient::CarrierLockCallback callback) override;
  void CompleteSetCarrierLock();
  void set_carrier_lock_result(CarrierLockResult result) {
    carrier_lock_result_ = result;
  }

 private:
  Modem3gppClient::CarrierLockCallback carrier_lock_callback_;
  CarrierLockResult carrier_lock_result_;
  base::WeakPtrFactory<FakeNetwork3gppHandler> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_FAKE_NETWORK_3GPP_HANDLER_H_
