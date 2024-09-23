// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/util/status.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "components/reporting/proto/synced/status.pb.h"
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

TEST(Status, Empty) {
  Status status;
  EXPECT_EQ(error::OK, status.error_code());
  EXPECT_EQ(error::OK, status.code());
  EXPECT_EQ("OK", status.ToString());
}

TEST(Status, GenericCodes) {
  EXPECT_EQ(error::OK, Status::StatusOK().error_code());
  EXPECT_EQ(error::OK, Status::StatusOK().code());
  EXPECT_EQ("OK", Status::StatusOK().ToString());
}

TEST(Status, OkConstructorIgnoresMessage) {
  Status status(error::OK, "msg");
  EXPECT_TRUE(status.ok());
  EXPECT_EQ("OK", status.ToString());
}

TEST(Status, CheckOK) {
  Status status;
  CHECK_OK(status);
  CHECK_OK(status) << "Failed";
  // DCHECK is ok to use here since we're just verifying functionality of Status
  // to work with all types of checks.
  DCHECK_OK(status) << "Failed";
}

TEST(Status, ErrorMessage) {
  Status status(error::INVALID_ARGUMENT, "");
  EXPECT_FALSE(status.ok());
  EXPECT_EQ("", status.error_message());
  EXPECT_EQ("", status.message());
  EXPECT_EQ("INVALID_ARGUMENT", status.ToString());
  status = Status(error::INVALID_ARGUMENT, "msg");
  EXPECT_FALSE(status.ok());
  EXPECT_EQ("msg", status.error_message());
  EXPECT_EQ("msg", status.message());
  EXPECT_EQ("INVALID_ARGUMENT:msg", status.ToString());
  status = Status(error::OK, "msg");
  EXPECT_TRUE(status.ok());
  EXPECT_EQ("", status.error_message());
  EXPECT_EQ("", status.message());
  EXPECT_EQ("OK", status.ToString());
}

TEST(Status, Copy) {
  Status a(error::UNKNOWN, "message");
  Status b(a);
  EXPECT_EQ(a.ToString(), b.ToString());
}

TEST(Status, Assign) {
  Status a(error::UNKNOWN, "message");
  Status b;
  b = a;
  EXPECT_EQ(a.ToString(), b.ToString());
}

TEST(Status, AssignEmpty) {
  Status a(error::UNKNOWN, "message");
  Status b;
  a = b;
  EXPECT_EQ(std::string("OK"), a.ToString());
  EXPECT_TRUE(b.ok());
  EXPECT_TRUE(a.ok());
}

TEST(Status, EqualsOK) {
  EXPECT_EQ(Status::StatusOK(), Status());
}

TEST(Status, EqualsSame) {
  const Status a = Status(error::CANCELLED, "message");
  const Status b = Status(error::CANCELLED, "message");
  EXPECT_EQ(a, b);
}

TEST(Status, EqualsCopy) {
  const Status a = Status(error::CANCELLED, "message");
  const Status b = a;
  EXPECT_EQ(a, b);
}

TEST(Status, EqualsDifferentCode) {
  const Status a = Status(error::CANCELLED, "message");
  const Status b = Status(error::UNKNOWN, "message");
  EXPECT_NE(a, b);
}

TEST(Status, EqualsDifferentMessage) {
  const Status a = Status(error::CANCELLED, "message");
  const Status b = Status(error::CANCELLED, "another");
  EXPECT_NE(a, b);
}

TEST(Status, SaveOkTo) {
  StatusProto status_proto;
  Status::StatusOK().SaveTo(&status_proto);

  EXPECT_EQ(status_proto.code(), error::OK);
  EXPECT_TRUE(status_proto.error_message().empty())
      << status_proto.error_message();
}

TEST(Status, SaveTo) {
  Status status(error::INVALID_ARGUMENT, "argument error");
  StatusProto status_proto;
  status.SaveTo(&status_proto);

  EXPECT_EQ(status_proto.code(), status.error_code());
  EXPECT_EQ(status_proto.error_message(), status.error_message());
}

TEST(Status, RestoreFromOk) {
  StatusProto status_proto;
  status_proto.set_code(error::OK);
  status_proto.set_error_message("invalid error");

  Status status;
  status.RestoreFrom(status_proto);

  EXPECT_EQ(status.error_code(), status_proto.code());
  // Error messages are ignored for OK status objects.
  EXPECT_TRUE(status.error_message().empty()) << status.error_message();
  EXPECT_TRUE(status.ok());
}

