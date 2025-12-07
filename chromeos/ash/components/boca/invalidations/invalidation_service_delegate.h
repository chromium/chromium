// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_INVALIDATIONS_INVALIDATION_SERVICE_DELEGATE_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_INVALIDATIONS_INVALIDATION_SERVICE_DELEGATE_H_

#include <string>

#include "base/functional/callback_forward.h"

namespace ash::boca {

class InvalidationServiceDelegate {
 public:
  InvalidationServiceDelegate(const InvalidationServiceDelegate&) = delete;
  InvalidationServiceDelegate& operator=(const InvalidationServiceDelegate&) =
      delete;

  virtual ~InvalidationServiceDelegate() = default;

  virtual void UploadToken(
      const std::string& fcm_token,
      base::OnceCallback<void(bool)> on_token_uploaded_cb) = 0;
  virtual void OnInvalidationReceived(const std::string& payload) = 0;

 protected:
  InvalidationServiceDelegate() = default;
};

}  // namespace ash::boca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_INVALIDATIONS_INVALIDATION_SERVICE_DELEGATE_H_
