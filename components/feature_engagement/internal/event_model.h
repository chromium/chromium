// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_EVENT_MODEL_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_EVENT_MODEL_H_

#include "base/functional/callback_forward.h"
#include "components/feature_engagement/internal/event_model_reader.h"
#include "components/feature_engagement/internal/event_model_writer.h"

namespace feature_engagement {

// A EventModel provides all necessary runtime state.
class EventModel : public EventModelReader, public EventModelWriter {
 public:
  // Callback for when model initialization has finished. The |success|
  // argument denotes whether the model was successfully initialized.
  using OnModelInitializationFinished = base::OnceCallback<void(bool success)>;

  EventModel(const EventModel&) = delete;
  EventModel& operator=(const EventModel&) = delete;

  ~EventModel() override = default;

  // Initialize the model, including all underlying sub systems. When all
  // required operations have been finished, a callback is posted.
  virtual void Initialize(OnModelInitializationFinished callback,
                          uint32_t current_day) = 0;

 protected:
  EventModel() = default;
};

}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_EVENT_MODEL_H_
