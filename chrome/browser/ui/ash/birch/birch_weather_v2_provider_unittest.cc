// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/birch/birch_weather_v2_provider.h"

#include <memory>
#include <vector>

#include "ash/birch/birch_model.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/shell.h"
#include "ash/system/geolocation/test_geolocation_url_loader_factory.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/geolocation/simple_geolocation_provider.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

using ::base::test::TestFuture;
using ::net::test_server::BasicHttpResponse;
using ::net::test_server::HttpMethod;
using ::net::test_server::HttpRequest;
using ::net::test_server::HttpResponse;
using ::testing::AllOf;
using ::testing::ByMove;
using ::testing::Field;
using ::testing::Invoke;
using ::testing::Return;

// Helper class to simplify mocking `net::EmbeddedTestServer` responses,
// especially useful for subsequent responses when testing pagination logic.
class TestRequestHandler {
 public:
  static std::unique_ptr<HttpResponse> CreateSuccessfulResponse(
      const std::string& content) {
    auto response = std::make_unique<BasicHttpResponse>();
    response->set_code(net::HTTP_OK);
    response->set_content(content);
    response->set_content_type("application/json");
    return response;
  }

  static std::unique_ptr<HttpResponse> CreateFailedResponse() {
    auto response = std::make_unique<BasicHttpResponse>();
    response->set_code(net::HTTP_INTERNAL_SERVER_ERROR);
    return response;
  }

  MOCK_METHOD(std::unique_ptr<HttpResponse>,
              HandleRequest,
              (const HttpRequest&));
};

class BirchWeatherV2ProviderTest : public testing::Test {
 public:
  BirchWeatherV2ProviderTest()
      : profile_manager_(
            TestingProfileManager(TestingBrowserProcess::GetGlobal())) {}

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());

    SimpleGeolocationProvider::Initialize(
        base::MakeRefCounted<TestGeolocationUrlLoaderFactory>());

    profile_ = profile_manager_.CreateTestingProfile("profile@example.com",
                                                     /*is_main_profile=*/true,
                                                     url_loader_factory_);
    weather_provider_ = std::make_unique<BirchWeatherV2Provider>(
        profile_,
        base::BindRepeating(&BirchWeatherV2ProviderTest::WeatherItemsUpdated,
                            base::Unretained(this)));

    test_server_.RegisterRequestHandler(
        base::BindRepeating(&TestRequestHandler::HandleRequest,
                            base::Unretained(&request_handler_)));
    ASSERT_TRUE(test_server_.Start());

    weather_provider_->OverrideBaseRequestUrlForTesting(
        test_server_.base_url());
  }

  void TearDown() override { SimpleGeolocationProvider::DestroyForTesting(); }

  using ItemsCallback = base::OnceCallback<void(std::vector<BirchWeatherItem>)>;
  void SetItemsCallback(ItemsCallback callback) {
    items_callback_ = std::move(callback);
  }

  TestRequestHandler& request_handler() { return request_handler_; }

  BirchWeatherV2Provider* weather_provider() { return weather_provider_.get(); }

  PrefService* GetPrefService() { return profile_->GetTestingPrefService(); }

 private:
  void WeatherItemsUpdated(std::vector<BirchWeatherItem> items) {
    if (items_callback_) {
      std::move(items_callback_).Run(std::move(items));
    }
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
  TestingProfileManager profile_manager_;
  net::EmbeddedTestServer test_server_;
  scoped_refptr<network::TestSharedURLLoaderFactory> url_loader_factory_ =
      base::MakeRefCounted<network::TestSharedURLLoaderFactory>(
          /*network_service=*/nullptr,
          /*is_trusted=*/true);
  testing::StrictMock<TestRequestHandler> request_handler_;
  data_decoder::test::InProcessDataDecoder data_decoder_;

  std::unique_ptr<BirchWeatherV2Provider> weather_provider_;

  ItemsCallback items_callback_;

  raw_ptr<TestingProfile> profile_;
};

TEST_F(BirchWeatherV2ProviderTest, WeatherWithTemp) {
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  "/v1/weather?feature_id=1")))
      .WillOnce(Return(ByMove(
          TestRequestHandler::CreateSuccessfulResponse(R"({"tempF": 70})"))));

  TestFuture<std::vector<BirchWeatherItem>> items_future;
  SetItemsCallback(items_future.GetCallback());

  weather_provider()->RequestBirchDataFetch();

  ASSERT_TRUE(items_future.Wait());
  auto weather_items = items_future.Take();
  ASSERT_EQ(1u, weather_items.size());
  EXPECT_EQ(u"[i18n] Current weather", weather_items[0].title());
  EXPECT_FLOAT_EQ(70.f, weather_items[0].temp_f());
}

