// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/base/bind_to_task_runner.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Pointee;

namespace chromecast {

namespace {

using Type = int;

constexpr Type kValue = 15;

class Callbacks {
 public:
  virtual void VoidCallback() = 0;
  virtual void ValueCallback(Type) = 0;
  virtual void ConstRefCallback(const Type&) = 0;
  virtual void MoveOnlyCallback(std::unique_ptr<Type>) = 0;
};

class MockCallbacks : public Callbacks {
 public:
  MOCK_METHOD0(VoidCallback, void());
  MOCK_METHOD1(ValueCallback, void(Type));
  MOCK_METHOD1(ConstRefCallback, void(const Type&));
  void MoveOnlyCallback(std::unique_ptr<Type> arg) override {
    DoMoveOnlyCallback(arg.get());
  }
  MOCK_METHOD1(DoMoveOnlyCallback, void(Type*));
};

}  // namespace

class BindToTaskRunnerTest : public ::testing::Test {
 public:
  ~BindToTaskRunnerTest() override { base::RunLoop().RunUntilIdle(); }

  base::test::SingleThreadTaskEnvironment task_environment_;
  MockCallbacks callbacks_;
};

TEST_F(BindToTaskRunnerTest, OnceClosure) {
  base::OnceClosure callback = BindToCurrentThread(
      base::BindOnce(&Callbacks::VoidCallback, base::Unretained(&callbacks_)));
  std::move(callback).Run();
  EXPECT_CALL(callbacks_, VoidCallback());
}

TEST_F(BindToTaskRunnerTest, OnceCallbackWithBoundValue) {
  base::OnceCallback<void()> callback = BindToCurrentThread(base::BindOnce(
      &Callbacks::ValueCallback, base::Unretained(&callbacks_), kValue));
  std::move(callback).Run();
  EXPECT_CALL(callbacks_, ValueCallback(kValue));
}

TEST_F(BindToTaskRunnerTest, OnceCallbackWithUnboundValue) {
  base::OnceCallback<void(Type)> callback = BindToCurrentThread(
      base::BindOnce(&Callbacks::ValueCallback, base::Unretained(&callbacks_)));
  std::move(callback).Run(kValue);
  EXPECT_CALL(callbacks_, ValueCallback(kValue));
}

TEST_F(BindToTaskRunnerTest, OnceCallbackWithBoundConstRef) {
  base::OnceCallback<void()> callback = BindToCurrentThread(base::BindOnce(
      &Callbacks::ConstRefCallback, base::Unretained(&callbacks_), kValue));
  std::move(callback).Run();
  EXPECT_CALL(callbacks_, ConstRefCallback(kValue));
}

TEST_F(BindToTaskRunnerTest, OnceCallbackWithUnboundConstRef) {
  base::OnceCallback<void(const Type&)> callback =
      BindToCurrentThread(base::BindOnce(&Callbacks::ConstRefCallback,
                                         base::Unretained(&callbacks_)));
  std::move(callback).Run(kValue);
  EXPECT_CALL(callbacks_, ConstRefCallback(kValue));
}

TEST_F(BindToTaskRunnerTest, OnceCallbackWithBoundMoveOnly) {
  base::OnceCallback<void()> callback = BindToCurrentThread(base::BindOnce(
      &Callbacks::MoveOnlyCallback, base::Unretained(&callbacks_),
      std::make_unique<Type>(kValue)));
  std::move(callback).Run();
  EXPECT_CALL(callbacks_, DoMoveOnlyCallback(Pointee(kValue)));
}

TEST_F(BindToTaskRunnerTest, OnceCallbackWithUnboundMoveOnly) {
  base::OnceCallback<void(std::unique_ptr<Type>)> callback =
      BindToCurrentThread(base::BindOnce(&Callbacks::MoveOnlyCallback,
                                         base::Unretained(&callbacks_)));
  std::move(callback).Run(std::make_unique<Type>(kValue));
  EXPECT_CALL(callbacks_, DoMoveOnlyCallback(Pointee(kValue)));
}

TEST_F(BindToTaskRunnerTest, RepeatingClosure) {
  base::RepeatingClosure callback = BindToCurrentThread(base::BindRepeating(
      &Callbacks::VoidCallback, base::Unretained(&callbacks_)));
  callback.Run();
  EXPECT_CALL(callbacks_, VoidCallback());
}

TEST_F(BindToTaskRunnerTest, RepeatingCallbackWithBoundValue) {
  base::RepeatingCallback<void()> callback =
      BindToCurrentThread(base::BindRepeating(
          &Callbacks::ValueCallback, base::Unretained(&callbacks_), kValue));
  callback.Run();
  EXPECT_CALL(callbacks_, ValueCallback(kValue));
}

TEST_F(BindToTaskRunnerTest, RepeatingCallbackWithUnboundValue) {
  base::RepeatingCallback<void(Type)> callback =
      BindToCurrentThread(base::BindRepeating(&Callbacks::ValueCallback,
                                              base::Unretained(&callbacks_)));
  callback.Run(kValue);
  EXPECT_CALL(callbacks_, ValueCallback(kValue));
}

TEST_F(BindToTaskRunnerTest, RepeatingCallbackWithBoundConstRef) {
  base::RepeatingCallback<void()> callback =
      BindToCurrentThread(base::BindRepeating(
          &Callbacks::ConstRefCallback, base::Unretained(&callbacks_), kValue));
  callback.Run();
  EXPECT_CALL(callbacks_, ConstRefCallback(kValue));
}

TEST_F(BindToTaskRunnerTest, RepeatingCallbackWithUnboundConstRef) {
  base::RepeatingCallback<void(const Type&)> callback =
      BindToCurrentThread(base::BindRepeating(&Callbacks::ConstRefCallback,
                                              base::Unretained(&callbacks_)));
  callback.Run(kValue);
  EXPECT_CALL(callbacks_, ConstRefCallback(kValue));
}

TEST_F(BindToTaskRunnerTest, RepeatingCallbackWithBoundMoveOnly) {
  base::RepeatingCallback<void()> callback =
      BindToCurrentThread(base::BindRepeating(
          &Callbacks::MoveOnlyCallback, base::Unretained(&callbacks_),
          base::Passed(std::make_unique<Type>(kValue))));
  callback.Run();
  EXPECT_CALL(callbacks_, DoMoveOnlyCallback(Pointee(kValue)));
}

TEST_F(BindToTaskRunnerTest, RepeatingCallbackWithUnboundMoveOnly) {
  base::RepeatingCallback<void(std::unique_ptr<Type>)> callback =
      BindToCurrentThread(base::BindRepeating(&Callbacks::MoveOnlyCallback,
                                              base::Unretained(&callbacks_)));
  callback.Run(std::make_unique<Type>(kValue));
  EXPECT_CALL(callbacks_, DoMoveOnlyCallback(Pointee(kValue)));
}

}  // namespace chromecast
