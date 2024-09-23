// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/util/statusor.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "base/types/expected.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/status_macros.h"
#include "components/reporting/util/test_support_callbacks.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AllOf;
using ::testing::Eq;
using ::testing::Property;
using ::testing::StrEq;

namespace reporting {
namespace {

TEST(StatusOr, MoveConstructFromAndExtractToStatusImplicitly) {
  Status status(error::INTERNAL, "internal error");
  base::unexpected<Status> unexpected_status(status);
  StatusOr<int> status_or(std::move(unexpected_status));
  Status extracted_status{std::move(status_or).error()};
  EXPECT_EQ(status, extracted_status);
}

TEST(StatusOr, CopyConstructFromAndExtractToStatusImplicitly) {
  Status status(error::INTERNAL, "internal error");
  base::unexpected<Status> unexpected_status(status);
  StatusOr<int> status_or(unexpected_status);
  Status extracted_status{status_or.error()};
  EXPECT_EQ(status, extracted_status);
}

TEST(StatusOr, CallbackAfterDeletion) {
  base::test::TaskEnvironment test_env{};
  class Handler {
   public:
    using Result = StatusOr<int64_t>;

    Handler() = default;
    Handler(const Handler&) = delete;
    Handler& operator=(const Handler&) = delete;

    void Handle(base::OnceCallback<void(Result)> done_cb) {
      std::move(done_cb).Run(12345L);
    }

    base::WeakPtr<Handler> GetWeakPtr() {
      return weak_ptr_factory_.GetWeakPtr();
    }

   private:
    base::WeakPtrFactory<Handler> weak_ptr_factory_{this};
  };

  // Create seq task runner as required by weak pointers.
  auto task_runner = base::ThreadPool::CreateSequencedTaskRunner({});

  test::TestEvent<Handler::Result> cb_dead;
  base::OnceClosure async_cb_dead;

  {
    std::unique_ptr<Handler, base::OnTaskRunnerDeleter> handler{
        new Handler, base::OnTaskRunnerDeleter(task_runner)};
    test::TestEvent<Handler::Result> cb_alive;
    task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(&Handler::Handle, handler->GetWeakPtr(), cb_alive.cb()));
    const auto result = cb_alive.result();
    ASSERT_TRUE(result.has_value()) << result.error();
    EXPECT_THAT(result.value(), Eq(12345L));
    async_cb_dead = base::BindOnce(
        &Handler::Handle, handler->GetWeakPtr(),
        Scoped<Handler::Result>(
            cb_dead.cb(), base::unexpected(Status(error::UNAVAILABLE,
                                                  "Handler destructed"))));
  }

  // Out of scope: run after Handler has been deleted (on sequence!).
  task_runner->PostTask(FROM_HERE, std::move(async_cb_dead));

  // Make sure callback is still invoked, but with error.
  const auto result = cb_dead.result();
  EXPECT_FALSE(result.has_value());
  EXPECT_THAT(
      result.error(),
      AllOf(Property(&Status::error_code, Eq(error::UNAVAILABLE)),
            Property(&Status::error_message, StrEq("Handler destructed"))));
}
}  // namespace
}  // namespace reporting