TEST_F(BirchWeatherV2ProviderTest, WeatherWithNonIntegerTemp) {
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  "/v1/weather?feature_id=1")))
      .WillOnce(Return(ByMove(
          TestRequestHandler::CreateSuccessfulResponse(R"({"tempF": 71.3})"))));

  TestFuture<std::vector<BirchWeatherItem>> items_future;
  SetItemsCallback(items_future.GetCallback());

  weather_provider()->RequestBirchDataFetch();

  ASSERT_TRUE(items_future.Wait());
  auto weather_items = items_future.Take();
  ASSERT_EQ(1u, weather_items.size());
  EXPECT_EQ(u"[i18n] Current weather", weather_items[0].title());
  EXPECT_EQ(71.3f, weather_items[0].temp_f());
}

TEST_F(BirchWeatherV2ProviderTest, ConcurrentRequests) {
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  "/v1/weather?feature_id=1")))
      .WillOnce(Return(ByMove(
          TestRequestHandler::CreateSuccessfulResponse(R"({"tempF": 70})"))));

  TestFuture<std::vector<BirchWeatherItem>> items_future;
  SetItemsCallback(items_future.GetCallback());

  weather_provider()->RequestBirchDataFetch();
  weather_provider()->RequestBirchDataFetch();

  ASSERT_TRUE(items_future.Wait());
  auto weather_items = items_future.Take();
  ASSERT_EQ(1u, weather_items.size());
  EXPECT_EQ(u"[i18n] Current weather", weather_items[0].title());
  EXPECT_FLOAT_EQ(70.f, weather_items[0].temp_f());
}

TEST_F(BirchWeatherV2ProviderTest, SequentialRequests) {
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  "/v1/weather?feature_id=1")))
      .WillOnce(Return(ByMove(
          TestRequestHandler::CreateSuccessfulResponse(R"({"tempF": 70})"))))
      .WillOnce(Return(ByMove(
          TestRequestHandler::CreateSuccessfulResponse(R"({"tempF": 71})"))));

  TestFuture<std::vector<BirchWeatherItem>> items_future;
  SetItemsCallback(items_future.GetCallback());
  weather_provider()->RequestBirchDataFetch();

  ASSERT_TRUE(items_future.Wait());
  auto weather_items = items_future.Take();
  ASSERT_EQ(1u, weather_items.size());
  EXPECT_EQ(u"[i18n] Current weather", weather_items[0].title());
  EXPECT_FLOAT_EQ(70.f, weather_items[0].temp_f());

  TestFuture<std::vector<BirchWeatherItem>> second_items_future;
  SetItemsCallback(second_items_future.GetCallback());
  weather_provider()->RequestBirchDataFetch();

  ASSERT_TRUE(second_items_future.Wait());
  weather_items = second_items_future.Take();
  ASSERT_EQ(1u, weather_items.size());
  EXPECT_EQ(u"[i18n] Current weather", weather_items[0].title());
  EXPECT_FLOAT_EQ(71.f, weather_items[0].temp_f());
}

TEST_F(BirchWeatherV2ProviderTest, FailedRequest) {
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  "/v1/weather?feature_id=1")))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateFailedResponse())));

  TestFuture<std::vector<BirchWeatherItem>> items_future;
  SetItemsCallback(items_future.GetCallback());
  weather_provider()->RequestBirchDataFetch();

  ASSERT_TRUE(items_future.Wait());
  auto weather_items = items_future.Take();
  EXPECT_EQ(0u, weather_items.size());
}

TEST_F(BirchWeatherV2ProviderTest, InvalidResponse) {
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  "/v1/weather?feature_id=1")))
      .WillOnce(Return(
          ByMove(TestRequestHandler::CreateSuccessfulResponse("}{----!~"))));

  TestFuture<std::vector<BirchWeatherItem>> items_future;
  SetItemsCallback(items_future.GetCallback());

  weather_provider()->RequestBirchDataFetch();

  ASSERT_TRUE(items_future.Wait());
  auto weather_items = items_future.Take();
  EXPECT_EQ(0u, weather_items.size());
}

TEST_F(BirchWeatherV2ProviderTest, UnexpectedResponse_List) {
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  "/v1/weather?feature_id=1")))
      .WillOnce(Return(ByMove(
          TestRequestHandler::CreateSuccessfulResponse(R"([{"tempF": 3}])"))));

  TestFuture<std::vector<BirchWeatherItem>> items_future;
  SetItemsCallback(items_future.GetCallback());

  weather_provider()->RequestBirchDataFetch();

  ASSERT_TRUE(items_future.Wait());
  auto weather_items = items_future.Take();
  EXPECT_EQ(0u, weather_items.size());
}

TEST_F(BirchWeatherV2ProviderTest, UnexpectedResponse_Integer) {
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  "/v1/weather?feature_id=1")))
      .WillOnce(
          Return(ByMove(TestRequestHandler::CreateSuccessfulResponse("404"))));

  TestFuture<std::vector<BirchWeatherItem>> items_future;
  SetItemsCallback(items_future.GetCallback());

  weather_provider()->RequestBirchDataFetch();

  ASSERT_TRUE(items_future.Wait());
  auto weather_items = items_future.Take();
  EXPECT_EQ(0u, weather_items.size());
}

