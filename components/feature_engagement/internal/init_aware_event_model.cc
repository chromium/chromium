// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/internal/init_aware_event_model.h"

#include "base/bind.h"

namespace feature_engagement {

InitAwareEventModel::InitAwareEventModel(
    std::unique_ptr<EventModel> event_model)
    : event_model_(std::move(event_model)), initialization_complete_(false) {
  DCHECK(event_model_);
}

InitAwareEventModel::~InitAwareEventModel() = default;

void InitAwareEventModel::Initialize(
    const OnModelInitializationFinished& callback,
    uint32_t current_day) {
  event_model_->Initialize(
      base::Bind(&InitAwareEventModel::OnInitializeComplete,
                 weak_ptr_factory_.GetWeakPtr(), callback),
      current_day);
}

bool InitAwareEventModel::IsReady() const {
  return event_model_->IsReady();
}

const Event* InitAwareEventModel::GetEvent(
    const std::string& event_name) const {
  return event_model_->GetEvent(event_name);
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

void InitAwareEventModel::OnInitializeComplete(
    const OnModelInitializationFinished& callback,
    bool success) {
  initialization_complete_ = true;
  if (success) {
    for (auto& event : queued_events_)
      event_model_->IncrementEvent(std::get<0>(event), std::get<1>(event));
  }
  queued_events_.clear();

  callback.Run(success);
}

size_t InitAwareEventModel::GetQueuedEventCountForTesting() {
  return queued_events_.size();
}

}  // namespace feature_engagement
