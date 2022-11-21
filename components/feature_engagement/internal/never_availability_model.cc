// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/internal/never_availability_model.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace feature_engagement {

NeverAvailabilityModel::NeverAvailabilityModel() = default;

NeverAvailabilityModel::~NeverAvailabilityModel() = default;

void NeverAvailabilityModel::Initialize(OnInitializedCallback callback,
                                        uint32_t current_day) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&NeverAvailabilityModel::ForwardedOnInitializedCallback,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

bool NeverAvailabilityModel::IsReady() const {
  return ready_;
}

absl::optional<uint32_t> NeverAvailabilityModel::GetAvailability(
    const base::Feature& feature) const {
  return absl::nullopt;
}

void NeverAvailabilityModel::ForwardedOnInitializedCallback(
    OnInitializedCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
  ready_ = true;
}

}  // namespace feature_engagement