TEST_F(BirchWeatherV2ProviderTest, UnexpectedResponse_EmptyDict) {
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  "/v1/weather?feature_id=1")))
      .WillOnce(
          Return(ByMove(TestRequestHandler::CreateSuccessfulResponse("{}"))));

  TestFuture<std::vector<BirchWeatherItem>> items_future;
  SetItemsCallback(items_future.GetCallback());

  weather_provider()->RequestBirchDataFetch();

  ASSERT_TRUE(items_future.Wait());
  auto weather_items = items_future.Take();
  EXPECT_EQ(0u, weather_items.size());
}

TEST_F(BirchWeatherV2ProviderTest, GeolocationDisabled) {
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  "/v1/weather?feature_id=1")))
      .Times(0);

  // Disable geolocation.
  SimpleGeolocationProvider::GetInstance()->SetGeolocationAccessLevel(
      GeolocationAccessLevel::kDisallowed);

  TestFuture<std::vector<BirchWeatherItem>> items_future;
  SetItemsCallback(items_future.GetCallback());

  weather_provider()->RequestBirchDataFetch();

  ASSERT_TRUE(items_future.Wait());
  auto weather_items = items_future.Take();
  EXPECT_EQ(0u, weather_items.size());
}

TEST_F(BirchWeatherV2ProviderTest, DisabledByPolicy) {
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  "/v1/weather?feature_id=1")))
      .Times(0);

  // Disable by policy.
  GetPrefService()->SetList(prefs::kContextualGoogleIntegrationsConfiguration,
                            {});

  TestFuture<std::vector<BirchWeatherItem>> items_future;
  SetItemsCallback(items_future.GetCallback());

  weather_provider()->RequestBirchDataFetch();

  ASSERT_TRUE(items_future.Wait());
  auto weather_items = items_future.Take();
  EXPECT_EQ(0u, weather_items.size());
}

TEST_F(BirchWeatherV2ProviderTest, FailedRequestWithSuccessfulRetry) {
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  "/v1/weather?feature_id=1")))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateFailedResponse())))
      .WillOnce(Return(ByMove(
          TestRequestHandler::CreateSuccessfulResponse(R"({"tempF": 71})"))));

  TestFuture<std::vector<BirchWeatherItem>> items_future;
  SetItemsCallback(items_future.GetCallback());
  weather_provider()->RequestBirchDataFetch();

  ASSERT_TRUE(items_future.Wait());
  auto weather_items = items_future.Take();
  EXPECT_EQ(0u, weather_items.size());

  TestFuture<std::vector<BirchWeatherItem>> second_items_future;
  SetItemsCallback(second_items_future.GetCallback());
  weather_provider()->RequestBirchDataFetch();

  ASSERT_TRUE(second_items_future.Wait());
  weather_items = second_items_future.Take();
  ASSERT_EQ(1u, weather_items.size());
  EXPECT_EQ(u"[i18n] Current weather", weather_items[0].title());
  EXPECT_FLOAT_EQ(71.f, weather_items[0].temp_f());
}

TEST_F(BirchWeatherV2ProviderTest, ImmediateProviderShutdownCancelsRequest) {
  base::RunLoop request_waiter;
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  "/v1/weather?feature_id=1")))
      .Times(0);

  TestFuture<std::vector<BirchWeatherItem>> items_future;
  SetItemsCallback(base::BindOnce([](std::vector<BirchWeatherItem> items) {
    ADD_FAILURE() << "Model updated unexpecstedly after shutdown";
  }));
  weather_provider()->RequestBirchDataFetch();
  weather_provider()->Shutdown();

  // Flush any tasks potentaly started asynchronously by the provider .
  base::RunLoop().RunUntilIdle();
}

TEST_F(BirchWeatherV2ProviderTest, ProviderShutdownMidRequest) {
  base::RunLoop request_waiter;
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  "/v1/weather?feature_id=1")))
      .WillOnce(Invoke([&](const HttpRequest& request) {
        request_waiter.Quit();
        return TestRequestHandler::CreateSuccessfulResponse(R"({"tempF": 71})");
      }));

  TestFuture<std::vector<BirchWeatherItem>> items_future;
  SetItemsCallback(base::BindOnce([](std::vector<BirchWeatherItem> items) {
    ADD_FAILURE() << "Model updated unexpecstedly after shutdown";
  }));
  weather_provider()->RequestBirchDataFetch();

  // Wait to make sure that the request gets processed.
  request_waiter.Run();
  weather_provider()->Shutdown();

  // Flush any tasks potentaly started asynchronously by the provider .
  base::RunLoop().RunUntilIdle();
}

}  // namespace ash
