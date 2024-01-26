// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_AVAILABILITY_MODEL_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_AVAILABILITY_MODEL_H_

#include <stdint.h>

#include <optional>

#include "base/feature_list.h"
#include "base/functional/callback_forward.h"

namespace feature_engagement {

// An AvailabilityModel tracks when each feature was made available to an
// end user.
class AvailabilityModel {
 public:
  // Invoked when the availability data has finished loading, and whether the
  // load was a success. In the case of a failure, it is invalid to ever call
  // GetAvailability(...).
  using OnInitializedCallback = base::OnceCallback<void(bool success)>;

  AvailabilityModel(const AvailabilityModel&) = delete;
  AvailabilityModel& operator=(const AvailabilityModel&) = delete;

  virtual ~AvailabilityModel() = default;

  // Starts initialization of the AvailabilityModel.
  virtual void Initialize(OnInitializedCallback callback,
                          uint32_t current_day) = 0;

  // Returns whether the model is ready, i.e. whether it has been successfully
  // initialized.
  virtual bool IsReady() const = 0;

  // Returns the day number since epoch (1970-01-01) in the local timezone for
  // when the particular |feature| was made available.
  // See TimeProvider::GetCurrentDay().
  virtual std::optional<uint32_t> GetAvailability(
      const base::Feature& feature) const = 0;

 protected:
  AvailabilityModel() = default;
};

}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_AVAILABILITY_MODEL_H_
