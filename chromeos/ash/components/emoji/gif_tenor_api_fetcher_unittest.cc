// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gif_tenor_api_fetcher.h"

#include <memory>
#include <string>
#include <vector>

#include "base/test/gmock_expected_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "chromeos/ash/components/emoji/tenor_types.mojom.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "content/public/test/browser_task_environment.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace ash {

namespace {

using ::base::test::ErrorIs;
using ::base::test::ValueIs;
using ::testing::ElementsAre;
using ::testing::FieldsAre;
using ::testing::Pointee;

constexpr char kFakeCategoriesResponse[] = R"json(
  {
    "tags": [
      {
        "image": "url1",
        "name": "#awesome",
        "path": "/v2/path1",
        "searchterm": "awesome"
      },
      {
        "image": "url2",
        "name": "#jk",
        "path": "/v2/path2",
        "searchterm": "jk"
      }
    ]
  }
)json";

constexpr char kFakeGifsResponse[] = R"json(
  {
    "next": "1",
    "results": [
      {
        "id": "0",
        "content_description": "GIF0",
        "media_formats": {
          "gif": {
            "dims": [
              498,
              339
            ],
            "url": "https://tenor.com/view/media.tenor.com/full_url0",
            "preview": ""
          },
          "tinygif": {
            "dims": [
              220,
              150
            ],
            "url": "https://tenor.com/view/media.tenor.com/preview_url0",
            "preview": ""
          },
          "tinygifpreview": {
            "dims": [
              220,
              150
            ],
            "url": "https://tenor.com/view/media.tenor.com/preview_image_url0",
            "preview": ""
          }
        }
      },
      {
        "id": "1",
        "content_description": "GIF1",
        "media_formats": {
          "gif": {
            "dims": [
              498,
              498
            ],
            "url": "https://tenor.com/view/media.tenor.com/full_url1",
            "preview": ""
          },
          "tinygif": {
            "dims": [
              220,
              220
            ],
            "url": "https://tenor.com/view/media.tenor.com/preview_url1",
            "preview": ""
          },
          "tinygifpreview": {
            "dims": [
              220,
              220
            ],
            "url": "https://tenor.com/view/media.tenor.com/preview_image_url1",
            "preview": ""
          }
        }
      }
    ]
  }
)json";

auto IsFakeGifs() {
  return ElementsAre(
      Pointee(FieldsAre(
          "0", "GIF0",
          Pointee(FieldsAre(
              GURL("https://tenor.com/view/media.tenor.com/full_url0"),
              GURL("https://tenor.com/view/media.tenor.com/preview_url0"),
              GURL("https://tenor.com/view/media.tenor.com/"
                   "preview_image_url0"))),
          gfx::Size(220, 150), gfx::Size(498, 339))),
      Pointee(FieldsAre(
          "1", "GIF1",
          Pointee(FieldsAre(
              GURL("https://tenor.com/view/media.tenor.com/full_url1"),
              GURL("https://tenor.com/view/media.tenor.com/preview_url1"),
              GURL("https://tenor.com/view/media.tenor.com/"
                   "preview_image_url1"))),
          gfx::Size(220, 220), gfx::Size(498, 498))));
}
}  // namespace

class FakeEndpointFetcher : public EndpointFetcher {
 public:
  explicit FakeEndpointFetcher(EndpointResponse response)
      : EndpointFetcher(net::DefineNetworkTrafficAnnotation(
            "chromeos_emoji_picker_mock_fetcher",
            R"()")),
        response_(response) {}

  void PerformRequest(EndpointFetcherCallback endpoint_fetcher_callback,
                      const char* key) override {
    std::move(endpoint_fetcher_callback)
        .Run(std::make_unique<EndpointResponse>(response_));
  }

 private:
  EndpointResponse response_;
};

class GifTenorApiFetcherTest : public testing::Test {
 public:
  GifTenorApiFetcherTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP,
                          base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

 protected:
  content::BrowserTaskEnvironment task_environment_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
};

auto CreateFakeEndpointFetcherCreator(EndpointResponse fake_response) {
  return [fake_response](
             scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
             const GURL& url,
             const net::NetworkTrafficAnnotationTag& annotation_tag) {
    return std::make_unique<FakeEndpointFetcher>(fake_response);
  };
}

