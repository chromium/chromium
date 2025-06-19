// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_SINGLE_EVENT_MODEL_PROVIDER_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_SINGLE_EVENT_MODEL_PROVIDER_H_

#include "base/memory/weak_ptr.h"
#include "components/feature_engagement/internal/event_model_provider.h"
#include "components/feature_engagement/internal/event_model_reader.h"
#include "components/feature_engagement/internal/event_model_writer.h"
#include "components/feature_engagement/public/configuration.h"

namespace feature_engagement {
class EventModel;

// A SingleEventModelProvider provides the default implementation of the
// EventModelProvider.
class SingleEventModelProvider : public EventModelProvider {
 public:
  explicit SingleEventModelProvider(std::unique_ptr<EventModel> event_model);

  SingleEventModelProvider(const SingleEventModelProvider&) = delete;
  SingleEventModelProvider& operator=(const SingleEventModelProvider&) = delete;

  ~SingleEventModelProvider() override;

  // EventModelProvider implementation.
  void Initialize(OnModelInitializationFinished callback,
                  uint32_t current_day) override;
  bool IsReady() const override;
  const EventModelReader* GetEventModelReaderForFeature(
      const FeatureConfig& feature_config) const override;
  EventModelWriter* GetEventModelWriter() override;

 private:
  // Callback for when the underlying event model has been initialized.
  void OnInitializeComplete(OnModelInitializationFinished callback,
                            bool success);

  // The underlying event model.
  std::unique_ptr<EventModel> event_model_;

  base::WeakPtrFactory<SingleEventModelProvider> weak_ptr_factory_{this};
};

}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_SINGLE_EVENT_MODEL_PROVIDER_H_
