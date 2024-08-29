// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/enterprise_companion/event_logger.h"

#include <memory>
#include <optional>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/files/scoped_temp_file.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/bind_post_task.h"
#include "base/test/task_environment.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "chrome/enterprise_companion/enterprise_companion_branding.h"
#include "chrome/enterprise_companion/enterprise_companion_status.h"
#include "chrome/enterprise_companion/global_constants.h"
#include "chrome/enterprise_companion/ipc_support.h"
#include "chrome/enterprise_companion/proto/enterprise_companion_event.pb.h"
#include "chrome/enterprise_companion/proto/log_request.pb.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/cookies/cookie_access_result.h"
#include "net/cookies/cookie_change_dispatcher.h"
#include "net/http/http_status_code.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_cookie_manager.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_companion {

namespace {

class MockEventLogUploader final : public EventLogUploader {
 public:
  MockEventLogUploader() = default;
  ~MockEventLogUploader() override = default;

  MOCK_METHOD(void,
              DoLogRequest,
              (proto::LogRequest request, LogRequestCallback callback),
              (override));
};

}  // namespace

class EventLoggerTest : public ::testing::Test {
 protected:
  base::test::TaskEnvironment environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(EventLoggerTest, TransmitsEvents) {
  const EnterpriseCompanionStatus policy_persistence_failed =
      EnterpriseCompanionStatus(ApplicationError::kPolicyPersistenceFailed);
  std::unique_ptr<MockEventLogUploader> mock_uploader =
      std::make_unique<MockEventLogUploader>();
  std::optional<proto::LogRequest> captured_request;

  EXPECT_CALL(*mock_uploader, DoLogRequest)
      .WillOnce([&](proto::LogRequest request,
                    MockEventLogUploader::LogRequestCallback callback) {
        captured_request = std::move(request);
        proto::LogResponse response;
        response.set_next_request_wait_millis(
            GetGlobalConstants()->EventLoggerMinTimeout().InMilliseconds());
        std::move(callback).Run(network::CreateURLResponseHead(net::HTTP_OK),
                                response.SerializeAsString());
      });

  std::unique_ptr<EventLoggerManager> event_logger_manager =
      CreateEventLoggerManager(std::move(mock_uploader),
                               environment_.GetMockClock());
  scoped_refptr<EventLogger> event_logger =
      event_logger_manager->CreateEventLogger();

  // Logging callbacks are intended to be run from any sequence. They accomplish
  // this by posting to the event logger's sequence. As such, the test must wait
  // for the environment to become idle before flushing.
  event_logger->OnEnrollmentStart().Run(EnterpriseCompanionStatus::Success());
  event_logger->OnPolicyFetchStart().Run(EnterpriseCompanionStatus::Success());
  event_logger->OnPolicyFetchStart().Run(
      EnterpriseCompanionStatus(policy_persistence_failed));
  environment_.AdvanceClock(base::Seconds(10));
  environment_.RunUntilIdle();
  event_logger->Flush();

  ASSERT_TRUE(captured_request);

  ASSERT_TRUE(captured_request->has_client_info());
  EXPECT_EQ(captured_request->client_info().client_type(),
            proto::ClientInfo::CHROME_ENTERPRISE_COMPANION);

  EXPECT_EQ(captured_request->log_source(),
            proto::CHROME_ENTERPRISE_COMPANION_APP);

  EXPECT_EQ(captured_request->log_event_size(), 1);
  EXPECT_EQ(captured_request->log_event(0).event_time_ms(),
            environment_.GetMockClock()->Now().InMillisecondsSinceUnixEpoch());
  proto::ChromeEnterpriseCompanionAppExtension extension;
  ASSERT_TRUE(extension.ParseFromString(
      captured_request->log_event(0).source_extension()));

  EXPECT_EQ(extension.event_size(), 3);

  // Enrollment event: Success.
  EXPECT_TRUE(extension.event(0).has_browser_enrollment_event());
  ASSERT_TRUE(extension.event(0).has_status());
  EXPECT_EQ(extension.event(0).status().space(), 0);
  EXPECT_EQ(extension.event(0).status().code(), 0);
  EXPECT_EQ(extension.event(1).duration_ms(),
            base::Seconds(10).InMilliseconds());

  // Policy fetch event: Success.
  EXPECT_TRUE(extension.event(1).has_policy_fetch_event());
  ASSERT_TRUE(extension.event(1).has_status());
  EXPECT_EQ(extension.event(1).status().space(), 0);
  EXPECT_EQ(extension.event(1).status().code(), 0);
  EXPECT_EQ(extension.event(1).duration_ms(),
            base::Seconds(10).InMilliseconds());

  // Policy fetch event: kPolicyPersistenceFailed.
  EXPECT_TRUE(extension.event(2).has_policy_fetch_event());
  ASSERT_TRUE(extension.event(2).has_status());
  EXPECT_EQ(extension.event(2).status().space(),
            policy_persistence_failed.space());
  EXPECT_EQ(extension.event(2).status().code(),
            policy_persistence_failed.code());
  EXPECT_EQ(extension.event(2).duration_ms(),
            base::Seconds(10).InMilliseconds());
}

TEST_F(EventLoggerTest, RespectsMinimumCooldown) {
  std::unique_ptr<MockEventLogUploader> mock_uploader =
      std::make_unique<MockEventLogUploader>();
  std::optional<proto::LogRequest> captured_request;

  EXPECT_CALL(*mock_uploader, DoLogRequest)
      .WillRepeatedly([&](proto::LogRequest request,
                          MockEventLogUploader::LogRequestCallback callback) {
        captured_request = std::move(request);
        proto::LogResponse response;
        // Respond with a cooldown less than the hardcoded minimum.
        response.set_next_request_wait_millis(0);
        std::move(callback).Run(network::CreateURLResponseHead(net::HTTP_OK),
                                response.SerializeAsString());
      });

  std::unique_ptr<EventLoggerManager> event_logger_manager =
      CreateEventLoggerManager(std::move(mock_uploader),
                               environment_.GetMockClock());
  scoped_refptr<EventLogger> event_logger =
      event_logger_manager->CreateEventLogger();

  // Log and flush an event. The cooldown should be active.
  event_logger->OnPolicyFetchStart().Run(EnterpriseCompanionStatus::Success());
  environment_.RunUntilIdle();
  event_logger->Flush();
  ASSERT_TRUE(captured_request);

  captured_request = std::nullopt;

  // Subsequent flushes should queue events until the cooldown is over.
  event_logger->OnPolicyFetchStart().Run(EnterpriseCompanionStatus::Success());
  environment_.RunUntilIdle();
  event_logger->Flush();
  ASSERT_FALSE(captured_request);

  event_logger->OnPolicyFetchStart().Run(EnterpriseCompanionStatus::Success());
  environment_.RunUntilIdle();
  event_logger->Flush();
  ASSERT_FALSE(captured_request);

  // The passage of time triggers the uploading of queued logs.
  environment_.AdvanceClock(GetGlobalConstants()->EventLoggerMinTimeout());
  environment_.RunUntilIdle();
  ASSERT_TRUE(captured_request);

  EXPECT_EQ(captured_request->log_event_size(), 1);
  proto::ChromeEnterpriseCompanionAppExtension extension;
  ASSERT_TRUE(extension.ParseFromString(
      captured_request->log_event(0).source_extension()));
  EXPECT_EQ(extension.event_size(), 2);
}

TEST_F(EventLoggerTest, RespectsRequestedCooldown) {
  std::unique_ptr<MockEventLogUploader> mock_uploader =
      std::make_unique<MockEventLogUploader>();
  std::optional<proto::LogRequest> captured_request;

  EXPECT_CALL(*mock_uploader, DoLogRequest)
      .WillRepeatedly([&](proto::LogRequest request,
                          MockEventLogUploader::LogRequestCallback callback) {
        captured_request = std::move(request);
        proto::LogResponse response;
        // Respond with a cooldown greater than the hardcoded minimum.
        response.set_next_request_wait_millis(
            GetGlobalConstants()->EventLoggerMinTimeout().InMilliseconds() * 2);
        std::move(callback).Run(network::CreateURLResponseHead(net::HTTP_OK),
                                response.SerializeAsString());
      });

  std::unique_ptr<EventLoggerManager> event_logger_manager =
      CreateEventLoggerManager(std::move(mock_uploader),
                               environment_.GetMockClock());
  scoped_refptr<EventLogger> event_logger =
      event_logger_manager->CreateEventLogger();

  // Log and flush an event. The cooldown should be active.
  event_logger->OnPolicyFetchStart().Run(EnterpriseCompanionStatus::Success());
  environment_.RunUntilIdle();
  event_logger->Flush();
  ASSERT_TRUE(captured_request);

  captured_request = std::nullopt;

  // Subsequent flushes should queue events until the cooldown is over.
  event_logger->OnPolicyFetchStart().Run(EnterpriseCompanionStatus::Success());
  environment_.RunUntilIdle();
  event_logger->Flush();
  ASSERT_FALSE(captured_request);

  event_logger->OnPolicyFetchStart().Run(EnterpriseCompanionStatus::Success());
  environment_.RunUntilIdle();
  event_logger->Flush();
  ASSERT_FALSE(captured_request);

  // Logs should not be sent using the default cooldown.
  environment_.AdvanceClock(GetGlobalConstants()->EventLoggerMinTimeout());
  environment_.RunUntilIdle();
  ASSERT_FALSE(captured_request);

  environment_.AdvanceClock(GetGlobalConstants()->EventLoggerMinTimeout());
  environment_.RunUntilIdle();
  ASSERT_TRUE(captured_request);

  EXPECT_EQ(captured_request->log_event_size(), 1);
  proto::ChromeEnterpriseCompanionAppExtension extension;
  ASSERT_TRUE(extension.ParseFromString(
      captured_request->log_event(0).source_extension()));
  EXPECT_EQ(extension.event_size(), 2);
}

TEST_F(EventLoggerTest, RequestsBatchedAcrossLoggers) {
  std::unique_ptr<MockEventLogUploader> mock_uploader =
      std::make_unique<MockEventLogUploader>();
  std::optional<proto::LogRequest> captured_request;

  EXPECT_CALL(*mock_uploader, DoLogRequest)
      .WillRepeatedly([&](proto::LogRequest request,
                          MockEventLogUploader::LogRequestCallback callback) {
        captured_request = std::move(request);
        std::move(callback).Run(network::CreateURLResponseHead(net::HTTP_OK),
                                proto::LogResponse().SerializeAsString());
      });

  std::unique_ptr<EventLoggerManager> event_logger_manager =
      CreateEventLoggerManager(std::move(mock_uploader),
                               environment_.GetMockClock());
  scoped_refptr<EventLogger> event_logger =
      event_logger_manager->CreateEventLogger();

  // Log and flush an event. The cooldown should be active.
  event_logger->OnPolicyFetchStart().Run(EnterpriseCompanionStatus::Success());
  environment_.RunUntilIdle();
  event_logger->Flush();
  ASSERT_TRUE(captured_request);

  captured_request = std::nullopt;

  // Queue events from a variety of loggers, taking advantage of the fact that
  // each logger will flush on destruction.
  event_logger_manager->CreateEventLogger()->OnPolicyFetchStart().Run(
      EnterpriseCompanionStatus::Success());
  event_logger_manager->CreateEventLogger()->OnPolicyFetchStart().Run(
      EnterpriseCompanionStatus::Success());
  event_logger_manager->CreateEventLogger()->OnPolicyFetchStart().Run(
      EnterpriseCompanionStatus::Success());

  // The passage of time triggers the uploading of queued logs.
  environment_.AdvanceClock(GetGlobalConstants()->EventLoggerMinTimeout());
  environment_.RunUntilIdle();
  ASSERT_TRUE(captured_request);

  EXPECT_EQ(captured_request->log_event_size(), 1);
  proto::ChromeEnterpriseCompanionAppExtension extension;
  ASSERT_TRUE(extension.ParseFromString(
      captured_request->log_event(0).source_extension()));
  EXPECT_EQ(extension.event_size(), 3);
}

TEST_F(EventLoggerTest, FlushedLogsClearedOnHTTP2XX) {
  std::unique_ptr<MockEventLogUploader> mock_uploader =
      std::make_unique<MockEventLogUploader>();
  std::optional<proto::LogRequest> captured_request;

  EXPECT_CALL(*mock_uploader, DoLogRequest)
      .WillRepeatedly([&](proto::LogRequest request,
                          MockEventLogUploader::LogRequestCallback callback) {
        captured_request = std::move(request);
        std::move(callback).Run(
            network::CreateURLResponseHead(net::HTTP_CREATED),
            proto::LogResponse().SerializeAsString());
      });

  std::unique_ptr<EventLoggerManager> event_logger_manager =
      CreateEventLoggerManager(std::move(mock_uploader),
                               environment_.GetMockClock());
  scoped_refptr<EventLogger> event_logger =
      event_logger_manager->CreateEventLogger();

  event_logger->OnPolicyFetchStart().Run(EnterpriseCompanionStatus::Success());
  event_logger->OnPolicyFetchStart().Run(EnterpriseCompanionStatus::Success());
  event_logger->OnPolicyFetchStart().Run(EnterpriseCompanionStatus::Success());
  environment_.RunUntilIdle();
  event_logger->Flush();
  ASSERT_TRUE(captured_request);

  captured_request = std::nullopt;

  // The flushed logs should not be retransmitted.
  environment_.AdvanceClock(GetGlobalConstants()->EventLoggerMinTimeout());
  environment_.RunUntilIdle();
  ASSERT_FALSE(captured_request);
}

TEST_F(EventLoggerTest, FlushedLogsClearedOnHTTP4XX) {
  std::unique_ptr<MockEventLogUploader> mock_uploader =
      std::make_unique<MockEventLogUploader>();
  std::optional<proto::LogRequest> captured_request;

  EXPECT_CALL(*mock_uploader, DoLogRequest)
      .WillRepeatedly([&](proto::LogRequest request,
                          MockEventLogUploader::LogRequestCallback callback) {
        captured_request = std::move(request);
        std::move(callback).Run(
            network::CreateURLResponseHead(net::HTTP_NOT_FOUND),
            proto::LogResponse().SerializeAsString());
      });

  std::unique_ptr<EventLoggerManager> event_logger_manager =
      CreateEventLoggerManager(std::move(mock_uploader),
                               environment_.GetMockClock());
  scoped_refptr<EventLogger> event_logger =
      event_logger_manager->CreateEventLogger();

  event_logger->OnPolicyFetchStart().Run(EnterpriseCompanionStatus::Success());
  event_logger->OnPolicyFetchStart().Run(EnterpriseCompanionStatus::Success());
  event_logger->OnPolicyFetchStart().Run(EnterpriseCompanionStatus::Success());
  environment_.RunUntilIdle();
  event_logger->Flush();
  ASSERT_TRUE(captured_request);

  captured_request = std::nullopt;

  // The flushed logs should not be retransmitted.
  environment_.AdvanceClock(GetGlobalConstants()->EventLoggerMinTimeout());
  environment_.RunUntilIdle();
  ASSERT_FALSE(captured_request);
}

TEST_F(EventLoggerTest, FlushedLogsRetainedOnHTTP5XX) {
  std::unique_ptr<MockEventLogUploader> mock_uploader =
      std::make_unique<MockEventLogUploader>();
  std::optional<proto::LogRequest> captured_request;

  EXPECT_CALL(*mock_uploader, DoLogRequest)
      .WillRepeatedly([&](proto::LogRequest request,
                          MockEventLogUploader::LogRequestCallback callback) {
        captured_request = std::move(request);
        std::move(callback).Run(
            network::CreateURLResponseHead(net::HTTP_SERVICE_UNAVAILABLE),
            proto::LogResponse().SerializeAsString());
      });

  std::unique_ptr<EventLoggerManager> event_logger_manager =
      CreateEventLoggerManager(std::move(mock_uploader),
                               environment_.GetMockClock());
  scoped_refptr<EventLogger> event_logger =
      event_logger_manager->CreateEventLogger();

  event_logger->OnPolicyFetchStart().Run(EnterpriseCompanionStatus::Success());
  event_logger->OnPolicyFetchStart().Run(EnterpriseCompanionStatus::Success());
  event_logger->OnPolicyFetchStart().Run(EnterpriseCompanionStatus::Success());
  environment_.RunUntilIdle();
  event_logger->Flush();
  ASSERT_TRUE(captured_request);

  captured_request = std::nullopt;

  // The flushed logs should be retransmitted.
  environment_.AdvanceClock(GetGlobalConstants()->EventLoggerMinTimeout());
  environment_.RunUntilIdle();
  ASSERT_TRUE(captured_request);

  EXPECT_EQ(captured_request->log_event_size(), 1);
  proto::ChromeEnterpriseCompanionAppExtension extension;
  ASSERT_TRUE(extension.ParseFromString(
      captured_request->log_event(0).source_extension()));
  EXPECT_EQ(extension.event_size(), 3);
}

class MockCookieManager
    : public ::testing::StrictMock<network::TestCookieManager> {
 public:
  MOCK_METHOD(void,
              SetCanonicalCookie,
              (const net::CanonicalCookie& cookie,
               const GURL& source_url,
               const net::CookieOptions& cookie_options,
               SetCanonicalCookieCallback callback),
              (override));
};

class EventLoggerCookieHandlerTest : public ::testing::Test {
 public:
  void SetUp() override { ASSERT_TRUE(cookie_file_.Create()); }

