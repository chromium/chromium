// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include "components/reporting/client/report_queue_provider.h"

#include "base/bind.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/reporting/client/mock_report_queue.h"
#include "components/reporting/client/report_queue.h"
#include "components/reporting/client/report_queue_configuration.h"
#include "components/reporting/proto/record.pb.h"
#include "components/reporting/storage/test_storage_module.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/status_macros.h"
#include "components/reporting/util/statusor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::StrEq;
using ::testing::WithArg;

namespace reporting {
namespace {

// Usage(in tests only):
//   TestEvent<ResType> e;
//   ... Do some async work passing e.cb() as a completion callback of
//       base::OnceCallback<void(ResType* res)> type which also may
//       perform some other action specified by |done| callback provided
//       by the caller.
//   ... = e.result();  // Will wait for e.cb() to be called and return
//   the
//       collected result.
//
template <typename ResType>
class TestEvent {
 public:
  TestEvent() : run_loop_(std::make_unique<base::RunLoop>()) {}
  ~TestEvent() { EXPECT_FALSE(run_loop_->running()) << "Not responded"; }
  TestEvent(const TestEvent& other) = delete;
  TestEvent& operator=(const TestEvent& other) = delete;
  ResType result() {
    run_loop_->Run();
    return std::forward<ResType>(result_);
  }

  // Completion callback to hand over to the processing method.
  base::OnceCallback<void(ResType res)> cb() {
    return base::BindOnce(
        [](base::RunLoop* run_loop, ResType* result, ResType res) {
          *result = std::forward<ResType>(res);
          run_loop->Quit();
        },
        base::Unretained(run_loop_.get()), base::Unretained(&result_));
  }

 private:
  std::unique_ptr<base::RunLoop> run_loop_;
  ResType result_;
};

class MockReportQueueProvider : public ReportQueueProvider {
 public:
  InitializingContext* InstantiateInitializingContext(
      InitializingContext::UpdateConfigurationCallback update_config_cb,
      InitCompleteCallback init_complete_cb,
      scoped_refptr<InitializationStateTracker> init_state_tracker) override {
    return new MockInitializingContext(std::move(update_config_cb),
                                       std::move(init_complete_cb),
                                       init_state_tracker, this);
  }

  StatusOr<std::unique_ptr<ReportQueue>> CreateNewQueue(
      std::unique_ptr<ReportQueueConfiguration> config) override {
    return std::make_unique<MockReportQueue>();
  }

  void Reset() { storage_.reset(); }

 private:
  // Mock initialization class.
  class MockInitializingContext
      : public ReportQueueProvider::InitializingContext {
   public:
    MockInitializingContext(
        UpdateConfigurationCallback update_config_cb,
        InitCompleteCallback init_complete_cb,
        scoped_refptr<InitializationStateTracker> init_state_tracker,
        MockReportQueueProvider* provider)
        : InitializingContext(std::move(update_config_cb),
                              std::move(init_complete_cb),
                              init_state_tracker),
          provider_(provider) {
      DCHECK(provider_ != nullptr);
    }

   private:
    ~MockInitializingContext() override = default;

    void OnStart() override {
      // Create storage.
      storage_ = base::MakeRefCounted<test::TestStorageModule>();
      // Hand it over to the completion.
      base::ThreadPool::PostTask(
          FROM_HERE, {base::TaskPriority::BEST_EFFORT},
          base::BindOnce(&InitializingContext::Complete, base::Unretained(this),
                         Status::StatusOK()));
    }

    void OnCompleted() override { provider_->storage_ = std::move(storage_); }

    scoped_refptr<StorageModuleInterface> storage_;
    MockReportQueueProvider* const provider_;
  };

  scoped_refptr<StorageModuleInterface> storage_;
};

class ReportQueueProviderTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Enable reporting.
    scoped_feature_list_.InitAndEnableFeature(
        reporting::ReportQueueProvider::kEncryptedReportingPipeline);
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  base::test::ScopedFeatureList scoped_feature_list_;
  const std::string dm_token_ = "TOKEN";
  const Destination destination_ = Destination::UPLOAD_EVENTS;
  ReportQueueConfiguration::PolicyCheckCallback policy_checker_callback_ =
      base::BindRepeating([]() { return Status::StatusOK(); });
};

TEST_F(ReportQueueProviderTest, CreateAndGetQueue) {
  static constexpr char kTestMessage[] = "TEST MESSAGE";
  // Create configuration.
  auto config_result = ReportQueueConfiguration::Create(
      dm_token_, destination_, policy_checker_callback_);
  ASSERT_OK(config_result);
  // Use it to asynchronously create ReportingQueue and then asynchronously
  // send the message.
  TestEvent<Status> e;
  base::ThreadPool::PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::StringPiece data, ReportQueue::EnqueueCallback done_cb,
             std::unique_ptr<ReportQueueConfiguration> config) {
            // Asynchronously create ReportingQueue.
            base::OnceCallback<void(StatusOr<std::unique_ptr<ReportQueue>>)>
                queue_cb = base::BindOnce(
                    [](base::StringPiece data,
                       reporting::ReportQueue::EnqueueCallback done_cb,
                       reporting::StatusOr<std::unique_ptr<
                           reporting::ReportQueue>> report_queue_result) {
                      // Bail out if queue failed to create.
                      if (!report_queue_result.ok()) {
                        std::move(done_cb).Run(report_queue_result.status());
                        return;
                      }
                      // Queue created successfully, enqueue the message.
                      EXPECT_CALL(*static_cast<MockReportQueue*>(
                                      report_queue_result.ValueOrDie().get()),
                                  AddRecord(StrEq(data), Eq(FAST_BATCH), _))
                          .WillOnce(WithArg<2>(
                              Invoke([](ReportQueue::EnqueueCallback cb) {
                                std::move(cb).Run(Status::StatusOK());
                              })));
                      report_queue_result.ValueOrDie()->Enqueue(
                          data, FAST_BATCH, std::move(done_cb));
                    },
                    std::string(data), std::move(done_cb));
            reporting::ReportQueueProvider::CreateQueue(std::move(config),
                                                        std::move(queue_cb));
          },
          kTestMessage, e.cb(), std::move(config_result.ValueOrDie())));
  const auto res = e.result();
  EXPECT_OK(res) << res;
  static_cast<MockReportQueueProvider*>(MockReportQueueProvider::GetInstance())
      ->Reset();
}

}  // namespace

// Implementation of the mock report provider for this test.
ReportQueueProvider* ReportQueueProvider::GetInstance() {
  static base::NoDestructor<MockReportQueueProvider> provider;
  return provider.get();
}

}  // namespace reporting