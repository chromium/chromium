// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_IMPL_FAKE_INVALIDATION_HANDLER_H_
#define COMPONENTS_INVALIDATION_IMPL_FAKE_INVALIDATION_HANDLER_H_

#include <string>

#include "components/invalidation/public/invalidation_handler.h"
#include "components/invalidation/public/topic_invalidation_map.h"

namespace invalidation {

class FakeInvalidationHandler : public InvalidationHandler {
 public:
  explicit FakeInvalidationHandler(const std::string& owner);
  FakeInvalidationHandler(const FakeInvalidationHandler& other) = delete;
  FakeInvalidationHandler& operator=(const FakeInvalidationHandler& other) =
      delete;
  ~FakeInvalidationHandler() override;

  InvalidatorState GetInvalidatorState() const;
  const TopicInvalidationMap& GetLastInvalidationMap() const;
  int GetInvalidationCount() const;
  const std::string& GetInvalidatorClientId() const;

  // InvalidationHandler implementation.
  void OnInvalidatorStateChange(InvalidatorState state) override;
  void OnIncomingInvalidation(
      const TopicInvalidationMap& invalidation_map) override;
  std::string GetOwnerName() const override;
  bool IsPublicTopic(const Topic& topic) const override;
  void OnInvalidatorClientIdChange(const std::string& client_id) override;

 private:
  InvalidatorState state_ = DEFAULT_INVALIDATION_ERROR;
  TopicInvalidationMap last_invalidation_map_;
  int invalidation_count_ = 0;
  std::string owner_name_;
  std::string client_id_;
};

}  // namespace invalidation

#endif  // COMPONENTS_INVALIDATION_IMPL_FAKE_INVALIDATION_HANDLER_H_
