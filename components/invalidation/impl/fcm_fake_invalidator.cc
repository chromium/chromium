// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/fcm_fake_invalidator.h"

#include "components/invalidation/public/invalidation_util.h"
#include "components/invalidation/public/object_id_invalidation_map.h"
#include "components/invalidation/public/topic_invalidation_map.h"

namespace syncer {

FCMFakeInvalidator::FCMFakeInvalidator() {}

FCMFakeInvalidator::~FCMFakeInvalidator() {}

bool FCMFakeInvalidator::IsHandlerRegistered(
    InvalidationHandler* handler) const {
  return registrar_.IsHandlerRegistered(handler);
}

ObjectIdSet FCMFakeInvalidator::GetRegisteredIds(
    InvalidationHandler* handler) const {
  return ConvertTopicsToIds(registrar_.GetRegisteredTopics(handler));
}

const std::string& FCMFakeInvalidator::GetCredentialsEmail() const {
  return email_;
}

const std::string& FCMFakeInvalidator::GetCredentialsToken() const {
  return token_;
}

void FCMFakeInvalidator::EmitOnInvalidatorStateChange(InvalidatorState state) {
  registrar_.UpdateInvalidatorState(state);
}

void FCMFakeInvalidator::EmitOnIncomingInvalidation(
    const ObjectIdInvalidationMap& invalidation_map) {
  registrar_.DispatchInvalidationsToHandlers(
      ConvertObjectIdInvalidationMapToTopicInvalidationMap(invalidation_map));
}

void FCMFakeInvalidator::RegisterHandler(InvalidationHandler* handler) {
  registrar_.RegisterHandler(handler);
}

bool FCMFakeInvalidator::UpdateRegisteredIds(InvalidationHandler* handler,
                                             const ObjectIdSet& ids) {
  return false;
  NOTREACHED();
}

bool FCMFakeInvalidator::UpdateRegisteredIds(InvalidationHandler* handler,
                                             const TopicSet& topics) {
  return registrar_.UpdateRegisteredTopics(handler, topics);
}

void FCMFakeInvalidator::UnregisterHandler(InvalidationHandler* handler) {
  registrar_.UnregisterHandler(handler);
}

InvalidatorState FCMFakeInvalidator::GetInvalidatorState() const {
  return registrar_.GetInvalidatorState();
}

void FCMFakeInvalidator::UpdateCredentials(const std::string& email,
                                           const std::string& token) {
  email_ = email;
  token_ = token;
}

void FCMFakeInvalidator::RequestDetailedStatus(
    base::Callback<void(const base::DictionaryValue&)> callback) const {
  base::DictionaryValue value;
  callback.Run(value);
}
}  // namespace syncer
