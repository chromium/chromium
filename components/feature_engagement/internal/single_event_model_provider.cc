// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/internal/single_event_model_provider.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "components/feature_engagement/internal/event_model.h"

namespace feature_engagement {

SingleEventModelProvider::SingleEventModelProvider(
    std::unique_ptr<EventModel> event_model)
    : event_model_(std::move(event_model)) {
  DCHECK(event_model_);
}

SingleEventModelProvider::~SingleEventModelProvider() = default;

void SingleEventModelProvider::Initialize(
    OnModelInitializationFinished callback,
    uint32_t current_day) {
  event_model_->Initialize(
      base::BindOnce(&SingleEventModelProvider::OnInitializeComplete,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      current_day);
}

bool SingleEventModelProvider::IsReady() const {
  return event_model_->IsReady();
}

const EventModelReader* SingleEventModelProvider::GetEventModelReaderForFeature(
    const FeatureConfig& feature_config) const {
  return event_model_.get();
}

EventModelWriter* SingleEventModelProvider::GetEventModelWriter() {
  return event_model_.get();
}

void SingleEventModelProvider::OnInitializeComplete(
    OnModelInitializationFinished callback,
    bool success) {
  std::move(callback).Run(success);
}

}  // namespace feature_engagement
