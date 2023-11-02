// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reading_list/core/reading_list_model.h"

#include "base/check_op.h"
#include "base/observer_list.h"

ReadingListModel::ReadingListModel() : current_batch_updates_count_(0) {}

ReadingListModel::~ReadingListModel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& observer : observers_) {
    observer.ReadingListModelBeingDeleted(this);
  }
}

// Observer methods.
void ReadingListModel::AddObserver(ReadingListModelObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(observer);
  observers_.AddObserver(observer);
  if (loaded()) {
    observer->ReadingListModelLoaded(this);
  }
}

void ReadingListModel::RemoveObserver(ReadingListModelObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
}

// Batch update methods.
bool ReadingListModel::IsPerformingBatchUpdates() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return current_batch_updates_count_ > 0;
}

std::unique_ptr<ReadingListModel::ScopedReadingListBatchUpdate>
ReadingListModel::CreateBatchToken() {
  return std::make_unique<ReadingListModel::ScopedReadingListBatchUpdate>(this);
}

std::unique_ptr<ReadingListModel::ScopedReadingListBatchUpdate>
ReadingListModel::BeginBatchUpdates() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto token = CreateBatchToken();

  ++current_batch_updates_count_;
  if (current_batch_updates_count_ == 1) {
    EnteringBatchUpdates();
  }
  return token;
}

void ReadingListModel::EnteringBatchUpdates() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& observer : observers_)
    observer.ReadingListModelBeganBatchUpdates(this);
}

void ReadingListModel::EndBatchUpdates() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsPerformingBatchUpdates());
  DCHECK(current_batch_updates_count_ > 0);
  --current_batch_updates_count_;
  if (current_batch_updates_count_ == 0) {
    LeavingBatchUpdates();
  }
}

void ReadingListModel::LeavingBatchUpdates() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& observer : observers_)
    observer.ReadingListModelCompletedBatchUpdates(this);
}

ReadingListModel::ScopedReadingListBatchUpdate::ScopedReadingListBatchUpdate(
    ReadingListModel* model)
    : model_(model) {
  model->AddObserver(this);
}

ReadingListModel::ScopedReadingListBatchUpdate::
    ~ScopedReadingListBatchUpdate() {
  if (model_) {
    model_->EndBatchUpdates();
    model_->RemoveObserver(this);
  }
}

void ReadingListModel::ScopedReadingListBatchUpdate::ReadingListModelLoaded(
    const ReadingListModel* model) {}

void ReadingListModel::ScopedReadingListBatchUpdate::
    ReadingListModelBeingShutdown(const ReadingListModel* model) {
  model_->EndBatchUpdates();
  model_->RemoveObserver(this);
  model_ = nullptr;
}
