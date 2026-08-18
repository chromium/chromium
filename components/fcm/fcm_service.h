// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FCM_FCM_SERVICE_H_
#define COMPONENTS_FCM_FCM_SERVICE_H_

#include <memory>

#include "components/fcm/fcm_driver.h"
#include "components/keyed_service/core/keyed_service.h"

namespace fcm {

// KeyedService wrapper providing access to the underlying FcmDriver for a
// Profile.
class FcmService : public KeyedService {
 public:
  explicit FcmService(std::unique_ptr<FcmDriver> driver);
  ~FcmService() override;

  FcmService(const FcmService&) = delete;
  FcmService& operator=(const FcmService&) = delete;

  FcmDriver* driver() { return driver_.get(); }
  const FcmDriver* driver() const { return driver_.get(); }

 private:
  std::unique_ptr<FcmDriver> driver_;
};

}  // namespace fcm

#endif  // COMPONENTS_FCM_FCM_SERVICE_H_
