// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/fake_invalidation_handler.h"

namespace invalidation {

FakeInvalidationHandler::FakeInvalidationHandler(const std::string& owner_name)
    : owner_name_(owner_name) {}

FakeInvalidationHandler::~FakeInvalidationHandler() = default;

InvalidatorState FakeInvalidationHandler::GetInvalidatorState() const {
  return state_;
}

const TopicInvalidationMap& FakeInvalidationHandler::GetLastInvalidationMap()
    const {
  return last_invalidation_map_;
}

int FakeInvalidationHandler::GetInvalidationCount() const {
  return invalidation_count_;
}

const std::string& FakeInvalidationHandler::GetInvalidatorClientId() const {
  return client_id_;
}

void FakeInvalidationHandler::OnInvalidatorStateChange(InvalidatorState state) {
  state_ = state;
}

void FakeInvalidationHandler::OnIncomingInvalidation(
    const TopicInvalidationMap& invalidation_map) {
  last_invalidation_map_ = invalidation_map;
  ++invalidation_count_;
}

std::string FakeInvalidationHandler::GetOwnerName() const {
  return owner_name_;
}

bool FakeInvalidationHandler::IsPublicTopic(const Topic& topic) const {
  return topic == "PREFERENCE";
}

void FakeInvalidationHandler::OnInvalidatorClientIdChange(
    const std::string& client_id) {
  client_id_ = client_id;
}

}  // namespace invalidation
