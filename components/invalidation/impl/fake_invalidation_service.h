// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_IMPL_FAKE_INVALIDATION_SERVICE_H_
#define COMPONENTS_INVALIDATION_IMPL_FAKE_INVALIDATION_SERVICE_H_

#include <memory>
#include <utility>

#include "base/functional/callback_forward.h"
#include "components/invalidation/impl/fake_ack_handler.h"
#include "components/invalidation/impl/invalidator_registrar_with_memory.h"
#include "components/invalidation/public/invalidation_service.h"
#include "components/prefs/testing_pref_service.h"

namespace invalidation {

class Invalidation;
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

  void RegisterInvalidationHandler(InvalidationHandler* handler) override;
  bool UpdateInterestedTopics(InvalidationHandler* handler,
                              const TopicSet& topics) override;
  void UnsubscribeFromUnregisteredTopics(InvalidationHandler* handler) override;
  void UnregisterInvalidationHandler(InvalidationHandler* handler) override;

  InvalidatorState GetInvalidatorState() const override;
  std::string GetInvalidatorClientId() const override;
  InvalidationLogger* GetInvalidationLogger() override;
  void RequestDetailedStatus(
      base::RepeatingCallback<void(base::Value::Dict)> caller) const override;

  void SetInvalidatorState(InvalidatorState state);

  const InvalidatorRegistrarWithMemory& invalidator_registrar() const {
    return *invalidator_registrar_;
  }

  void EmitInvalidationForTest(const Invalidation& invalidation);

  // Emitted invalidations will be hooked up to this AckHandler.  Clients can
  // query it to assert the invalidaitons are being acked properly.
  FakeAckHandler* GetFakeAckHandler();

 private:
  std::string client_id_;
  // |pref_service_| must outlive |invalidator_registrar_|.
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<InvalidatorRegistrarWithMemory> invalidator_registrar_;
  FakeAckHandler fake_ack_handler_;
};

}  // namespace invalidation

#endif  // COMPONENTS_INVALIDATION_IMPL_FAKE_INVALIDATION_SERVICE_H_
