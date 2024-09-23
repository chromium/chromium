// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/fake_invalidation_service.h"

#include "components/invalidation/impl/invalidation_service_util.h"
#include "components/invalidation/public/invalidation.h"
#include "components/invalidation/public/invalidation_util.h"
#include "components/invalidation/public/invalidator_state.h"

namespace invalidation {

FakeInvalidationService::FakeInvalidationService()
    : client_id_(GenerateInvalidatorClientId()) {
  InvalidatorRegistrarWithMemory::RegisterProfilePrefs(
      pref_service_.registry());
  invalidator_registrar_ = std::make_unique<InvalidatorRegistrarWithMemory>(
      &pref_service_, /*sender_id=*/"sender_id");
  invalidator_registrar_->UpdateInvalidatorState(InvalidatorState::kEnabled);
}

FakeInvalidationService::~FakeInvalidationService() = default;

void FakeInvalidationService::AddObserver(InvalidationHandler* handler) {
  invalidator_registrar_->AddObserver(handler);
}

bool FakeInvalidationService::HasObserver(
    const InvalidationHandler* handler) const {
  return invalidator_registrar_->HasObserver(handler);
}

bool FakeInvalidationService::UpdateInterestedTopics(
    InvalidationHandler* handler,
    const TopicSet& topic_set) {
  TopicMap topic_map;
  for (const auto& topic_name : topic_set) {
    topic_map[topic_name] = TopicMetadata(handler->IsPublicTopic(topic_name));
  }
  return invalidator_registrar_->UpdateRegisteredTopics(handler, topic_map);
}

void FakeInvalidationService::RemoveObserver(
    const InvalidationHandler* handler) {
  invalidator_registrar_->RemoveObserver(handler);
}

InvalidatorState FakeInvalidationService::GetInvalidatorState() const {
  return invalidator_registrar_->GetInvalidatorState();
}

std::string FakeInvalidationService::GetInvalidatorClientId() const {
  return client_id_;
}

void FakeInvalidationService::SetInvalidatorState(InvalidatorState state) {
  invalidator_registrar_->UpdateInvalidatorState(state);
}

void FakeInvalidationService::EmitInvalidationForTest(
    const Invalidation& invalidation) {
  invalidator_registrar_->DispatchInvalidationToHandlers(invalidation);
}

}  // namespace invalidation
