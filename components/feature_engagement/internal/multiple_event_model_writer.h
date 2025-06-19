// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_MULTIPLE_EVENT_MODEL_WRITER_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_MULTIPLE_EVENT_MODEL_WRITER_H_

#include "base/memory/weak_ptr.h"
#include "components/feature_engagement/internal/event_model_writer.h"
#include "components/feature_engagement/public/configuration.h"

namespace feature_engagement {
class EventModel;
class EventModelWriter;

// A MultipleEventModelWriter provides the ability to use multiple event models.
class MultipleEventModelWriter : public EventModelWriter {
 public:
  explicit MultipleEventModelWriter(raw_ptr<EventModel> profile_event_model,
                                    raw_ptr<EventModel> device_event_model);

  MultipleEventModelWriter(const MultipleEventModelWriter&) = delete;
  MultipleEventModelWriter& operator=(const MultipleEventModelWriter&) = delete;

  ~MultipleEventModelWriter() override;

  // EventModelWriter implementation.
  void IncrementEvent(const std::string& event_name,
                      uint32_t current_day) override;
  void ClearEvent(const std::string& event_name) override;
  void IncrementSnooze(const std::string& event_name,
                       uint32_t current_day,
                       base::Time current_time) override;
  void DismissSnooze(const std::string& event_name) override;

 private:
  // The profile event model.
  raw_ptr<EventModel> profile_event_model_;

  // The local event model.
  raw_ptr<EventModel> device_event_model_;

  base::WeakPtrFactory<MultipleEventModelWriter> weak_ptr_factory_{this};
};

}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_MULTIPLE_EVENT_MODEL_WRITER_H_
