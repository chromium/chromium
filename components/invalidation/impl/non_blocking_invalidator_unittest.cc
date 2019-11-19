// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/non_blocking_invalidator.h"

#include <memory>

#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "components/invalidation/impl/fake_invalidation_handler.h"
#include "components/invalidation/impl/invalidation_state_tracker.h"
#include "components/invalidation/impl/invalidator_test_template.h"
#include "google/cacheinvalidation/types.pb.h"
#include "jingle/glue/network_service_config_test_util.h"
#include "jingle/notifier/base/fake_base_task.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

class NonBlockingInvalidatorTestDelegate {
 public:
  NonBlockingInvalidatorTestDelegate() : io_thread_("IO thread") {}

  ~NonBlockingInvalidatorTestDelegate() {
    DestroyInvalidator();
  }

  void CreateInvalidator(
      const std::string& invalidator_client_id,
      const std::string& initial_state,
      const base::WeakPtr<InvalidationStateTracker>&
          invalidation_state_tracker) {
    DCHECK(!invalidator_);
    base::Thread::Options options;
    options.message_pump_type = base::MessagePumpType::IO;
    io_thread_.StartWithOptions(options);
    net_config_helper_ =
        std::make_unique<jingle_glue::NetworkServiceConfigTestUtil>(
            base::MakeRefCounted<net::TestURLRequestContextGetter>(
                io_thread_.task_runner()));
    notifier::NotifierOptions notifier_options;
    net_config_helper_->FillInNetworkConfig(&notifier_options.network_config);
    notifier_options.network_connection_tracker =
        network::TestNetworkConnectionTracker::GetInstance();
    NetworkChannelCreator network_channel_creator =
        NonBlockingInvalidator::MakePushClientChannelCreator(notifier_options);
    invalidator_.reset(new NonBlockingInvalidator(
        network_channel_creator, invalidator_client_id,
        UnackedInvalidationsMap(), initial_state,
        invalidation_state_tracker.get(), "fake_client_info",
        notifier_options.network_config.task_runner));
  }

  Invalidator* GetInvalidator() {
    return invalidator_.get();
  }

  void DestroyInvalidator() {
    invalidator_.reset();
    net_config_helper_ = nullptr;
    io_thread_.Stop();
    base::RunLoop().RunUntilIdle();
  }

  void WaitForInvalidator() {
    base::RunLoop run_loop;
    ASSERT_TRUE(io_thread_.task_runner()->PostTaskAndReply(
        FROM_HERE, base::DoNothing(), run_loop.QuitClosure()));
    run_loop.Run();
  }

  void TriggerOnInvalidatorStateChange(InvalidatorState state) {
    invalidator_->OnInvalidatorStateChange(state);
  }

  void TriggerOnIncomingInvalidation(
      const ObjectIdInvalidationMap& invalidation_map) {
    invalidator_->OnIncomingInvalidation(invalidation_map);
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::Thread io_thread_;
  std::unique_ptr<jingle_glue::NetworkServiceConfigTestUtil> net_config_helper_;
  std::unique_ptr<NonBlockingInvalidator> invalidator_;
};

INSTANTIATE_TYPED_TEST_SUITE_P(NonBlockingInvalidatorTest,
                               InvalidatorTest,
                               NonBlockingInvalidatorTestDelegate);

}  // namespace syncer
