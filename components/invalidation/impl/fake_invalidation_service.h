// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_IMPL_FAKE_INVALIDATION_SERVICE_H_
#define COMPONENTS_INVALIDATION_IMPL_FAKE_INVALIDATION_SERVICE_H_

#include <list>
#include <memory>
#include <utility>

#include "base/callback_forward.h"
#include "components/invalidation/impl/invalidator_registrar_with_memory.h"
#include "components/invalidation/impl/mock_ack_handler.h"
#include "components/invalidation/public/invalidation_service.h"
#include "components/prefs/testing_pref_service.h"

namespace syncer {
class Invalidation;
}

namespace invalidation {

class InvalidationLogger;

// An InvalidationService that emits invalidations only when
// its EmitInvalidationForTest method is called.
class FakeInvalidationService : public InvalidationService {
 public:
  FakeInvalidationService();
  FakeInvalidationService(const FakeInvalidationService& other) = delete;
  FakeInvalidationService& operator=(const FakeInvalidationService& other) =
      delete;
  ~FakeInvalidationService() override;

  void RegisterInvalidationHandler(
      syncer::InvalidationHandler* handler) override;
  bool UpdateInterestedTopics(syncer::InvalidationHandler* handler,
                              const syncer::TopicSet& topics) override;
  void UnregisterInvalidationHandler(
      syncer::InvalidationHandler* handler) override;

  syncer::InvalidatorState GetInvalidatorState() const override;
  std::string GetInvalidatorClientId() const override;
  InvalidationLogger* GetInvalidationLogger() override;
  void RequestDetailedStatus(
      base::RepeatingCallback<void(const base::DictionaryValue&)> caller)
      const override;

  void SetInvalidatorState(syncer::InvalidatorState state);

  const syncer::InvalidatorRegistrarWithMemory& invalidator_registrar() const {
    return *invalidator_registrar_;
  }

  void EmitInvalidationForTest(const syncer::Invalidation& invalidation);

  // Emitted invalidations will be hooked up to this AckHandler.  Clients can
  // query it to assert the invalidaitons are being acked properly.
  syncer::MockAckHandler* GetMockAckHandler();

 private:
  std::string client_id_;
  // |pref_service_| must outlive |invalidator_registrar_|.
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<syncer::InvalidatorRegistrarWithMemory>
      invalidator_registrar_;
  syncer::MockAckHandler mock_ack_handler_;
};

}  // namespace invalidation

#endif  // COMPONENTS_INVALIDATION_IMPL_FAKE_INVALIDATION_SERVICE_H_
