// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/internal/multiple_event_model_provider.h"

#include "base/barrier_closure.h"
#include "base/functional/bind.h"
#include "components/feature_engagement/internal/event_model.h"
#include "components/feature_engagement/internal/event_model_reader.h"
#include "components/feature_engagement/internal/event_model_writer.h"
#include "components/feature_engagement/internal/multiple_event_model_writer.h"

namespace feature_engagement {

namespace {
const int kNumberOfEventModels = 2;
}  // namespace

MultipleEventModelProvider::MultipleEventModelProvider(
    std::unique_ptr<EventModel> profile_event_model,
    std::unique_ptr<EventModel> device_event_model)
    : profile_event_model_(std::move(profile_event_model)),
      device_event_model_(std::move(device_event_model)),
      multiple_event_model_writer_(std::make_unique<MultipleEventModelWriter>(
          profile_event_model_.get(),
          device_event_model_.get())) {}

MultipleEventModelProvider::~MultipleEventModelProvider() = default;

void MultipleEventModelProvider::Initialize(
    OnModelInitializationFinished callback,
    uint32_t current_day) {
  // If a request is already in progress, drop the new request.
  if (initialization_callback_) {
    return;
  }

  // Set the callback to be invoked once overall initialization is complete.
  initialization_callback_ = std::move(callback);
  // Use a `BarrierClosure` to ensure all async tasks are completed before
  // executing the overall completion callback and returning the data. The
  // BarrierClosure will wait until the `OnInitializationComplete` callback
  // is itself run `kNumberOfEventModels` times.
  initialization_complete_barrier_ = base::BarrierClosure(
      kNumberOfEventModels,
      base::BindOnce(
          &MultipleEventModelProvider::EventModelsInitializationCompleted,
          weak_ptr_factory_.GetWeakPtr()));

  // Initialize to true. The overall success is the AND of all individual model
  // initializations. If any of them fail, this will become false.
  initialization_success_ = true;

  profile_event_model_->Initialize(
      base::BindOnce(&MultipleEventModelProvider::OnInitializationComplete,
                     weak_ptr_factory_.GetWeakPtr()),
      current_day);
  device_event_model_->Initialize(
      base::BindOnce(&MultipleEventModelProvider::OnInitializationComplete,
                     weak_ptr_factory_.GetWeakPtr()),
      current_day);
}

bool MultipleEventModelProvider::IsReady() const {
  return profile_event_model_->IsReady() && device_event_model_->IsReady();
}

const EventModelReader*
MultipleEventModelProvider::GetEventModelReaderForFeature(
    const FeatureConfig& feature_config) const {
  if (feature_config.storage_type == StorageType::PROFILE) {
    return profile_event_model_.get();
  }
  return device_event_model_.get();
}

EventModelWriter* MultipleEventModelProvider::GetEventModelWriter() {
  return multiple_event_model_writer_.get();
}

void MultipleEventModelProvider::OnInitializationComplete(bool success) {
  // If any of the storage types fail to initialize, the overall initialization
  // will fail.
  initialization_success_ = initialization_success_ && success;
  // The `BarrierClosure` must be run regardless of the error type to ensure
  // that it is run `kNumberOfEventModels` times before the
  // `EventModelsInitializationCompleted` callback can be run.
  initialization_complete_barrier_.Run();
}

void MultipleEventModelProvider::EventModelsInitializationCompleted() {
  std::move(initialization_callback_).Run(initialization_success_);
}

}  // namespace feature_engagement
