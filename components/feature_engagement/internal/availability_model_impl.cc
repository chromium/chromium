// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/internal/availability_model_impl.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "components/feature_engagement/internal/persistent_availability_store.h"

namespace feature_engagement {

AvailabilityModelImpl::AvailabilityModelImpl(
    StoreLoadCallback store_load_callback)
    : ready_(false), store_load_callback_(std::move(store_load_callback)) {}

AvailabilityModelImpl::~AvailabilityModelImpl() = default;

void AvailabilityModelImpl::Initialize(OnInitializedCallback callback,
                                       uint32_t current_day) {
  DCHECK(store_load_callback_);
  std::move(store_load_callback_)
      .Run(base::BindOnce(&AvailabilityModelImpl::OnStoreLoadComplete,
                          weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
           current_day);
}

bool AvailabilityModelImpl::IsReady() const {
  return ready_;
}

base::Optional<uint32_t> AvailabilityModelImpl::GetAvailability(
    const base::Feature& feature) const {
  auto search = feature_availabilities_.find(feature.name);
  if (search == feature_availabilities_.end())
    return base::nullopt;

  return search->second;
}

void AvailabilityModelImpl::OnStoreLoadComplete(
    OnInitializedCallback on_initialized_callback,
    bool success,
    std::unique_ptr<std::map<std::string, uint32_t>> feature_availabilities) {
  if (!success) {
    std::move(on_initialized_callback).Run(false);
    return;
  }

  feature_availabilities_ = std::move(*feature_availabilities);

  ready_ = true;
  std::move(on_initialized_callback).Run(true);
}

}  // namespace feature_engagement
