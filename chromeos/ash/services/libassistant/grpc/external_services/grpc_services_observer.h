// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_LIBASSISTANT_GRPC_EXTERNAL_SERVICES_GRPC_SERVICES_OBSERVER_H_
#define CHROMEOS_ASH_SERVICES_LIBASSISTANT_GRPC_EXTERNAL_SERVICES_GRPC_SERVICES_OBSERVER_H_

#include "base/observer_list_types.h"

namespace ash::libassistant {

// Observer class registered to event handler drivers.
template <class TRequest>
class GrpcServicesObserver : public base::CheckedObserver {
 public:
  // Called when a new event of type |TRequest| has delivered.
  virtual void OnGrpcMessage(const TRequest& request) = 0;

 protected:
  ~GrpcServicesObserver() override = default;
};

}  // namespace ash::libassistant

#endif  // CHROMEOS_ASH_SERVICES_LIBASSISTANT_GRPC_EXTERNAL_SERVICES_GRPC_SERVICES_OBSERVER_H_