TEST(Status, RestoreFromNonOk) {
  StatusProto status_proto;
  status_proto.set_code(error::INVALID_ARGUMENT);
  status_proto.set_error_message("argument error");

  Status status;
  status.RestoreFrom(status_proto);

  EXPECT_EQ(status.error_code(), status_proto.code());
  EXPECT_EQ(status.error_message(), status_proto.error_message());
}

TEST(Status, ConvertStatusToString) {
  const std::pair<Status, const char*> status_pairs[] = {
      {Status::StatusOK(), "OK"},
      {Status(error::CANCELLED, "Cancelled"), "CANCELLED:Cancelled"},
      {Status(error::UNKNOWN, "Unknown"), "UNKNOWN:Unknown"},
      {Status(error::INVALID_ARGUMENT, "Invalid argument"),
       "INVALID_ARGUMENT:Invalid argument"},
      {Status(error::DEADLINE_EXCEEDED, "Deadline exceeded"),
       "DEADLINE_EXCEEDED:Deadline exceeded"},
      {Status(error::NOT_FOUND, "Not found"), "NOT_FOUND:Not found"},
      {Status(error::ALREADY_EXISTS, "Already exists"),
       "ALREADY_EXISTS:Already exists"},
      {Status(error::PERMISSION_DENIED, "Permission denied"),
       "PERMISSION_DENIED:Permission denied"},
      {Status(error::UNAUTHENTICATED, "Unathenticated"),
       "UNAUTHENTICATED:Unathenticated"},
      {Status(error::RESOURCE_EXHAUSTED, "Resourse exhausted"),
       "RESOURCE_EXHAUSTED:Resourse exhausted"},
      {Status(error::FAILED_PRECONDITION, "Failed precondition"),
       "FAILED_PRECONDITION:Failed precondition"},
      {Status(error::ABORTED, "Aborted"), "ABORTED:Aborted"},
      {Status(error::OUT_OF_RANGE, "Out of range"),
       "OUT_OF_RANGE:Out of range"},
      {Status(error::UNIMPLEMENTED, "Unimplemented"),
       "UNIMPLEMENTED:Unimplemented"},
      {Status(error::INTERNAL, "Internal"), "INTERNAL:Internal"},
      {Status(error::UNAVAILABLE, "Unavailable"), "UNAVAILABLE:Unavailable"},
      {Status(error::DATA_LOSS, "Data loss"), "DATA_LOSS:Data loss"},
  };
  for (const auto& p : status_pairs) {
    LOG(INFO) << p.first;
    EXPECT_THAT(p.first.ToString(), StrEq(p.second));
  }
}

TEST(Status, CallbackAfterDeletion) {
  base::test::TaskEnvironment test_env{};
  class Handler {
   public:
    Handler() = default;
    Handler(const Handler&) = delete;
    Handler& operator=(const Handler&) = delete;

    void Handle(base::OnceCallback<void(Status)> done_cb) {
      std::move(done_cb).Run(Status::StatusOK());
    }

    base::WeakPtr<Handler> GetWeakPtr() {
      return weak_ptr_factory_.GetWeakPtr();
    }

   private:
    base::WeakPtrFactory<Handler> weak_ptr_factory_{this};
  };

  // Create seq task runner as required by weak pointers.
  auto task_runner = base::ThreadPool::CreateSequencedTaskRunner({});

  test::TestEvent<Status> cb_dead;
  base::OnceClosure async_cb_dead;

  {
    std::unique_ptr<Handler, base::OnTaskRunnerDeleter> handler{
        new Handler, base::OnTaskRunnerDeleter(task_runner)};
    test::TestEvent<Status> cb_alive;
    task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(&Handler::Handle, handler->GetWeakPtr(), cb_alive.cb()));
    EXPECT_OK(cb_alive.result());
    async_cb_dead = base::BindOnce(
        &Handler::Handle, handler->GetWeakPtr(),
        Scoped<Status>(cb_dead.cb(),
                       Status(error::UNAVAILABLE, "Handler destructed")));
  }

  // Out of scope: run after Handler has been deleted (on sequence!).
  task_runner->PostTask(FROM_HERE, std::move(async_cb_dead));

  // Make sure callback is still invoked, but with error.
  EXPECT_THAT(
      cb_dead.result(),
      AllOf(Property(&Status::error_code, Eq(error::UNAVAILABLE)),
            Property(&Status::error_message, StrEq("Handler destructed"))));
}
}  // namespace
}  // namespace reporting
