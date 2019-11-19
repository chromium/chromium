// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/feed_journal_mutation.h"

#include <utility>

#include "base/logging.h"

namespace feed {

JournalMutation::JournalMutation(std::string journal_name)
    : journal_name_(std::move(journal_name)),
      start_time_(base::TimeTicks::Now()) {}

JournalMutation::~JournalMutation() = default;

void JournalMutation::AddAppendOperation(std::string value) {
  operations_list_.emplace_back(
      JournalOperation::CreateAppendOperation(std::move(value)));
}

void JournalMutation::AddCopyOperation(std::string to_journal_name) {
  operations_list_.emplace_back(
      JournalOperation::CreateCopyOperation(std::move(to_journal_name)));
}

void JournalMutation::AddDeleteOperation() {
  operations_list_.emplace_back(JournalOperation::CreateDeleteOperation());
}

const std::string& JournalMutation::journal_name() {
  return journal_name_;
}

bool JournalMutation::Empty() {
  return operations_list_.empty();
}

size_t JournalMutation::Size() const {
  return operations_list_.size();
}

base::TimeTicks JournalMutation::GetStartTime() const {
  return start_time_;
}

JournalOperation JournalMutation::TakeFirstOperation() {
  JournalOperation operation = std::move(operations_list_.front());
  operations_list_.pop_front();
  return operation;
}

JournalOperation::Type JournalMutation::FirstOperationType() {
  DCHECK(!operations_list_.empty());
  return operations_list_.front().type();
}

}  // namespace feed
