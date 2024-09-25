// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_INVALIDATIONS_INVALIDATION_SERVICE_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_INVALIDATIONS_INVALIDATION_SERVICE_IMPL_H_

#include <memory>

#include "base/time/time.h"
#include "base/types/expected.h"
#include "chromeos/ash/components/boca/invalidations/fcm_handler.h"
#include "components/account_id/account_id.h"
#include "google_apis/common/api_error_codes.h"

namespace instance_id {
class InstanceIDDriver;
}

namespace gcm {
class GCMDriver;
}

namespace ash::boca {
class BocaSessionManager;
class SessionClientImpl;
class InvalidationServiceImpl : public InvalidationsListener,
                                FCMRegistrationTokenObserver {
 public:
  inline static constexpr char kSenderId[] = "947897361853";
  inline static constexpr char kApplicationId[] =
      "com.google.chrome.boca.fcm.invalidations";
  InvalidationServiceImpl(gcm::GCMDriver* gcm_driver,
                          instance_id::InstanceIDDriver* instance_id_driver,
                          AccountId account_id,
                          BocaSessionManager* boca_session_manager_,
                          SessionClientImpl* session_client_impl_);
  ~InvalidationServiceImpl() override;

  // InvalidationsListener implementation.
  void OnInvalidationReceived(const std::string& payload) override;

  // FCMRegistrationTokenObserver implementation.
  void OnFCMRegistrationTokenChanged() override;

  void OnTokenUploaded(base::expected<bool, google_apis::ApiErrorCode> result);

  virtual void ShutDown();

  FCMHandler* fcm_handler() { return fcm_handler_.get(); }

 private:
  SEQUENCE_CHECKER(sequence_checker_);
  std::unique_ptr<FCMHandler> fcm_handler_;
  AccountId account_id_;
  raw_ptr<BocaSessionManager> boca_session_manager_;
  raw_ptr<SessionClientImpl> session_client_impl_;
  base::WeakPtrFactory<InvalidationServiceImpl> weak_factory_{this};
};

}  // namespace ash::boca
#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_INVALIDATIONS_INVALIDATION_SERVICE_IMPL_H_
