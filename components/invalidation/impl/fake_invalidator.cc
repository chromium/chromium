// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/fake_invalidator.h"

#include "components/invalidation/public/object_id_invalidation_map.h"

namespace syncer {

FakeInvalidator::FakeInvalidator() {}

FakeInvalidator::~FakeInvalidator() {}

bool FakeInvalidator::IsHandlerRegistered(InvalidationHandler* handler) const {
  return registrar_.IsHandlerRegisteredForTest(handler);
}

ObjectIdSet FakeInvalidator::GetRegisteredIds(
    InvalidationHandler* handler) const {
  return registrar_.GetRegisteredIds(handler);
}

const CoreAccountId& FakeInvalidator::GetCredentialsAccountId() const {
  return account_id_;
}

const std::string& FakeInvalidator::GetCredentialsToken() const {
  return token_;
}

void FakeInvalidator::EmitOnInvalidatorStateChange(InvalidatorState state) {
  registrar_.UpdateInvalidatorState(state);
}

void FakeInvalidator::EmitOnIncomingInvalidation(
    const ObjectIdInvalidationMap& invalidation_map) {
  registrar_.DispatchInvalidationsToHandlers(invalidation_map);
}

void FakeInvalidator::RegisterHandler(InvalidationHandler* handler) {
  registrar_.RegisterHandler(handler);
}

bool FakeInvalidator::UpdateRegisteredIds(InvalidationHandler* handler,
                                          const ObjectIdSet& ids) {
  return registrar_.UpdateRegisteredIds(handler, ids);
}

bool FakeInvalidator::UpdateRegisteredIds(InvalidationHandler*, const Topics&) {
  NOTREACHED();
  return false;
}

void FakeInvalidator::UnregisterHandler(InvalidationHandler* handler) {
  registrar_.UnregisterHandler(handler);
}

InvalidatorState FakeInvalidator::GetInvalidatorState() const {
  return registrar_.GetInvalidatorState();
}

void FakeInvalidator::UpdateCredentials(const CoreAccountId& account_id,
                                        const std::string& token) {
  account_id_ = account_id;
  token_ = token;
}

void FakeInvalidator::RequestDetailedStatus(
    base::Callback<void(const base::DictionaryValue&)> callback) const {
  base::DictionaryValue value;
  callback.Run(value);
}
}  // namespace syncer
