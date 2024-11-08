// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/enterprise_companion/event_logger.h"

#include <memory>
#include <optional>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_file.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/bind_post_task.h"
#include "base/test/task_environment.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "chrome/enterprise_companion/enterprise_companion_status.h"
#include "chrome/enterprise_companion/global_constants.h"
#include "chrome/enterprise_companion/ipc_support.h"
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
