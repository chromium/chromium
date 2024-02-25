// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/internal/init_aware_event_model.h"

#include "base/functional/bind.h"

namespace feature_engagement {

InitAwareEventModel::InitAwareEventModel(
    std::unique_ptr<EventModel> event_model)
    : event_model_(std::move(event_model)), initialization_complete_(false) {
  DCHECK(event_model_);
}

InitAwareEventModel::~InitAwareEventModel() = default;

void InitAwareEventModel::Initialize(OnModelInitializationFinished callback,
                                     uint32_t current_day) {
  event_model_->Initialize(
      base::BindOnce(&InitAwareEventModel::OnInitializeComplete,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      current_day);
}

bool InitAwareEventModel::IsReady() const {
  return event_model_->IsReady();
}

const Event* InitAwareEventModel::GetEvent(
    const std::string& event_name) const {
  return event_model_->GetEvent(event_name);
}

uint32_t InitAwareEventModel::GetEventCount(const std::string& event_name,
                                            uint32_t current_day,
                                            uint32_t window_size) const {
  return event_model_->GetEventCount(event_name, current_day, window_size);
}

void InitAwareEventModel::IncrementEvent(const std::string& event_name,
                                         uint32_t current_day) {
  if (IsReady()) {
    event_model_->IncrementEvent(event_name, current_day);
    return;
  }

  if (initialization_complete_)
    return;

  queued_events_.push_back(std::tie(event_name, current_day));
}

void InitAwareEventModel::ClearEvent(const std::string& event_name) {
  // If the embedded model is ready, clear it out.
  //
  // TODO(dfried): consider storing the events to be deleted and removing them
  // when the embedded model is loaded.
  if (IsReady()) {
    event_model_->ClearEvent(event_name);
    return;
  }

  // Also clear any queued events of the same type that haven't yet been added
  // to the embedded model.
  const auto temp = std::move(queued_events_);
  std::copy_if(temp.begin(), temp.end(), std::back_inserter(queued_events_),
               [event_name](const auto& val) {
                 return std::get<std::string>(val) != event_name;
               });
}

void InitAwareEventModel::IncrementSnooze(const std::string& event_name,
                                          uint32_t current_day,
                                          base::Time current_time) {
  event_model_->IncrementSnooze(event_name, current_day, current_time);
}

void InitAwareEventModel::DismissSnooze(const std::string& event_name) {
  event_model_->DismissSnooze(event_name);
}

base::Time InitAwareEventModel::GetLastSnoozeTimestamp(
    const std::string& event_name) const {
  return event_model_->GetLastSnoozeTimestamp(event_name);
}

uint32_t InitAwareEventModel::GetSnoozeCount(const std::string& event_name,
                                             uint32_t window,
                                             uint32_t current_day) const {
  return event_model_->GetSnoozeCount(event_name, window, current_day);
}

bool InitAwareEventModel::IsSnoozeDismissed(
    const std::string& event_name) const {
  return event_model_->IsSnoozeDismissed(event_name);
}

void InitAwareEventModel::OnInitializeComplete(
    OnModelInitializationFinished callback,
    bool success) {
  initialization_complete_ = true;
  if (success) {
    for (auto& event : queued_events_)
      event_model_->IncrementEvent(std::get<0>(event), std::get<1>(event));
  }
  queued_events_.clear();

  std::move(callback).Run(success);
}

size_t InitAwareEventModel::GetQueuedEventCountForTesting() {
  return queued_events_.size();
}

}  // namespace feature_engagement
