// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/invalidation_notifier.h"

#include <memory>
#include <utility>

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/invalidation/impl/fake_invalidation_handler.h"
#include "components/invalidation/impl/fake_invalidation_state_tracker.h"
#include "components/invalidation/impl/invalidation_state_tracker.h"
#include "components/invalidation/impl/invalidator_test_template.h"
#include "components/invalidation/impl/push_client_channel.h"
#include "jingle/notifier/base/fake_base_task.h"
#include "jingle/notifier/base/notifier_options.h"
#include "jingle/notifier/listener/fake_push_client.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

class InvalidationNotifierTestDelegate {
 public:
  InvalidationNotifierTestDelegate() {}

  ~InvalidationNotifierTestDelegate() {
    DestroyInvalidator();
  }

  void CreateInvalidator(
      const std::string& invalidator_client_id,
      const std::string& initial_state,
      const base::WeakPtr<InvalidationStateTracker>&
          invalidation_state_tracker) {
    DCHECK(!invalidator_);
    std::unique_ptr<notifier::PushClient> push_client(
        new notifier::FakePushClient());
    std::unique_ptr<SyncNetworkChannel> network_channel(
        new PushClientChannel(std::move(push_client)));
    invalidator_.reset(new InvalidationNotifier(
        std::move(network_channel), invalidator_client_id,
        UnackedInvalidationsMap(), initial_state, invalidation_state_tracker,
        base::ThreadTaskRunnerHandle::Get(), "fake_client_info"));
  }

  Invalidator* GetInvalidator() {
    return invalidator_.get();
  }

  void DestroyInvalidator() {
    // Stopping the invalidation notifier stops its scheduler, which deletes
    // any pending tasks without running them.  Some tasks "run and delete"
    // another task, so they must be run in order to avoid leaking the inner
    // task.  Stopping does not schedule any tasks, so it's both necessary and
    // sufficient to drain the task queue before stopping the notifier.
    base::RunLoop().RunUntilIdle();
    invalidator_.reset();
  }

  void WaitForInvalidator() { base::RunLoop().RunUntilIdle(); }

  void TriggerOnInvalidatorStateChange(InvalidatorState state) {
    invalidator_->OnInvalidatorStateChange(state);
  }

  void TriggerOnIncomingInvalidation(
      const ObjectIdInvalidationMap& invalidation_map) {
    invalidator_->OnInvalidate(invalidation_map);
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<InvalidationNotifier> invalidator_;
};

INSTANTIATE_TYPED_TEST_SUITE_P(InvalidationNotifierTest,
                               InvalidatorTest,
                               InvalidationNotifierTestDelegate);

}  // namespace

}  // namespace syncer
