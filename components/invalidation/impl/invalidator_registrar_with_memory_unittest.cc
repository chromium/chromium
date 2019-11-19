// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/invalidator_registrar_with_memory.h"

#include <memory>

#include "base/macros.h"
#include "components/invalidation/impl/invalidator_test_template.h"
#include "components/invalidation/public/topic_invalidation_map.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

class RegistrarInvalidator : public Invalidator {
 public:
  RegistrarInvalidator() {
    InvalidatorRegistrarWithMemory::RegisterProfilePrefs(
        pref_service_.registry());
    registrar_ = std::make_unique<InvalidatorRegistrarWithMemory>(
        &pref_service_, "sender_id", /*migrate_old_prefs=*/false);
  }
  ~RegistrarInvalidator() override {}

  InvalidatorRegistrarWithMemory* GetRegistrar() { return registrar_.get(); }

  // Invalidator implementation.
  void RegisterHandler(InvalidationHandler* handler) override {
    registrar_->RegisterHandler(handler);
  }

  bool UpdateRegisteredIds(InvalidationHandler* handler,
                           const ObjectIdSet& ids) override {
    return registrar_->UpdateRegisteredTopics(handler,
                                              ConvertIdsToTopics(ids, handler));
  }

  bool UpdateRegisteredIds(InvalidationHandler* handler,
                           const Topics& ids) override {
    NOTREACHED();
    return false;
  }

  void UnregisterHandler(InvalidationHandler* handler) override {
    registrar_->UnregisterHandler(handler);
  }

  InvalidatorState GetInvalidatorState() const override {
    return registrar_->GetInvalidatorState();
  }

  void UpdateCredentials(const CoreAccountId& account_id,
                         const std::string& token) override {
    // Do nothing.
  }

  void RequestDetailedStatus(
      base::Callback<void(const base::DictionaryValue&)> call) const override {
    // Do nothing.
  }

 private:
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<InvalidatorRegistrarWithMemory> registrar_;

  DISALLOW_COPY_AND_ASSIGN(RegistrarInvalidator);
};

class RegistrarInvalidatorWithMemoryTestDelegate {
 public:
  RegistrarInvalidatorWithMemoryTestDelegate() {}

  ~RegistrarInvalidatorWithMemoryTestDelegate() { DestroyInvalidator(); }

  void CreateInvalidator(const std::string& invalidator_client_id,
                         const std::string& initial_state,
                         const base::WeakPtr<InvalidationStateTracker>&
                             invalidation_state_tracker) {
    DCHECK(!invalidator_);
    invalidator_ = std::make_unique<RegistrarInvalidator>();
  }

  RegistrarInvalidator* GetInvalidator() { return invalidator_.get(); }

  void DestroyInvalidator() { invalidator_.reset(); }

  void WaitForInvalidator() {
    // Do nothing.
  }

  void TriggerOnInvalidatorStateChange(InvalidatorState state) {
    invalidator_->GetRegistrar()->UpdateInvalidatorState(state);
  }

  void TriggerOnIncomingInvalidation(
      const ObjectIdInvalidationMap& invalidation_map) {
    TopicInvalidationMap topics_map;
    std::vector<syncer::Invalidation> invalidations;
    invalidation_map.GetAllInvalidations(&invalidations);
    for (const auto& invalidation : invalidations) {
      topics_map.Insert(invalidation);
    }

    invalidator_->GetRegistrar()->DispatchInvalidationsToHandlers(topics_map);
  }

 private:
  std::unique_ptr<RegistrarInvalidator> invalidator_;
};

INSTANTIATE_TYPED_TEST_SUITE_P(RegistrarInvalidatorWithMemoryTest,
                               InvalidatorTest,
                               RegistrarInvalidatorWithMemoryTestDelegate);

}  // namespace

}  // namespace syncer
