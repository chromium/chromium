// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_EVENT_MODEL_PROVIDER_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_EVENT_MODEL_PROVIDER_H_

#include "base/functional/callback_forward.h"
#include "components/feature_engagement/internal/event_model_reader.h"
#include "components/feature_engagement/internal/event_model_writer.h"
#include "components/feature_engagement/public/configuration.h"

namespace feature_engagement {

// A virtual class that provides an EventModelReader and an EventModelWriter.
class EventModelProvider {
 public:
  // Callback for when model initialization has finished. The |success|
  // argument denotes whether the model was successfully initialized.
  using OnModelInitializationFinished = base::OnceCallback<void(bool success)>;

  virtual ~EventModelProvider() = default;

  // Initialize the model, including all underlying sub systems. When all
  // required operations have been finished, a callback is posted.
  virtual void Initialize(OnModelInitializationFinished callback,
                          uint32_t current_day) = 0;

  // Returns whether the model is ready, i.e. whether it has been successfully
  // initialized.
  virtual bool IsReady() const = 0;

  // Returns the EventModelReader for the given feature config.
  virtual const EventModelReader* GetEventModelReaderForFeature(
      const FeatureConfig& feature_config) const = 0;

  // Returns the EventModelWriter.
  virtual EventModelWriter* GetEventModelWriter() = 0;

 protected:
  EventModelProvider() = default;
};

}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_EVENT_MODEL_PROVIDER_H_
