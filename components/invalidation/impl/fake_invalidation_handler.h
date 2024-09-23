// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_IMPL_FAKE_INVALIDATION_HANDLER_H_
#define COMPONENTS_INVALIDATION_IMPL_FAKE_INVALIDATION_HANDLER_H_

#include <map>
#include <set>
#include <string>

#include "components/invalidation/public/invalidation.h"
#include "components/invalidation/public/invalidation_handler.h"
#include "components/invalidation/public/invalidator_state.h"

namespace invalidation {

class FakeInvalidationHandler : public InvalidationHandler {
 public:
  explicit FakeInvalidationHandler(const std::string& owner);
  FakeInvalidationHandler(const FakeInvalidationHandler& other) = delete;
  FakeInvalidationHandler& operator=(const FakeInvalidationHandler& other) =
      delete;
  ~FakeInvalidationHandler() override;

  InvalidatorState GetInvalidatorState() const;
  const std::map<Topic, Invalidation>& GetReceivedInvalidations() const;
  const std::multiset<Topic>& GetSuccessfullySubscribed() const;
  void Clear();
  int GetInvalidationCount() const;

  // InvalidationHandler implementation.
  void OnInvalidatorStateChange(InvalidatorState state) override;
  void OnIncomingInvalidation(const Invalidation& invalidation) override;
  void OnSuccessfullySubscribed(const Topic& topic) override;
  std::string GetOwnerName() const override;
  bool IsPublicTopic(const Topic& topic) const override;

 private:
  InvalidatorState state_ = InvalidatorState::kDisabled;
  std::map<Topic, Invalidation> received_invalidations_;
  int invalidation_count_ = 0;
  std::multiset<Topic> successfully_subscribed_;
  std::string owner_name_;
};

}  // namespace invalidation

#endif  // COMPONENTS_INVALIDATION_IMPL_FAKE_INVALIDATION_HANDLER_H_