TEST_F(GifTenorApiFetcherTest, FetchCategories) {
  base::test::TestFuture<
      base::expected<std::vector<std::string>, GifTenorApiFetcher::Error>>
      future;
  GifTenorApiFetcher::FetchCategories(
      CreateFakeEndpointFetcherCreator(EndpointResponse{}), url_loader_factory_,
      future.GetCallback());
  EXPECT_THAT(future.Take(), ErrorIs(GifTenorApiFetcher::Error::kHttpError));

  GifTenorApiFetcher::FetchCategories(
      CreateFakeEndpointFetcherCreator(
          EndpointResponse{.error_type = FetchErrorType::kNetError}),
      url_loader_factory_, future.GetCallback());
  EXPECT_THAT(future.Take(), ErrorIs(GifTenorApiFetcher::Error::kNetError));

  GifTenorApiFetcher::FetchCategories(
      CreateFakeEndpointFetcherCreator(
          EndpointResponse{.response = kFakeCategoriesResponse,
                           .http_status_code = net::HTTP_OK}),
      url_loader_factory_, future.GetCallback());
  EXPECT_THAT(future.Take(), ValueIs(ElementsAre("#awesome", "#jk")));
}

TEST_F(GifTenorApiFetcherTest, FetchFeaturedGifs) {
  base::test::TestFuture<base::expected<tenor::mojom::PaginatedGifResponsesPtr,
                                        GifTenorApiFetcher::Error>>
      future;
  GifTenorApiFetcher::FetchFeaturedGifs(
      CreateFakeEndpointFetcherCreator(EndpointResponse{}), url_loader_factory_,
      "", future.GetCallback());
  EXPECT_THAT(future.Take(), ErrorIs(GifTenorApiFetcher::Error::kHttpError));

  GifTenorApiFetcher::FetchFeaturedGifs(
      CreateFakeEndpointFetcherCreator(
          EndpointResponse{.error_type = FetchErrorType::kNetError}),
      url_loader_factory_, "", future.GetCallback());
  EXPECT_THAT(future.Take(), ErrorIs(GifTenorApiFetcher::Error::kNetError));

  GifTenorApiFetcher::FetchFeaturedGifs(
      CreateFakeEndpointFetcherCreator(EndpointResponse{
          .response = kFakeGifsResponse, .http_status_code = net::HTTP_OK}),
      url_loader_factory_, "", future.GetCallback());
  EXPECT_THAT(future.Take(), ValueIs(Pointee(FieldsAre("1", IsFakeGifs()))));
}

TEST_F(GifTenorApiFetcherTest, FetchGifSearch) {
  base::test::TestFuture<base::expected<tenor::mojom::PaginatedGifResponsesPtr,
                                        GifTenorApiFetcher::Error>>
      future;
  GifTenorApiFetcher::FetchGifSearch(
      CreateFakeEndpointFetcherCreator(EndpointResponse{}), url_loader_factory_,
      "", "", std::nullopt, future.GetCallback());
  EXPECT_THAT(future.Take(), ErrorIs(GifTenorApiFetcher::Error::kHttpError));

  GifTenorApiFetcher::FetchFeaturedGifs(
      CreateFakeEndpointFetcherCreator(
          EndpointResponse{.error_type = FetchErrorType::kNetError}),
      url_loader_factory_, "", future.GetCallback());
  EXPECT_THAT(future.Take(), ErrorIs(GifTenorApiFetcher::Error::kNetError));

  GifTenorApiFetcher::FetchFeaturedGifs(
      CreateFakeEndpointFetcherCreator(EndpointResponse{
          .response = kFakeGifsResponse, .http_status_code = net::HTTP_OK}),
      url_loader_factory_, "", future.GetCallback());
  EXPECT_THAT(future.Take(), ValueIs(Pointee(FieldsAre("1", IsFakeGifs()))));
}

TEST_F(GifTenorApiFetcherTest, FetchGifsByIds) {
  base::test::TestFuture<base::expected<
      std::vector<tenor::mojom::GifResponsePtr>, GifTenorApiFetcher::Error>>
      future;
  GifTenorApiFetcher::FetchGifsByIds(
      CreateFakeEndpointFetcherCreator(EndpointResponse{}), url_loader_factory_,
      std::vector<std::string>(), future.GetCallback());
  EXPECT_THAT(future.Take(), ErrorIs(GifTenorApiFetcher::Error::kHttpError));

  GifTenorApiFetcher::FetchGifsByIds(
      CreateFakeEndpointFetcherCreator(
          EndpointResponse{.error_type = FetchErrorType::kNetError}),
      url_loader_factory_, std::vector<std::string>(), future.GetCallback());
  EXPECT_THAT(future.Take(), ErrorIs(GifTenorApiFetcher::Error::kNetError));

  GifTenorApiFetcher::FetchGifsByIds(
      CreateFakeEndpointFetcherCreator(EndpointResponse{
          .response = kFakeGifsResponse, .http_status_code = net::HTTP_OK}),
      url_loader_factory_, std::vector<std::string>(), future.GetCallback());
  EXPECT_THAT(future.Take(), ValueIs(IsFakeGifs()));
}

}  // namespace ash