 protected:
  void InitCookieHandlerAndWait() {
    cookie_handler_ = CreateEventLoggerCookieHandler(base::File(
        cookie_file_.path(), base::File::FLAG_OPEN | base::File::FLAG_READ |
                                 base::File::FLAG_WRITE));
    ASSERT_TRUE(cookie_handler_);
    base::RunLoop run_loop;
    cookie_handler_.AsyncCall(&EventLoggerCookieHandler::Init)
        .WithArgs(std::move(cookie_manager_pending_remote_),
                  base::BindPostTaskToCurrentDefault(run_loop.QuitClosure()));
    run_loop.Run();
  }

  void DispatchLoggingCookieChange(const std::string& logging_cookie_value,
                                   net::CookieChangeCause cause) {
    const GURL event_logging_url =
        GetGlobalConstants()->EnterpriseCompanionEventLoggingURL();
    url::SchemeHostPort event_logging_scheme_host_port =
        url::SchemeHostPort(event_logging_url);
    std::unique_ptr<net::CanonicalCookie> cookie =
        net::CanonicalCookie::CreateSanitizedCookie(
            event_logging_url, kLoggingCookieName, logging_cookie_value,
            base::StrCat({".", event_logging_scheme_host_port.host()}),
            /*path=*/"/", /*creation_time=*/base::Time::Now(),
            /*expiration_time=*/base::Time::Now() + base::Days(180),
            /*last_access_time=*/base::Time::Now(), /*secure=*/false,
            /*http_only=*/true, net::CookieSameSite::UNSPECIFIED,
            net::CookiePriority::COOKIE_PRIORITY_DEFAULT,
            /*partition_key=*/std::nullopt, /*status=*/nullptr);
    mock_cookie_manager_.DispatchCookieChange(
        net::CookieChangeInfo(*cookie, net::CookieAccessResult(), cause));

    // The CookieListener interface does not have completion callbacks.
    // RunUntilIdle is used to ensure that the EventLoggerCookieHandler has
    // processed the OnCookieChange event before making assertions.
    environment_.RunUntilIdle();
  }

