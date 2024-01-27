// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_AVAILABILITY_MODEL_IMPL_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_AVAILABILITY_MODEL_IMPL_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/feature_engagement/internal/availability_model.h"
#include "components/feature_engagement/internal/persistent_availability_store.h"

namespace feature_engagement {
// An AvailabilityModel which supports loading data from an
// PersistentAvailabilityStore.
class AvailabilityModelImpl : public AvailabilityModel {
 public:
  using StoreLoadCallback =
      base::OnceCallback<void(PersistentAvailabilityStore::OnLoadedCallback,
                              uint32_t current_day)>;

  explicit AvailabilityModelImpl(StoreLoadCallback load_callback);

  AvailabilityModelImpl(const AvailabilityModelImpl&) = delete;
  AvailabilityModelImpl& operator=(const AvailabilityModelImpl&) = delete;

  ~AvailabilityModelImpl() override;

  // AvailabilityModel implementation.
  void Initialize(OnInitializedCallback callback,
                  uint32_t current_day) override;
  bool IsReady() const override;
  std::optional<uint32_t> GetAvailability(
      const base::Feature& feature) const override;

 private:
  // This is invoked when the store has completed loading.
  void OnStoreLoadComplete(
      OnInitializedCallback on_initialized_callback,
      bool success,
      std::unique_ptr<std::map<std::string, uint32_t>> feature_availabilities);

  // Stores the day number since epoch (1970-01-01) in the local timezone for
  // when the particular feature was made available. The key is the feature
  // name.
  std::map<std::string, uint32_t> feature_availabilities_;

  // Whether the model has successfully initialized.
  bool ready_;

  // A callback for loading availability data from the store. This is reset
  // as soon as it is invoked.
  StoreLoadCallback store_load_callback_;

  base::WeakPtrFactory<AvailabilityModelImpl> weak_ptr_factory_{this};
};

}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_AVAILABILITY_MODEL_IMPL_H_
