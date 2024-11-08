// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/enterprise_companion/telemetry_logger/telemetry_logger.h"

#include <iterator>
#include <list>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/threading/platform_thread.h"
#include "chrome/enterprise_companion/telemetry_logger/proto/log_request.pb.h"
#include "chrome/enterprise_companion/test/test_utils.h"
#include "net/http/http_status_code.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_companion::telemetry_logger {

namespace {

struct TestEvent {
  int type = 0;
  int code = 0;
  std::string description;
};

std::string SerializeEvents(base::span<TestEvent> events) {
  return base::JoinString(
      [](base::span<TestEvent> events) {
        std::vector<std::string> serialized_events;
        base::ranges::transform(
            events, std::back_inserter(serialized_events),
            [](const TestEvent& event) {
              return base::StringPrintf(
                  "Event: type=%d, code=%d, description=[%s]", event.type,
                  event.code, event.description);
            });
        return serialized_events;
      }(events),
      "\n");
}

class MockServer : public base::RefCountedThreadSafe<MockServer> {
 public:
  explicit MockServer(base::OnceClosure quit_callback)
      : quit_callback_(std::move(quit_callback)) {}
  void ExpectRequest(const std::string& expected_request,
                     std::pair<net::HttpStatusCode, std::string> response) {
    expected_requests_.push_back(expected_request);
    responses_.push_back(response);
  }

  void HandleRequest(
      const std::string& request_body,
      base::OnceCallback<void(std::optional<int> http_status,
                              std::optional<std::string> response_body)>
          callback) {
    EXPECT_FALSE(expected_requests_.empty())
        << "request not expected: " << request_body;

    telemetry_logger::proto::LogRequest request;
    EXPECT_TRUE(request.ParseFromString(request_body))
        << " cannot parse request.";
    EXPECT_TRUE(request.has_client_info());
    EXPECT_EQ(request.client_info().client_type(),
              telemetry_logger::proto::
                  ClientInfo_ClientType_CHROME_ENTERPRISE_COMPANION);
    EXPECT_EQ(request.log_source(), 1234);
    EXPECT_EQ(request.log_event_size(), 1);
    EXPECT_EQ(request.log_event(0).source_extension(),
              expected_requests_.front());

    const auto& [status_code, response_body] = responses_.front();
    std::move(callback).Run(status_code, response_body);

    expected_requests_.pop_front();
    responses_.pop_front();
  }

  bool has_unmet_requests() const { return !expected_requests_.empty(); }

 private:
  virtual ~MockServer() {
    VLOG(1) << __func__;
    for (const auto& expected_request : expected_requests_) {
      ADD_FAILURE() << "Expected request not recieved: " << expected_request;
    }
    std::move(quit_callback_).Run();
  }

  friend class base::RefCountedThreadSafe<MockServer>;

  base::OnceClosure quit_callback_;
  std::list<std::string> expected_requests_;
  std::list<std::pair<net::HttpStatusCode, std::string>> responses_;
};

class TestDelegate : public TelemetryLogger<TestEvent>::Delegate {
 public:
  explicit TestDelegate(scoped_refptr<MockServer> server) : server_(server) {}
  ~TestDelegate() override = default;

  // Overrides for TelemetryLogger<TestEvent>::Delegate.
  bool StoreNextAllowedAttemptTime(base::Time time) override {
    next_allowed_attemp_time_ = time;
    return true;
  }
  std::optional<base::Time> GetNextAllowedAttemptTime() const override {
    return next_allowed_attemp_time_;
  }

  void DoPostRequest(
      const std::string& request_body,
      base::OnceCallback<void(std::optional<int> http_status,
                              std::optional<std::string> response_body)>
          callback) override {
    server_->HandleRequest(request_body, std::move(callback));
  }

  std::string AggregateAndSerializeEvents(
      base::span<TestEvent> events) const override {
    return SerializeEvents(events);
  }

  base::TimeDelta MinimumCooldownTime() const override {
    return base::Milliseconds(5000);
  }

  int GetLogIdentifier() const override { return 1234; }

 private:
  scoped_refptr<MockServer> server_;
  std::optional<base::Time> next_allowed_attemp_time_;
};

}  // namespace

