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
#include "content/public/test/browser_task_environment.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
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

class GifTenorApiFetcherTest : public testing::Test {
 public:
  GifTenorApiFetcherTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP,
                          base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SimulateResponse(net::Error net_error,
                        net::HttpStatusCode http_status_code,
                        std::string_view contents) {
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        GURL(), network::URLLoaderCompletionStatus(net_error),
        network::CreateURLResponseHead(http_status_code), contents,
        static_cast<network::TestURLLoaderFactory::ResponseMatchFlags>(
            network::TestURLLoaderFactory::kUrlMatchPrefix |
            network::TestURLLoaderFactory::kWaitForRequest));
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
};

TEST_F(GifTenorApiFetcherTest, FetchCategories) {
  base::test::TestFuture<
      base::expected<std::vector<std::string>, GifTenorApiFetcher::Error>>
      future;
  GifTenorApiFetcher::FetchCategories(
      test_url_loader_factory_.GetSafeWeakWrapper(), future.GetCallback());
  SimulateResponse(net::OK, net::HTTP_NOT_FOUND, "");
  EXPECT_THAT(future.Take(), ErrorIs(GifTenorApiFetcher::Error::kHttpError));

  GifTenorApiFetcher::FetchCategories(
      test_url_loader_factory_.GetSafeWeakWrapper(), future.GetCallback());
  SimulateResponse(net::ERR_FAILED, net::HTTP_NOT_FOUND, "");
  EXPECT_THAT(future.Take(), ErrorIs(GifTenorApiFetcher::Error::kNetError));

  GifTenorApiFetcher::FetchCategories(
      test_url_loader_factory_.GetSafeWeakWrapper(), future.GetCallback());
  SimulateResponse(net::OK, net::HTTP_OK, kFakeCategoriesResponse);
  EXPECT_THAT(future.Take(), ValueIs(ElementsAre("#awesome", "#jk")));
}

TEST_F(GifTenorApiFetcherTest, FetchFeaturedGifs) {
  base::test::TestFuture<base::expected<tenor::mojom::PaginatedGifResponsesPtr,
                                        GifTenorApiFetcher::Error>>
      future;
  GifTenorApiFetcher::FetchFeaturedGifs(
      test_url_loader_factory_.GetSafeWeakWrapper(), "", future.GetCallback());
  SimulateResponse(net::OK, net::HTTP_NOT_FOUND, "");
  EXPECT_THAT(future.Take(), ErrorIs(GifTenorApiFetcher::Error::kHttpError));

  GifTenorApiFetcher::FetchFeaturedGifs(
      test_url_loader_factory_.GetSafeWeakWrapper(), "", future.GetCallback());
  SimulateResponse(net::ERR_FAILED, net::HTTP_NOT_FOUND, "");
  EXPECT_THAT(future.Take(), ErrorIs(GifTenorApiFetcher::Error::kNetError));

  GifTenorApiFetcher::FetchFeaturedGifs(
      test_url_loader_factory_.GetSafeWeakWrapper(), "", future.GetCallback());
  SimulateResponse(net::OK, net::HTTP_OK, kFakeGifsResponse);
  EXPECT_THAT(future.Take(), ValueIs(Pointee(FieldsAre("1", IsFakeGifs()))));
}

TEST_F(GifTenorApiFetcherTest, FetchGifSearch) {
  base::test::TestFuture<base::expected<tenor::mojom::PaginatedGifResponsesPtr,
                                        GifTenorApiFetcher::Error>>
      future;
  GifTenorApiFetcher::FetchGifSearch(
      test_url_loader_factory_.GetSafeWeakWrapper(), "", "", std::nullopt,
      future.GetCallback());
  SimulateResponse(net::OK, net::HTTP_NOT_FOUND, "");
  EXPECT_THAT(future.Take(), ErrorIs(GifTenorApiFetcher::Error::kHttpError));

  GifTenorApiFetcher::FetchFeaturedGifs(
      test_url_loader_factory_.GetSafeWeakWrapper(), "", future.GetCallback());
  SimulateResponse(net::ERR_FAILED, net::HTTP_NOT_FOUND, "");
  EXPECT_THAT(future.Take(), ErrorIs(GifTenorApiFetcher::Error::kNetError));

  GifTenorApiFetcher::FetchFeaturedGifs(
      test_url_loader_factory_.GetSafeWeakWrapper(), "", future.GetCallback());
  SimulateResponse(net::OK, net::HTTP_OK, kFakeGifsResponse);
  EXPECT_THAT(future.Take(), ValueIs(Pointee(FieldsAre("1", IsFakeGifs()))));
}

TEST_F(GifTenorApiFetcherTest, FetchGifsByIds) {
  base::test::TestFuture<base::expected<
      std::vector<tenor::mojom::GifResponsePtr>, GifTenorApiFetcher::Error>>
      future;
  GifTenorApiFetcher::FetchGifsByIds(
      test_url_loader_factory_.GetSafeWeakWrapper(), std::vector<std::string>(),
      future.GetCallback());
  SimulateResponse(net::OK, net::HTTP_NOT_FOUND, "");
  EXPECT_THAT(future.Take(), ErrorIs(GifTenorApiFetcher::Error::kHttpError));

  GifTenorApiFetcher::FetchGifsByIds(
      test_url_loader_factory_.GetSafeWeakWrapper(), std::vector<std::string>(),
      future.GetCallback());
  SimulateResponse(net::ERR_FAILED, net::HTTP_NOT_FOUND, "");
  EXPECT_THAT(future.Take(), ErrorIs(GifTenorApiFetcher::Error::kNetError));

  GifTenorApiFetcher::FetchGifsByIds(
      test_url_loader_factory_.GetSafeWeakWrapper(), std::vector<std::string>(),
      future.GetCallback());
  SimulateResponse(net::OK, net::HTTP_OK, kFakeGifsResponse);
  EXPECT_THAT(future.Take(), ValueIs(IsFakeGifs()));
}

}  // namespace ash
