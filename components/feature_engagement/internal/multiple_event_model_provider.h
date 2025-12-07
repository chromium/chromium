// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_MULTIPLE_EVENT_MODEL_PROVIDER_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_MULTIPLE_EVENT_MODEL_PROVIDER_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/feature_engagement/internal/event_model_provider.h"
#include "components/feature_engagement/public/configuration.h"

namespace feature_engagement {
class EventModel;
class MultipleEventModelWriter;
class EventModelWriter;
class EventModelReader;

// A MultipleEventModelProvider provides the ability to use multiple event
// models.
class MultipleEventModelProvider : public EventModelProvider {
 public:
  explicit MultipleEventModelProvider(
      std::unique_ptr<EventModel> profile_event_model,
      std::unique_ptr<EventModel> device_event_model);

  MultipleEventModelProvider(const MultipleEventModelProvider&) = delete;
  MultipleEventModelProvider& operator=(const MultipleEventModelProvider&) =
      delete;

  ~MultipleEventModelProvider() override;

  // EventModelProvider implementation.
  void Initialize(OnModelInitializationFinished callback,
                  uint32_t current_day) override;
  bool IsReady() const override;
  const EventModelReader* GetEventModelReaderForFeature(
      const FeatureConfig& feature_config) const override;
  EventModelWriter* GetEventModelWriter() override;

 private:
  // Callback for when an underlying event model has been initialized.
  void OnInitializationComplete(bool success);

  // Called when both underlying models have finished their initialization
  // attempts. Invokes the main `initialization_callback_` with the overall
  // success status.
  void EventModelsInitializationCompleted();

  // Barrier closure that is run when both models have initialized.
  base::RepeatingClosure initialization_complete_barrier_;

  // Callback to be invoked once overall initialization is complete.
  OnModelInitializationFinished initialization_callback_;

  // Tracks the overall success of the initialization process. True if both
  // underlying models initialize successfully.
  bool initialization_success_ = false;

  // The profile event model.
  std::unique_ptr<EventModel> profile_event_model_;

  // The local event model.
  std::unique_ptr<EventModel> device_event_model_;

  // Event model writer that combines the profile and device event models.
  std::unique_ptr<MultipleEventModelWriter> multiple_event_model_writer_;

  base::WeakPtrFactory<MultipleEventModelProvider> weak_ptr_factory_{this};
};

}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_MULTIPLE_EVENT_MODEL_PROVIDER_H_
