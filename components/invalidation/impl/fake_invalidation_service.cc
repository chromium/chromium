// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/fake_invalidation_service.h"

#include "components/invalidation/impl/invalidation_service_util.h"
#include "components/invalidation/public/invalidation.h"
#include "components/invalidation/public/invalidation_util.h"

namespace invalidation {

FakeInvalidationService::FakeInvalidationService()
    : client_id_(GenerateInvalidatorClientId()) {
  InvalidatorRegistrarWithMemory::RegisterProfilePrefs(
      pref_service_.registry());
  invalidator_registrar_ = std::make_unique<InvalidatorRegistrarWithMemory>(
      &pref_service_, /*sender_id=*/"sender_id");
  invalidator_registrar_->UpdateInvalidatorState(INVALIDATIONS_ENABLED);
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
    const TopicSet& legacy_topic_set) {
  std::set<TopicData> topic_set;
  for (const auto& topic_name : legacy_topic_set) {
    topic_set.insert(TopicData(topic_name, handler->IsPublicTopic(topic_name)));
  }
  return invalidator_registrar_->UpdateRegisteredTopics(handler, topic_set);
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
  // This function might need to modify the |invalidation|, so we start by
  // making an identical copy of it.
  Invalidation invalidation_copy(invalidation);

  // If no one is listening to this invalidation, do not send it out.
  TopicMap subscribed_topics = invalidator_registrar_->GetAllSubscribedTopics();
  if (subscribed_topics.find(invalidation.topic()) == subscribed_topics.end()) {
    fake_ack_handler_.RegisterUnsentInvalidation(&invalidation_copy);
    return;
  }

  // Otherwise, register the invalidation with the fake_ack_handler_ and deliver
  // it to the appropriate consumer.
  fake_ack_handler_.RegisterInvalidation(&invalidation_copy);
  invalidator_registrar_->DispatchInvalidationToHandlers(invalidation_copy);
}

FakeAckHandler* FakeInvalidationService::GetFakeAckHandler() {
  return &fake_ack_handler_;
}

}  // namespace invalidation