class TelemetryLoggerTest : public testing::Test {
 protected:
  TelemetryLoggerTest() = default;
  ~TelemetryLoggerTest() override = default;

  void WaitForExpectedRequests(
      scoped_refptr<MockServer> server,
      base::TimeDelta fast_forward_interval = base::Seconds(1)) {
    while (server->has_unmet_requests()) {
      VLOG(1) << "Still wait for more requests.";
      environment_.FastForwardBy(fast_forward_interval);
      base::PlatformThread::Sleep(base::Milliseconds(0));
    }
  }

  base::test::TaskEnvironment environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(TelemetryLoggerTest, Upload) {
  base::RunLoop run_loop;
  {
    auto server = base::MakeRefCounted<MockServer>(run_loop.QuitClosure());
    auto logger = TelemetryLogger<TestEvent>::Create(
        std::make_unique<TestDelegate>(server));
    TestEvent events[] = {TestEvent(1, 2, "event 1"),
                          TestEvent(2, 2, "event 2")};
    logger->Log(events[0]);
    logger->Log(events[1]);
    server->ExpectRequest(SerializeEvents(events),
                          std::make_pair(net::HTTP_OK, ""));
    logger->Flush(base::DoNothing());
  }
  run_loop.Run();
}

TEST_F(TelemetryLoggerTest, LogsRetainedOnRetriableHTTPErrors) {
  base::RunLoop run_loop;
  {
    auto server = base::MakeRefCounted<MockServer>(run_loop.QuitClosure());
    auto logger = TelemetryLogger<TestEvent>::Create(
        std::make_unique<TestDelegate>(server));

    TestEvent events[] = {TestEvent(1, 2, "event 1")};
    std::string events_str = SerializeEvents(events);
    logger->Log(events[0]);
    for (net::HttpStatusCode http_status :
         {net::HTTP_TEMPORARY_REDIRECT, net::HTTP_USE_PROXY,
          net::HTTP_INTERNAL_SERVER_ERROR, net::HTTP_NOT_IMPLEMENTED,
          net::HTTP_BAD_GATEWAY, net::HTTP_SERVICE_UNAVAILABLE,
          net::HTTP_NETWORK_AUTHENTICATION_REQUIRED, net::HTTP_OK}) {
      server->ExpectRequest(events_str, std::make_pair(http_status, ""));
      logger->Flush(base::DoNothing());
      WaitForExpectedRequests(server);
    }
    logger->CancelCooldownTimer();
  }
  run_loop.Run();
}

TEST_F(TelemetryLoggerTest, LogsClearedOnDeterministicHTTPResult) {
  base::RunLoop run_loop;
  {
    auto server = base::MakeRefCounted<MockServer>(run_loop.QuitClosure());
    auto logger = TelemetryLogger<TestEvent>::Create(
        std::make_unique<TestDelegate>(server));

    TestEvent events[] = {TestEvent(1, 2, "event 1")};
    std::string events_str = SerializeEvents(events);
    for (net::HttpStatusCode http_status :
         {net::HTTP_OK, net::HTTP_NOT_FOUND,
          net::HTTP_NON_AUTHORITATIVE_INFORMATION, net::HTTP_ALREADY_REPORTED,
          net::HTTP_BAD_REQUEST, net::HTTP_UNAUTHORIZED, net::HTTP_FORBIDDEN,
          net::HTTP_METHOD_NOT_ALLOWED, net::HTTP_NOT_ACCEPTABLE,
          net::HTTP_REQUEST_TIMEOUT, net::HTTP_CONFLICT, net::HTTP_GONE,
          net::HTTP_EXPECTATION_FAILED, net::HTTP_TOO_EARLY,
          net::HTTP_TOO_MANY_REQUESTS}) {
      logger->Log(events[0]);
      server->ExpectRequest(events_str, std::make_pair(http_status, ""));
      logger->Flush(base::DoNothing());
      WaitForExpectedRequests(server);
      environment_.AdvanceClock(base::Seconds(10));
    }
  }
  run_loop.Run();
}

TEST_F(TelemetryLoggerTest, AutoRetry) {
  base::RunLoop run_loop;
  {
    auto server = base::MakeRefCounted<MockServer>(run_loop.QuitClosure());
    auto logger = TelemetryLogger<TestEvent>::Create(
        std::make_unique<TestDelegate>(server));

    TestEvent events[] = {TestEvent(1, 2, "event 1")};
    std::string events_str = SerializeEvents(events);
    logger->Log(events[0]);
    for (net::HttpStatusCode http_status :
         {net::HTTP_INTERNAL_SERVER_ERROR, net::HTTP_INTERNAL_SERVER_ERROR,
          net::HTTP_INTERNAL_SERVER_ERROR, net::HTTP_OK}) {
      server->ExpectRequest(events_str, std::make_pair(http_status, ""));
    }

    logger->Flush(base::DoNothing());
    WaitForExpectedRequests(server);
  }
  run_loop.Run();
}

TEST_F(TelemetryLoggerTest, UploadCombinesPreviousEvents) {
  base::RunLoop run_loop;
  {
    auto server = base::MakeRefCounted<MockServer>(run_loop.QuitClosure());
    auto logger = TelemetryLogger<TestEvent>::Create(
        std::make_unique<TestDelegate>(server));
    TestEvent events[] = {
        TestEvent(1, 3, "1st event"),
        TestEvent(2, 2, "event happened after failed upload."),
        TestEvent(3, 1, "more event happened after failed upload.")};

    logger->Log(events[0]);
    server->ExpectRequest(SerializeEvents(base::span(events).subspan(0, 1)),
                          std::make_pair(net::HTTP_INTERNAL_SERVER_ERROR, ""));
    logger->Flush(base::DoNothing());
    WaitForExpectedRequests(server);

    logger->Log(events[1]);
    server->ExpectRequest(SerializeEvents(base::span(events).subspan(0, 2)),
                          std::make_pair(net::HTTP_INTERNAL_SERVER_ERROR, ""));
    logger->Flush(base::DoNothing());
    WaitForExpectedRequests(server);

    logger->Log(events[2]);
    server->ExpectRequest(SerializeEvents(events),
                          std::make_pair(net::HTTP_INTERNAL_SERVER_ERROR, ""));
    logger->Flush(base::DoNothing());
    WaitForExpectedRequests(server);

    server->ExpectRequest(SerializeEvents(events),
                          std::make_pair(net::HTTP_OK, ""));
    logger->Flush(base::DoNothing());
    WaitForExpectedRequests(server);

    // Successfully uploaded logs should not be retransmitted.
    logger->Flush(base::DoNothing());
    WaitForExpectedRequests(server);
    logger->CancelCooldownTimer();
  }
  run_loop.Run();
}

TEST_F(TelemetryLoggerTest, DelayedUpload) {
  base::RunLoop run_loop;
  {
    auto server = base::MakeRefCounted<MockServer>(run_loop.QuitClosure());
    auto logger = TelemetryLogger<TestEvent>::Create(
        std::make_unique<TestDelegate>(server));

    TestEvent event_batch1[] = {TestEvent(1, 0, "e1")};
    telemetry_logger::proto::LogResponse response;
    response.set_next_request_wait_millis(20000);
    server->ExpectRequest(
        SerializeEvents(event_batch1),
        std::make_pair(net::HTTP_OK, response.SerializeAsString()));
    logger->Log(event_batch1[0]);
    logger->Flush(base::DoNothing());
    WaitForExpectedRequests(server);

    TestEvent event_batch2[] = {TestEvent(1, 2, "event 1"),
                                TestEvent(2, 2, "event 2")};
    server->ExpectRequest(
        SerializeEvents(event_batch2),
        std::make_pair(net::HTTP_OK, response.SerializeAsString()));
    logger->Log(event_batch2[0]);
    logger->Log(event_batch2[1]);
    logger->Flush(base::DoNothing());
  }
  run_loop.Run();
}

TEST_F(TelemetryLoggerTest, CooldownTime) {
  base::RunLoop run_loop;
  {
    auto server = base::MakeRefCounted<MockServer>(run_loop.QuitClosure());
    auto logger = TelemetryLogger<TestEvent>::Create(
        std::make_unique<TestDelegate>(server));

    TestEvent event_batch1[] = {TestEvent(1, 0, "e1")};
    telemetry_logger::proto::LogResponse response;
    response.set_next_request_wait_millis(5000);
    server->ExpectRequest(
        SerializeEvents(event_batch1),
        std::make_pair(net::HTTP_OK, response.SerializeAsString()));
    logger->Log(event_batch1[0]);
    logger->Flush(base::DoNothing());
    WaitForExpectedRequests(server);

    // Upload won't happen during cool down period.
    TestEvent event_batch2[] = {
        TestEvent(2, 0, "e2"),
        TestEvent(333, 20, "an event with a long description.")};
    logger->Log(event_batch2[0]);
    logger->Log(event_batch2[1]);
    logger->Flush(base::DoNothing());
    WaitForExpectedRequests(server);

    // Advance the clock but still in cool down period.
    environment_.AdvanceClock(base::Milliseconds(400));
    logger->Flush(base::DoNothing());
    WaitForExpectedRequests(server);

    // Cooldown time exhausted, events are uploaded.
    server->ExpectRequest(
        SerializeEvents(event_batch2),
        std::make_pair(net::HTTP_OK, response.SerializeAsString()));
    environment_.AdvanceClock(base::Milliseconds(12000));
    logger->Flush(base::DoNothing());
    WaitForExpectedRequests(server);

    // Cooldown time exhausted, but nothing to upload.
    environment_.AdvanceClock(base::Milliseconds(5500));
    logger->Flush(base::DoNothing());
    WaitForExpectedRequests(server);
  }
  run_loop.Run();
}

TEST_F(TelemetryLoggerTest, InitialCooldownTimeFromPreviousRun) {
  base::RunLoop run_loop;
  {
    auto server = base::MakeRefCounted<MockServer>(run_loop.QuitClosure());
    auto delegate = std::make_unique<TestDelegate>(server);
    delegate->StoreNextAllowedAttemptTime(base::Time::Now() +
                                          base::Seconds(60));
    auto logger = TelemetryLogger<TestEvent>::Create(std::move(delegate));

    TestEvent events[] = {TestEvent(1, 0, "initia event"),
                          TestEvent(2, 10, "event happened after some time.")};
    logger->Log(events[0]);

    // No upload during the initial cool period.
    logger->Flush(base::DoNothing());

    environment_.FastForwardBy(base::Seconds(30));
    logger->Flush(base::DoNothing());

    // Events are uploaded after the initial cool down period.
    logger->Log(events[1]);
    server->ExpectRequest(SerializeEvents(events),
                          std::make_pair(net::HTTP_OK, ""));
    environment_.FastForwardBy(base::Seconds(10));
    logger->Flush(base::DoNothing());
    logger->Flush(base::DoNothing());
  }
  run_loop.Run();
}

TEST_F(TelemetryLoggerTest, FlushCallbackIsCalledOnCallerSequence) {
  SEQUENCE_CHECKER(sequence_checker);
  base::RunLoop run_loop;
  {
    auto server = base::MakeRefCounted<MockServer>(run_loop.QuitClosure());
    auto logger = TelemetryLogger<TestEvent>::Create(
        std::make_unique<TestDelegate>(server));

    // Callback is called without upload.
    {
      base::RunLoop inner_run_loop;
      logger->Flush(base::BindLambdaForTesting([&] {
        DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker);
        inner_run_loop.Quit();
      }));
      inner_run_loop.Run();
    }

    // Callback is called after upload.
    TestEvent events[] = {TestEvent(1, 0, "any event")};
    logger->Log(events[0]);
    server->ExpectRequest(SerializeEvents(events),
                          std::make_pair(net::HTTP_OK, ""));
    {
      base::RunLoop inner_run_loop;
      logger->Flush(base::BindLambdaForTesting([&] {
        DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker);
        inner_run_loop.Quit();
      }));
      inner_run_loop.Run();
    }
  }
  run_loop.Run();
}

}  // namespace enterprise_companion::telemetry_logger
