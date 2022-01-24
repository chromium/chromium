// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_LIBASSISTANT_GRPC_EXTERNAL_SERVICES_GRPC_SERVICES_OBSERVER_H_
#define CHROMEOS_SERVICES_LIBASSISTANT_GRPC_EXTERNAL_SERVICES_GRPC_SERVICES_OBSERVER_H_

#include "base/observer_list_types.h"

namespace chromeos {
namespace libassistant {

// Observer class registered to event handler drivers.
template <class TRequest>
class GrpcServicesObserver : public base::CheckedObserver {
 public:
  // Called when a new event of type |TRequest| has delivered.
  virtual void OnGrpcMessage(const TRequest& request) = 0;

 protected:
  ~GrpcServicesObserver() override = default;
};

}  // namespace libassistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_LIBASSISTANT_GRPC_EXTERNAL_SERVICES_GRPC_SERVICES_OBSERVER_H_
