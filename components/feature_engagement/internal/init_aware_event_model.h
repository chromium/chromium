// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_INIT_AWARE_EVENT_MODEL_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_INIT_AWARE_EVENT_MODEL_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "components/feature_engagement/internal/event_model.h"

namespace feature_engagement {

class InitAwareEventModel : public EventModel {
 public:
  InitAwareEventModel(std::unique_ptr<EventModel> event_model);

  InitAwareEventModel(const InitAwareEventModel&) = delete;
  InitAwareEventModel& operator=(const InitAwareEventModel&) = delete;

  ~InitAwareEventModel() override;

  // EventModel implementation.
  void Initialize(OnModelInitializationFinished callback,
                  uint32_t current_day) override;
  bool IsReady() const override;
  const Event* GetEvent(const std::string& event_name) const override;
  uint32_t GetEventCount(const std::string& event_name,
                         uint32_t current_day,
                         uint32_t window_size) const override;
  void IncrementEvent(const std::string& event_name,
                      uint32_t current_day) override;
  void ClearEvent(const std::string& event_name) override;
  void IncrementSnooze(const std::string& event_name,
                       uint32_t current_day,
                       base::Time current_time) override;
  void DismissSnooze(const std::string& event_name) override;
  base::Time GetLastSnoozeTimestamp(
      const std::string& event_name) const override;
  uint32_t GetSnoozeCount(const std::string& event_name,
                          uint32_t window,
                          uint32_t current_day) const override;
  bool IsSnoozeDismissed(const std::string& event_name) const override;

  size_t GetQueuedEventCountForTesting();

 private:
  void OnInitializeComplete(OnModelInitializationFinished callback,
                            bool success);

  std::unique_ptr<EventModel> event_model_;
  std::vector<std::tuple<std::string, uint32_t>> queued_events_;

  // Whether the initialization has completed. This will be set to true once
  // the underlying event model has been initialized, regardless of whether the
  // result was a success or not.
  bool initialization_complete_;

  base::WeakPtrFactory<InitAwareEventModel> weak_ptr_factory_{this};
};

}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_INIT_AWARE_EVENT_MODEL_H_
