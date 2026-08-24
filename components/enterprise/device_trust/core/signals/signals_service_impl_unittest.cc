// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/device_trust/core/signals/signals_service_impl.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "components/device_signals/core/common/signals_constants.h"
#include "components/enterprise/device_trust/core/signals/decorators/common/mock_signals_decorator.h"
#include "components/enterprise/device_trust/core/signals/decorators/common/signals_decorator.h"
#include "components/enterprise/device_trust/core/signals/mock_signals_filterer.h"
#include "components/enterprise/device_trust/core/signals/signals_filterer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {

using test::MockSignalsDecorator;
using test::MockSignalsFilterer;
using ::testing::_;

namespace {

constexpr char kLatencyHistogram[] =
    "Enterprise.DeviceTrust.SignalsDecorator.Latency.Full";

}  // namespace

class SignalsServiceImplTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(SignalsServiceImplTest, CollectSignals_CallsAllDecorators) {
  base::HistogramTester histogram_tester;
  std::string fake_display_name = "fake_display_name";
  std::unique_ptr<MockSignalsDecorator> first_decorator =
      std::make_unique<MockSignalsDecorator>();
  EXPECT_CALL(*first_decorator.get(), Decorate(_, _))
      .WillOnce([&fake_display_name](base::DictValue& signals,
                                     base::OnceClosure done_closure) {
        signals.Set(device_signals::names::kDisplayName, fake_display_name);
        std::move(done_closure).Run();
      });

  std::string fake_allow_lock_screen = "false";
  std::unique_ptr<MockSignalsDecorator> second_decorator =
      std::make_unique<MockSignalsDecorator>();
  EXPECT_CALL(*second_decorator.get(), Decorate(_, _))
      .WillOnce([&fake_allow_lock_screen](base::DictValue& signals,
                                          base::OnceClosure done_closure) {
        signals.Set(device_signals::names::kAllowScreenLock,
                    fake_allow_lock_screen);
        std::move(done_closure).Run();
      });

  std::vector<std::unique_ptr<SignalsDecorator>> decorators;
  decorators.push_back(std::move(first_decorator));
  decorators.push_back(std::move(second_decorator));

  std::unique_ptr<MockSignalsFilterer> signals_filterer =
      std::make_unique<MockSignalsFilterer>();
  EXPECT_CALL(*signals_filterer.get(), Filter(_))
      .WillOnce([](base::DictValue& signals) { return; });

  SignalsServiceImpl service(std::move(decorators),
                             std::move(signals_filterer));

  bool callback_called = false;
  base::RunLoop run_loop;
  auto callback =
      base::BindLambdaForTesting([&, quit_closure = run_loop.QuitClosure()](
                                     const base::DictValue signals) {
        EXPECT_EQ(
            signals.FindString(device_signals::names::kDisplayName)->c_str(),
            fake_display_name);
        EXPECT_EQ(signals.FindString(device_signals::names::kAllowScreenLock)
                      ->c_str(),
                  fake_allow_lock_screen);
        callback_called = true;
        quit_closure.Run();
      });

  service.CollectSignals(std::move(callback));

  EXPECT_FALSE(callback_called);
  run_loop.Run();
  EXPECT_TRUE(callback_called);
  histogram_tester.ExpectTotalCount(kLatencyHistogram, 1);
}

TEST_F(SignalsServiceImplTest,
       CollectSignals_EmptyDecoratorsCompletesAsynchronously) {
  std::vector<std::unique_ptr<SignalsDecorator>> decorators;
  auto filterer = std::make_unique<MockSignalsFilterer>();
  EXPECT_CALL(*filterer.get(), Filter(_));

  auto service = std::make_unique<SignalsServiceImpl>(std::move(decorators),
                                                      std::move(filterer));

  bool callback_called = false;
  base::RunLoop run_loop;

  service->CollectSignals(base::BindLambdaForTesting(
      [&callback_called,
       quit_closure = run_loop.QuitClosure()](base::DictValue) {
        callback_called = true;
        quit_closure.Run();
      }));

  EXPECT_FALSE(callback_called);
  run_loop.Run();
  EXPECT_TRUE(callback_called);
}

