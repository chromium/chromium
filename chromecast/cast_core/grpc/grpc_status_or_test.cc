// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/grpc/grpc_status_or.h"

#include "chromecast/cast_core/grpc/status_matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cast {
namespace utils {
namespace {

using ::cast::test::StatusIs;

static_assert(test::internal::has_status_api<GrpcStatusOr<int>>::value,
              "GrpcStatusOr::status() API must exist");
static_assert(!test::internal::has_status_api<grpc::Status>::value,
              "grps::Status::status() API must not exist");
static_assert(!test::internal::has_code_api<grpc::Status>::value,
              "grpc::Status uses error_* APIs");
static_assert(test::internal::has_error_code_api<grpc::Status>::value,
              "grpc::Status uses error_* APIs");

TEST(GrpcStatusOrTest, DefaultConstructor) {
  GrpcStatusOr<int> status_or;
  EXPECT_THAT(status_or, StatusIs(grpc::StatusCode::UNKNOWN));
}

TEST(GrpcStatusOrTest, ConstructorWithValue) {
  GrpcStatusOr<int> status_or(1);
  CU_EXPECT_OK(status_or);
  EXPECT_EQ(*status_or, 1);
}

TEST(GrpcStatusOrTest, ConstructorStatusCode) {
  GrpcStatusOr<int> status_or(grpc::StatusCode::ABORTED);
  EXPECT_THAT(status_or, StatusIs(grpc::StatusCode::ABORTED));
}

TEST(GrpcStatusOrTest, ConstructorStatusCodeAndErrorMessage) {
  GrpcStatusOr<int> status_or(grpc::StatusCode::NOT_FOUND, "error");
  EXPECT_THAT(status_or, StatusIs(grpc::StatusCode::NOT_FOUND, "error"));
}

struct MoveableOnly {
  explicit MoveableOnly(int v) : value(v) {}
  MoveableOnly(MoveableOnly&&) = default;
  MoveableOnly& operator=(MoveableOnly&&) { return *this; }
  MoveableOnly(const MoveableOnly& rhs) = delete;
  MoveableOnly& operator=(const MoveableOnly& rhs) = delete;

  int value;
};

int GetValueFromMoveableOnly(MoveableOnly&& mo) {
  return mo.value;
}

TEST(GrpcStatusOrTest, MoveValueOperator) {
  GrpcStatusOr<MoveableOnly> status_or(MoveableOnly(1));
  CU_EXPECT_OK(status_or);
  MoveableOnly res = std::move(status_or).value();
  EXPECT_EQ(res.value, 1);
  EXPECT_THAT(status_or, StatusIs(grpc::StatusCode::UNKNOWN));

  status_or.emplace(MoveableOnly(10));
  EXPECT_EQ(GetValueFromMoveableOnly(std::move(status_or).value()), 10);
  EXPECT_THAT(status_or, StatusIs(grpc::StatusCode::UNKNOWN));
}

TEST(GrpcStatusOrTest, MoveStatusOperator) {
  GrpcStatusOr<int> status_or(1);
  CU_EXPECT_OK(status_or);
  int value = std::move(status_or).value();
  EXPECT_THAT(status_or, StatusIs(grpc::StatusCode::UNKNOWN));
  EXPECT_EQ(value, 1);
}

TEST(GrpcStatusOrTest, Emplace) {
  GrpcStatusOr<int> status_or;
  status_or.emplace(1);
  CU_EXPECT_OK(status_or);
  EXPECT_EQ(*status_or, 1);
}

TEST(GrpcStatusOrTest, Accessor) {
  GrpcStatusOr<std::string> status_or("12345");
  EXPECT_EQ(status_or->size(), 5U);
  EXPECT_EQ(*status_or, "12345");
  status_or.emplace("567");
  EXPECT_EQ(*status_or, "567");
  status_or->append("89");
  EXPECT_EQ(*status_or, "56789");
  status_or.emplace("0");
  EXPECT_EQ(*status_or, "0");
}

TEST(GrpcStatusOrTest, StreamOperator) {
  GrpcStatusOr<int> status_or(1);
  EXPECT_EQ(status_or.ToString(), "[status=OK: ]");
}

TEST(GrpcStatusOrTest, StreamOperatorWithStatus) {
  GrpcStatusOr<int> status_or(grpc::StatusCode::CANCELLED, "method cancelled");
  EXPECT_EQ(status_or.ToString(), "[status=CANCELLED: method cancelled]");
}

}  // namespace
}  // namespace utils
}  // namespace cast
