// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/fake_invalidation_service.h"

#include "base/functional/callback.h"
#include "base/values.h"
#include "components/invalidation/impl/invalidation_service_util.h"
#include "components/invalidation/public/invalidation_util.h"
#include "components/invalidation/public/topic_invalidation_map.h"

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

void FakeInvalidationService::RegisterInvalidationHandler(
    InvalidationHandler* handler) {
  invalidator_registrar_->RegisterHandler(handler);
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

void FakeInvalidationService::UnsubscribeFromUnregisteredTopics(
    InvalidationHandler* handler) {
  invalidator_registrar_->RemoveUnregisteredTopics(handler);
}

void FakeInvalidationService::UnregisterInvalidationHandler(
    InvalidationHandler* handler) {
  invalidator_registrar_->UnregisterHandler(handler);
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
  Topics subscribed_topics = invalidator_registrar_->GetAllSubscribedTopics();
  if (subscribed_topics.find(invalidation.topic()) == subscribed_topics.end()) {
    fake_ack_handler_.RegisterUnsentInvalidation(&invalidation_copy);
    return;
  }

  // Otherwise, register the invalidation with the fake_ack_handler_ and deliver
  // it to the appropriate consumer.
  fake_ack_handler_.RegisterInvalidation(&invalidation_copy);
  TopicInvalidationMap invalidation_map;
  invalidation_map.Insert(invalidation_copy);
  invalidator_registrar_->DispatchInvalidationsToHandlers(invalidation_map);
}

FakeAckHandler* FakeInvalidationService::GetFakeAckHandler() {
  return &fake_ack_handler_;
}

}  // namespace invalidation