TEST_F(SignalsServiceImplTest,
       CollectSignals_SynchronousDecoratorCanDeleteService) {
  auto decorator = std::make_unique<MockSignalsDecorator>();
  EXPECT_CALL(*decorator.get(), Decorate(_, _))
      .WillOnce([](base::DictValue& signals, base::OnceClosure done_closure) {
        std::move(done_closure).Run();
      });

  std::vector<std::unique_ptr<SignalsDecorator>> decorators;
  decorators.push_back(std::move(decorator));

  auto filterer = std::make_unique<MockSignalsFilterer>();
  EXPECT_CALL(*filterer.get(), Filter(_));

  auto service = std::make_unique<SignalsServiceImpl>(std::move(decorators),
                                                      std::move(filterer));

  base::RunLoop run_loop;
  service->CollectSignals(base::BindLambdaForTesting(
      [&service, quit_closure = run_loop.QuitClosure()](base::DictValue) {
        service.reset();
        quit_closure.Run();
      }));

  EXPECT_NE(service, nullptr);
  run_loop.Run();
  EXPECT_EQ(service, nullptr);
}

// Verifies that mix of synchronous and asynchronous decorators still yields
// a single asynchronous callback, once every decorator has completed.
TEST_F(SignalsServiceImplTest, CollectSignals_MixedDecoratorsCompletesOnce) {
  auto sync_decorator = std::make_unique<MockSignalsDecorator>();
  EXPECT_CALL(*sync_decorator.get(), Decorate(_, _))
      .WillOnce([](base::DictValue& signals, base::OnceClosure done_closure) {
        signals.Set(device_signals::names::kDisplayName, "sync");
        std::move(done_closure).Run();
      });

  base::OnceClosure saved_async_closure;
  auto async_decorator = std::make_unique<MockSignalsDecorator>();
  EXPECT_CALL(*async_decorator.get(), Decorate(_, _))
      .WillOnce([&saved_async_closure](base::DictValue& signals,
                                       base::OnceClosure done_closure) {
        signals.Set(device_signals::names::kAllowScreenLock, "async");
        saved_async_closure = std::move(done_closure);
      });

  std::vector<std::unique_ptr<SignalsDecorator>> decorators;
  decorators.push_back(std::move(sync_decorator));
  decorators.push_back(std::move(async_decorator));

  auto filterer = std::make_unique<MockSignalsFilterer>();
  EXPECT_CALL(*filterer.get(), Filter(_));

  auto service = std::make_unique<SignalsServiceImpl>(std::move(decorators),
                                                      std::move(filterer));

  int callback_count = 0;
  base::RunLoop run_loop;
  service->CollectSignals(base::BindLambdaForTesting(
      [&callback_count,
       quit_closure = run_loop.QuitClosure()](base::DictValue signals) {
        EXPECT_EQ(*signals.FindString(device_signals::names::kDisplayName),
                  "sync");
        EXPECT_EQ(*signals.FindString(device_signals::names::kAllowScreenLock),
                  "async");
        ++callback_count;
        quit_closure.Run();
      }));

  // The outstanding decorator keeps the barrier from completing.
  base::RunLoop before_completion_run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, before_completion_run_loop.QuitClosure());
  before_completion_run_loop.Run();
  EXPECT_EQ(callback_count, 0);

  std::move(saved_async_closure).Run();
  EXPECT_EQ(callback_count, 0);

  run_loop.Run();
  EXPECT_EQ(callback_count, 1);
}

// Verifies that destroying the service while a decorator is still outstanding
// drops the callback.
TEST_F(SignalsServiceImplTest,
       CollectSignals_ServiceDestroyedBeforeCompletionDropsCallback) {
  base::RunLoop run_loop;
  base::OnceClosure saved_async_closure;
  auto async_decorator = std::make_unique<MockSignalsDecorator>();
  EXPECT_CALL(*async_decorator.get(), Decorate(_, _))
      .WillOnce([&saved_async_closure](base::DictValue& signals,
                                       base::OnceClosure done_closure) {
        saved_async_closure = std::move(done_closure);
      });

  std::vector<std::unique_ptr<SignalsDecorator>> decorators;
  decorators.push_back(std::move(async_decorator));

  auto filterer = std::make_unique<MockSignalsFilterer>();
  EXPECT_CALL(*filterer.get(), Filter(_)).Times(0);

  auto service = std::make_unique<SignalsServiceImpl>(std::move(decorators),
                                                      std::move(filterer));

  bool callback_called = false;
  service->CollectSignals(base::BindLambdaForTesting(
      [&callback_called](base::DictValue signals) { callback_called = true; }));

  service.reset();
  std::move(saved_async_closure).Run();
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_FALSE(callback_called);
}

}  // namespace enterprise_connectors
