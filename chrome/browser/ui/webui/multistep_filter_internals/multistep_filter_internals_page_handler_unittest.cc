// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/multistep_filter_internals/multistep_filter_internals_page_handler.h"

#include <memory>
#include <string>
#include <vector>

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/webui/multistep_filter_internals/multistep_filter_internals.mojom.h"
#include "components/multistep_filter/core/logging/log_entry.h"
#include "components/multistep_filter/core/logging/multistep_filter_log_router_impl.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace multistep_filter_internals {
namespace {

class MockPage : public mojom::Page {
 public:
  MockPage() = default;
  ~MockPage() override = default;

  mojo::PendingRemote<mojom::Page> BindAndGetRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  MOCK_METHOD(void,
              OnLogEntryAdded,
              (multistep_filter_internals::mojom::LogEntryPtr entry),
              (override));

 private:
  mojo::Receiver<mojom::Page> receiver_{this};
};

class MultistepFilterInternalsPageHandlerTest : public testing::Test {
 public:
  MultistepFilterInternalsPageHandlerTest() = default;

  static constexpr int64_t kTestNavigation1 = 1;
  static constexpr int64_t kTestNavigation2 = 2;
  static constexpr int64_t kTestNavigation3 = 3;
  static constexpr int64_t kTestNavigation4 = 4;

 protected:
  base::test::TaskEnvironment task_environment_;
  multistep_filter::MultistepFilterLogRouterImpl log_router_;
  testing::NiceMock<MockPage> page_;
  mojo::Remote<mojom::PageHandler> handler_remote_;
  std::unique_ptr<MultistepFilterInternalsPageHandler> handler_;
};

TEST_F(MultistepFilterInternalsPageHandlerTest, ForwardsLogEntries) {
  handler_ = std::make_unique<MultistepFilterInternalsPageHandler>(
      handler_remote_.BindNewPipeAndPassReceiver(), page_.BindAndGetRemote(),
      &log_router_);

  multistep_filter::LogEntry entry(
      kTestNavigation1, multistep_filter::LogEventType::kUrlEligibilityCheck,
      "example.com");

  multistep_filter_internals::mojom::LogEntryPtr captured_val;
  base::RunLoop run_loop;
  EXPECT_CALL(page_, OnLogEntryAdded(testing::_))
      .WillOnce([&](multistep_filter_internals::mojom::LogEntryPtr val) {
        captured_val = std::move(val);
        run_loop.Quit();
      });

  log_router_.RouteLogMessage(std::move(entry));

  run_loop.Run();

  ASSERT_TRUE(captured_val);
  EXPECT_EQ(captured_val->navigation_id, kTestNavigation1);
  EXPECT_EQ(captured_val->event_type, "Url Eligibility Check");
}

TEST_F(MultistepFilterInternalsPageHandlerTest, GetBufferedLogs_NullRouter) {
  handler_ = std::make_unique<MultistepFilterInternalsPageHandler>(
      handler_remote_.BindNewPipeAndPassReceiver(), page_.BindAndGetRemote(),
      nullptr);

  base::test::TestFuture<
      std::vector<multistep_filter_internals::mojom::LogEntryPtr>>
      future;
  handler_remote_->GetBufferedLogs(future.GetCallback());

  std::vector<multistep_filter_internals::mojom::LogEntryPtr> logs =
      future.Take();
  EXPECT_TRUE(logs.empty());
}

TEST_F(MultistepFilterInternalsPageHandlerTest, LogRouterShutdown) {
  handler_ = std::make_unique<MultistepFilterInternalsPageHandler>(
      handler_remote_.BindNewPipeAndPassReceiver(), page_.BindAndGetRemote(),
      &log_router_);

  // Simulate the log router shutting down.
  handler_->OnLogRouterShutdown();

  base::test::TestFuture<
      std::vector<multistep_filter_internals::mojom::LogEntryPtr>>
      future;
  handler_remote_->GetBufferedLogs(future.GetCallback());

  std::vector<multistep_filter_internals::mojom::LogEntryPtr> logs =
      future.Take();
  EXPECT_TRUE(logs.empty());
}

TEST_F(MultistepFilterInternalsPageHandlerTest, GetBufferedLogs_Empty) {
  handler_ = std::make_unique<MultistepFilterInternalsPageHandler>(
      handler_remote_.BindNewPipeAndPassReceiver(), page_.BindAndGetRemote(),
      &log_router_);

  base::test::TestFuture<
      std::vector<multistep_filter_internals::mojom::LogEntryPtr>>
      future;
  handler_remote_->GetBufferedLogs(future.GetCallback());

  std::vector<multistep_filter_internals::mojom::LogEntryPtr> logs =
      future.Take();
  EXPECT_TRUE(logs.empty());
}

TEST_F(MultistepFilterInternalsPageHandlerTest, GetBufferedLogs_Single) {
  handler_ = std::make_unique<MultistepFilterInternalsPageHandler>(
      handler_remote_.BindNewPipeAndPassReceiver(), page_.BindAndGetRemote(),
      &log_router_);

  multistep_filter::LogEntry entry(
      kTestNavigation2, multistep_filter::LogEventType::kUrlEligibilityCheck,
      "example.com");
  entry.details.Set("string_key", "string_val");
  entry.details.Set("bool_key", true);

  log_router_.RouteLogMessage(std::move(entry));

  base::test::TestFuture<
      std::vector<multistep_filter_internals::mojom::LogEntryPtr>>
      future;
  handler_remote_->GetBufferedLogs(future.GetCallback());

  std::vector<multistep_filter_internals::mojom::LogEntryPtr> logs =
      future.Take();
  ASSERT_EQ(logs.size(), 1u);
  ASSERT_TRUE(logs[0]);

  EXPECT_EQ(logs[0]->navigation_id, kTestNavigation2);
  EXPECT_EQ(logs[0]->event_type, "Url Eligibility Check");
  EXPECT_EQ(logs[0]->details, "bool_key: true, string_key: string_val");
}

TEST_F(MultistepFilterInternalsPageHandlerTest, GetBufferedLogs_Multiple) {
  handler_ = std::make_unique<MultistepFilterInternalsPageHandler>(
      handler_remote_.BindNewPipeAndPassReceiver(), page_.BindAndGetRemote(),
      &log_router_);

  multistep_filter::LogEntry entry1(
      kTestNavigation3, multistep_filter::LogEventType::kNavigationStarted,
      "example1.com");
  log_router_.RouteLogMessage(std::move(entry1));

  multistep_filter::LogEntry entry2(
      kTestNavigation4, multistep_filter::LogEventType::kUrlEligibilityCheck,
      "example2.com");
  log_router_.RouteLogMessage(std::move(entry2));

  base::test::TestFuture<
      std::vector<multistep_filter_internals::mojom::LogEntryPtr>>
      future;
  handler_remote_->GetBufferedLogs(future.GetCallback());

  std::vector<multistep_filter_internals::mojom::LogEntryPtr> logs =
      future.Take();
  ASSERT_EQ(logs.size(), 2u);
  ASSERT_TRUE(logs[0]);
  ASSERT_TRUE(logs[1]);

  EXPECT_EQ(logs[0]->navigation_id, kTestNavigation3);
  EXPECT_EQ(logs[1]->navigation_id, kTestNavigation4);
}

}  // namespace
}  // namespace multistep_filter_internals
