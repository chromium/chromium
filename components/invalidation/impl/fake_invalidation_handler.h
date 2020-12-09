// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_IMPL_FAKE_INVALIDATION_HANDLER_H_
#define COMPONENTS_INVALIDATION_IMPL_FAKE_INVALIDATION_HANDLER_H_

#include <string>

#include "components/invalidation/public/invalidation_handler.h"
#include "components/invalidation/public/topic_invalidation_map.h"

namespace syncer {

class FakeInvalidationHandler : public InvalidationHandler {
 public:
  FakeInvalidationHandler();
  explicit FakeInvalidationHandler(const std::string& owner);
  FakeInvalidationHandler(const FakeInvalidationHandler& other) = delete;
  FakeInvalidationHandler& operator=(const FakeInvalidationHandler& other) =
      delete;
  ~FakeInvalidationHandler() override;

  InvalidatorState GetInvalidatorState() const;
  const TopicInvalidationMap& GetLastInvalidationMap() const;
  int GetInvalidationCount() const;

  // InvalidationHandler implementation.
  void OnInvalidatorStateChange(InvalidatorState state) override;
  void OnIncomingInvalidation(
      const TopicInvalidationMap& invalidation_map) override;
  std::string GetOwnerName() const override;
  bool IsPublicTopic(const syncer::Topic& topic) const override;

 private:
  InvalidatorState state_;
  TopicInvalidationMap last_invalidation_map_;
  int invalidation_count_;
  std::string owner_name_;
};

}  // namespace syncer

#endif  // COMPONENTS_INVALIDATION_IMPL_FAKE_INVALIDATION_HANDLER_H_
