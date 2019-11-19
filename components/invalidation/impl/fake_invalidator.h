// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_IMPL_FAKE_INVALIDATOR_H_
#define COMPONENTS_INVALIDATION_IMPL_FAKE_INVALIDATOR_H_

#include <string>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "components/invalidation/impl/deprecated_invalidator_registrar.h"
#include "components/invalidation/impl/invalidator.h"
#include "components/invalidation/public/invalidation_util.h"

namespace syncer {

class FakeInvalidator : public Invalidator {
 public:
  FakeInvalidator();
  ~FakeInvalidator() override;

  bool IsHandlerRegistered(InvalidationHandler* handler) const;
  ObjectIdSet GetRegisteredIds(InvalidationHandler* handler) const;
  const std::string& GetUniqueId() const;
  const CoreAccountId& GetCredentialsAccountId() const;
  const std::string& GetCredentialsToken() const;

  void EmitOnInvalidatorStateChange(InvalidatorState state);
  void EmitOnIncomingInvalidation(
      const ObjectIdInvalidationMap& invalidation_map);

  void RegisterHandler(InvalidationHandler* handler) override;
  bool UpdateRegisteredIds(InvalidationHandler* handler,
                           const ObjectIdSet& ids) override;
  bool UpdateRegisteredIds(InvalidationHandler* handler,
                           const Topics& topics) override;
  void UnregisterHandler(InvalidationHandler* handler) override;
  InvalidatorState GetInvalidatorState() const override;
  void UpdateCredentials(const CoreAccountId& account_id,
                         const std::string& token) override;
  void RequestDetailedStatus(base::Callback<void(const base::DictionaryValue&)>
                                 callback) const override;

 private:
  DeprecatedInvalidatorRegistrar registrar_;
  std::string state_;
  CoreAccountId account_id_;
  std::string token_;
};

}  // namespace syncer

#endif  // COMPONENTS_INVALIDATION_IMPL_FAKE_INVALIDATOR_H_