  void ExpectCookieFileContents(const std::string& expected_cookie_value) {
    std::string cookie_value;
    ASSERT_TRUE(base::ReadFileToString(cookie_file_.path(), &cookie_value));
    EXPECT_EQ(cookie_value, expected_cookie_value);
  }

  base::test::TaskEnvironment environment_;
  ScopedIPCSupportWrapper ipc_support_;
  base::ScopedTempFile cookie_file_;
  MockCookieManager mock_cookie_manager_;
  mojo::PendingRemote<network::mojom::CookieManager>
      cookie_manager_pending_remote_;
  mojo::Receiver<network::mojom::CookieManager> cookie_manager_receiver_{
      &mock_cookie_manager_,
      cookie_manager_pending_remote_.InitWithNewPipeAndPassReceiver()};
  base::SequenceBound<EventLoggerCookieHandler> cookie_handler_;
};

TEST_F(EventLoggerCookieHandlerTest, InitializesNewLoggingCookie) {
  EXPECT_CALL(mock_cookie_manager_, SetCanonicalCookie)
      .WillOnce([](const net::CanonicalCookie& cookie, const GURL& source_url,
                   const net::CookieOptions&,
                   MockCookieManager::SetCanonicalCookieCallback callback) {
        ASSERT_TRUE(cookie.IsCanonical());
        EXPECT_EQ(cookie.Name(), kLoggingCookieName);
        EXPECT_EQ(cookie.Value(), kLoggingCookieDefaultValue);
        EXPECT_EQ(source_url,
                  GetGlobalConstants()->EnterpriseCompanionEventLoggingURL());
        std::move(callback).Run(net::CookieAccessResult());
      });

  InitCookieHandlerAndWait();
}

TEST_F(EventLoggerCookieHandlerTest, InitializesExistingLoggingCookie) {
  EXPECT_CALL(mock_cookie_manager_, SetCanonicalCookie)
      .WillOnce([](const net::CanonicalCookie& cookie, const GURL& source_url,
                   const net::CookieOptions&,
                   MockCookieManager::SetCanonicalCookieCallback callback) {
        ASSERT_TRUE(cookie.IsCanonical());
        EXPECT_EQ(cookie.Name(), kLoggingCookieName);
        EXPECT_EQ(cookie.Value(), "123");
        EXPECT_EQ(source_url,
                  GetGlobalConstants()->EnterpriseCompanionEventLoggingURL());
        std::move(callback).Run(net::CookieAccessResult());
      });

  ASSERT_TRUE(base::WriteFile(cookie_file_.path(), "123"));

  InitCookieHandlerAndWait();
}

TEST_F(EventLoggerCookieHandlerTest, PersistsLoggingCookie) {
  EXPECT_CALL(mock_cookie_manager_, SetCanonicalCookie)
      .WillOnce([](const net::CanonicalCookie&, const GURL&,
                   const net::CookieOptions&,
                   MockCookieManager::SetCanonicalCookieCallback callback) {
        std::move(callback).Run(net::CookieAccessResult());
      });
  InitCookieHandlerAndWait();

  DispatchLoggingCookieChange("123", net::CookieChangeCause::INSERTED);

  ExpectCookieFileContents("123");
}

TEST_F(EventLoggerCookieHandlerTest, OverwritesLoggingCookie) {
  EXPECT_CALL(mock_cookie_manager_, SetCanonicalCookie)
      .WillOnce([](const net::CanonicalCookie&, const GURL&,
                   const net::CookieOptions&,
                   MockCookieManager::SetCanonicalCookieCallback callback) {
        std::move(callback).Run(net::CookieAccessResult());
      });
  InitCookieHandlerAndWait();

  DispatchLoggingCookieChange("123", net::CookieChangeCause::INSERTED);
  DispatchLoggingCookieChange("456", net::CookieChangeCause::INSERTED);

  ExpectCookieFileContents("456");
}

TEST_F(EventLoggerCookieHandlerTest, IgnoresNonInsertionEvents) {
  EXPECT_CALL(mock_cookie_manager_, SetCanonicalCookie)
      .WillOnce([](const net::CanonicalCookie&, const GURL&,
                   const net::CookieOptions&,
                   MockCookieManager::SetCanonicalCookieCallback callback) {
        std::move(callback).Run(net::CookieAccessResult());
      });

  InitCookieHandlerAndWait();
  DispatchLoggingCookieChange("123", net::CookieChangeCause::EXPLICIT);
  DispatchLoggingCookieChange("123", net::CookieChangeCause::UNKNOWN_DELETION);
  DispatchLoggingCookieChange("123", net::CookieChangeCause::OVERWRITE);
  DispatchLoggingCookieChange("123", net::CookieChangeCause::EXPIRED);
  DispatchLoggingCookieChange("123", net::CookieChangeCause::EVICTED);
  DispatchLoggingCookieChange("123", net::CookieChangeCause::EXPIRED_OVERWRITE);

  ExpectCookieFileContents("");
}

}  // namespace enterprise_companion
