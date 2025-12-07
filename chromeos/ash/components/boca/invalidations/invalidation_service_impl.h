// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_INVALIDATIONS_INVALIDATION_SERVICE_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_INVALIDATIONS_INVALIDATION_SERVICE_IMPL_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "chromeos/ash/components/boca/invalidations/fcm_handler.h"
#include "components/account_id/account_id.h"
#include "google_apis/common/api_error_codes.h"
#include "net/base/backoff_entry.h"

namespace instance_id {
class InstanceIDDriver;
}

namespace gcm {
class GCMDriver;
}

namespace ash::boca {

class InvalidationServiceDelegate;

class InvalidationService {
 public:
  virtual ~InvalidationService() = default;

  virtual void ShutDown() = 0;
};

class InvalidationServiceImpl : public InvalidationService,
                                public InvalidationsListener,
                                public FCMRegistrationTokenObserver {
 public:
  // `delegate` should outlive the InvalidationServiceImpl instance.
  InvalidationServiceImpl(gcm::GCMDriver* gcm_driver,
                          instance_id::InstanceIDDriver* instance_id_driver,
                          InvalidationServiceDelegate* delegate);
  ~InvalidationServiceImpl() override;

  // InvalidationsListener implementation.
  void OnInvalidationReceived(const std::string& payload) override;

  // FCMRegistrationTokenObserver implementation.
  void OnFCMRegistrationTokenChanged() override;

  void UploadToken();
  void OnTokenUploaded(bool success);

  void ShutDown() override;

  FCMHandlerImpl* fcm_handler() { return fcm_handler_.get(); }

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  net::BackoffEntry upload_retry_backoff_;
  base::OneShotTimer token_refresh_timer_;
  std::unique_ptr<FCMHandlerImpl> fcm_handler_;
  raw_ptr<InvalidationServiceDelegate> delegate_;
  base::WeakPtrFactory<InvalidationServiceImpl> weak_factory_{this};
};

}  // namespace ash::boca
#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_INVALIDATIONS_INVALIDATION_SERVICE_IMPL_H_
