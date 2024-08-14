// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/drivefs/drivefs_search.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom-test-utils.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom.h"
#include "components/drive/file_errors.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drivefs {
namespace {

using ::testing::_;
using ::testing::FieldsAre;

class MockMojomQuery : public mojom::SearchQuery {
 public:
  void GetNextPage(GetNextPageCallback callback) override {
    future_.SetValue(std::move(callback));
  }

  void Bind(mojo::PendingReceiver<mojom::SearchQuery> receiver) {
    search_receiver_.Bind(std::move(receiver));
  }

  GetNextPageCallback TakeCallback() { return future_.Take(); }

 private:
  base::test::TestFuture<GetNextPageCallback> future_;
  mojo::Receiver<mojom::SearchQuery> search_receiver_{this};
};

class MockDriveFs : public mojom::DriveFsInterceptorForTesting {
 public:
  MockDriveFs() = default;

  MockDriveFs(const MockDriveFs&) = delete;
  MockDriveFs& operator=(const MockDriveFs&) = delete;

  DriveFs* GetForwardingInterface() override {
    NOTREACHED_IN_MIGRATION();
    return nullptr;
  }

  MOCK_METHOD(void,
              StartSearchQuery,
              (mojo::PendingReceiver<mojom::SearchQuery> query,
               mojom::QueryParametersPtr query_params),
              (override));
};

class DriveFsSearchTest : public testing::Test {
 public:
  DriveFsSearchTest()
      : network_connection_tracker_(
            network::TestNetworkConnectionTracker::CreateInstance()) {
    clock_.SetNow(base::Time::Now());
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<network::TestNetworkConnectionTracker>
      network_connection_tracker_;
  MockDriveFs mock_drivefs_;
  base::SimpleTestClock clock_;
};

std::vector<mojom::QueryItemPtr> PopulateSearch(int count) {
  std::vector<mojom::QueryItemPtr> items;
  for (int i = 0; i < count; ++i) {
    items.emplace_back(mojom::QueryItem::New());
    items.back()->metadata = mojom::FileMetadata::New();
    items.back()->metadata->capabilities = mojom::Capabilities::New();
  }
  return items;
}

}  // namespace

MATCHER_P5(MatchQuery, source, text, title, shared, offline, "") {
  if (arg->query_source != source) {
    return false;
  }
  if (text != nullptr) {
    if (!arg->text_content || *arg->text_content != std::string(text)) {
      return false;
    }
  } else {
    if (arg->text_content) {
      return false;
    }
  }
  if (title != nullptr) {
    if (!arg->title || *arg->title != std::string(title)) {
      return false;
    }
  } else {
    if (arg->title) {
      return false;
    }
  }
  return arg->shared_with_me == shared && arg->available_offline == offline;
}

TEST_F(DriveFsSearchTest, Search) {
  DriveFsSearch search(&mock_drivefs_, network_connection_tracker_.get(),
                       &clock_);

  MockMojomQuery mojom_query;
  EXPECT_CALL(mock_drivefs_, StartSearchQuery)
      .WillOnce([&](mojo::PendingReceiver<mojom::SearchQuery> query,
                    mojom::QueryParametersPtr query_params) {
        mojom_query.Bind(std::move(query));
      });

  mojom::QueryParametersPtr params = mojom::QueryParameters::New();
  params->query_source = mojom::QueryParameters::QuerySource::kLocalOnly;

  bool called = false;
  mojom::QueryParameters::QuerySource source = search.PerformSearch(
      std::move(params),
      base::BindLambdaForTesting(
          [&called](drive::FileError err,
                    std::optional<std::vector<mojom::QueryItemPtr>> items) {
            called = true;
            EXPECT_EQ(drive::FileError::FILE_ERROR_OK, err);
            EXPECT_EQ(3u, items->size());
          }));
  EXPECT_EQ(mojom::QueryParameters::QuerySource::kLocalOnly, source);
  mojom_query.TakeCallback().Run(drive::FileError::FILE_ERROR_OK,
                                 PopulateSearch(3));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);
}

TEST_F(DriveFsSearchTest, Search_Fail) {
  DriveFsSearch search(&mock_drivefs_, network_connection_tracker_.get(),
                       &clock_);

  MockMojomQuery mojom_query;
  EXPECT_CALL(mock_drivefs_, StartSearchQuery)
      .WillOnce([&](mojo::PendingReceiver<mojom::SearchQuery> query,
                    mojom::QueryParametersPtr query_params) {
        mojom_query.Bind(std::move(query));
      });

  mojom::QueryParametersPtr params = mojom::QueryParameters::New();
  params->query_source = mojom::QueryParameters::QuerySource::kCloudOnly;

  bool called = false;
  mojom::QueryParameters::QuerySource source = search.PerformSearch(
      std::move(params),
      base::BindLambdaForTesting(
          [&called](drive::FileError err,
                    std::optional<std::vector<mojom::QueryItemPtr>> items) {
            called = true;
            EXPECT_EQ(drive::FileError::FILE_ERROR_ACCESS_DENIED, err);
          }));
  EXPECT_EQ(mojom::QueryParameters::QuerySource::kCloudOnly, source);
  mojom_query.TakeCallback().Run(drive::FileError::FILE_ERROR_ACCESS_DENIED,
                                 std::nullopt);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);
}

TEST_F(DriveFsSearchTest, Search_OnlineToOffline) {
  DriveFsSearch search(&mock_drivefs_, network_connection_tracker_.get(),
                       &clock_);

  network_connection_tracker_->SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_NONE);

  MockMojomQuery mojom_query;
  EXPECT_CALL(mock_drivefs_, StartSearchQuery)
      .WillOnce([&](mojo::PendingReceiver<mojom::SearchQuery> query,
                    mojom::QueryParametersPtr query_params) {
        mojom_query.Bind(std::move(query));
      });

  mojom::QueryParametersPtr params = mojom::QueryParameters::New();
  params->query_source = mojom::QueryParameters::QuerySource::kCloudOnly;

  bool called = false;
  mojom::QueryParameters::QuerySource source = search.PerformSearch(
      std::move(params),
      base::BindLambdaForTesting(
          [&called](drive::FileError err,
                    std::optional<std::vector<mojom::QueryItemPtr>> items) {
            called = true;
            EXPECT_EQ(drive::FileError::FILE_ERROR_OK, err);
            EXPECT_EQ(3u, items->size());
          }));
  EXPECT_EQ(mojom::QueryParameters::QuerySource::kLocalOnly, source);
  mojom_query.TakeCallback().Run(drive::FileError::FILE_ERROR_OK,
                                 PopulateSearch(3));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);
}

TEST_F(DriveFsSearchTest, Search_OnlineToOfflineFallback) {
  DriveFsSearch search(&mock_drivefs_, network_connection_tracker_.get(),
                       &clock_);

  MockMojomQuery cloud_mojom_query;
  MockMojomQuery local_mojom_query;
  EXPECT_CALL(mock_drivefs_,
              StartSearchQuery(
                  _, MatchQuery(mojom::QueryParameters::QuerySource::kCloudOnly,
                                "foobar", nullptr, false, false)))
      .WillOnce([&](mojo::PendingReceiver<mojom::SearchQuery> query,
                    mojom::QueryParametersPtr query_params) {
        cloud_mojom_query.Bind(std::move(query));
      });
  EXPECT_CALL(mock_drivefs_,
              StartSearchQuery(
                  _, MatchQuery(mojom::QueryParameters::QuerySource::kLocalOnly,
                                nullptr, "foobar", false, false)))
      .WillOnce([&](mojo::PendingReceiver<mojom::SearchQuery> query,
                    mojom::QueryParametersPtr query_params) {
        local_mojom_query.Bind(std::move(query));
      });

  mojom::QueryParametersPtr params = mojom::QueryParameters::New();
  params->query_source = mojom::QueryParameters::QuerySource::kCloudOnly;
  params->text_content = "foobar";

  bool called = false;
  mojom::QueryParameters::QuerySource source = search.PerformSearch(
      std::move(params),
      base::BindLambdaForTesting(
          [&called](drive::FileError err,
                    std::optional<std::vector<mojom::QueryItemPtr>> items) {
            called = true;
            EXPECT_EQ(drive::FileError::FILE_ERROR_OK, err);
            EXPECT_EQ(3u, items->size());
          }));
  EXPECT_EQ(mojom::QueryParameters::QuerySource::kCloudOnly, source);
  cloud_mojom_query.TakeCallback().Run(
      drive::FileError::FILE_ERROR_NO_CONNECTION, std::nullopt);
  local_mojom_query.TakeCallback().Run(drive::FileError::FILE_ERROR_OK,
                                       PopulateSearch(3));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);
}

TEST_F(DriveFsSearchTest, Search_SharedWithMeCaching) {
  DriveFsSearch search(&mock_drivefs_, network_connection_tracker_.get(),
                       &clock_);

  MockMojomQuery cloud_mojom_query_1;
  MockMojomQuery cloud_mojom_query_2;
  MockMojomQuery local_mojom_query;
  EXPECT_CALL(mock_drivefs_,
              StartSearchQuery(
                  _, MatchQuery(mojom::QueryParameters::QuerySource::kCloudOnly,
                                nullptr, nullptr, true, false)))
      .WillOnce([&](mojo::PendingReceiver<mojom::SearchQuery> query,
                    mojom::QueryParametersPtr query_params) {
        cloud_mojom_query_1.Bind(std::move(query));
      })
      .WillOnce([&](mojo::PendingReceiver<mojom::SearchQuery> query,
                    mojom::QueryParametersPtr query_params) {
        cloud_mojom_query_2.Bind(std::move(query));
      });
  EXPECT_CALL(mock_drivefs_,
              StartSearchQuery(
                  _, MatchQuery(mojom::QueryParameters::QuerySource::kLocalOnly,
                                nullptr, nullptr, true, false)))
      .WillOnce([&](mojo::PendingReceiver<mojom::SearchQuery> query,
                    mojom::QueryParametersPtr query_params) {
        local_mojom_query.Bind(std::move(query));
      });

  mojom::QueryParametersPtr params = mojom::QueryParameters::New();
  params->query_source = mojom::QueryParameters::QuerySource::kCloudOnly;
  params->shared_with_me = true;

  bool called = false;
  mojom::QueryParameters::QuerySource source = search.PerformSearch(
      std::move(params),
      base::BindLambdaForTesting(
          [&called](drive::FileError err,
                    std::optional<std::vector<mojom::QueryItemPtr>> items) {
            called = true;
            EXPECT_EQ(drive::FileError::FILE_ERROR_OK, err);
            EXPECT_EQ(3u, items->size());
          }));
  EXPECT_EQ(mojom::QueryParameters::QuerySource::kCloudOnly, source);
  cloud_mojom_query_1.TakeCallback().Run(drive::FileError::FILE_ERROR_OK,
                                         PopulateSearch(3));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);

  params = mojom::QueryParameters::New();
  params->query_source = mojom::QueryParameters::QuerySource::kCloudOnly;
  params->shared_with_me = true;

  called = false;
  source = search.PerformSearch(
      std::move(params),
      base::BindLambdaForTesting(
          [&called](drive::FileError err,
                    std::optional<std::vector<mojom::QueryItemPtr>> items) {
            called = true;
            EXPECT_EQ(drive::FileError::FILE_ERROR_OK, err);
            EXPECT_EQ(3u, items->size());
          }));
  EXPECT_EQ(mojom::QueryParameters::QuerySource::kLocalOnly, source);
  local_mojom_query.TakeCallback().Run(drive::FileError::FILE_ERROR_OK,
                                       PopulateSearch(3));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);

  // Time has passed...
  clock_.Advance(base::Hours(1));

  params = mojom::QueryParameters::New();
  params->query_source = mojom::QueryParameters::QuerySource::kCloudOnly;
  params->shared_with_me = true;

  called = false;
  source = search.PerformSearch(
      std::move(params),
      base::BindLambdaForTesting(
          [&called](drive::FileError err,
                    std::optional<std::vector<mojom::QueryItemPtr>> items) {
            called = true;
            EXPECT_EQ(drive::FileError::FILE_ERROR_OK, err);
            EXPECT_EQ(3u, items->size());
          }));
  EXPECT_EQ(mojom::QueryParameters::QuerySource::kCloudOnly, source);
  cloud_mojom_query_2.TakeCallback().Run(drive::FileError::FILE_ERROR_OK,
                                         PopulateSearch(3));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);
}

TEST_F(DriveFsSearchTest, Search_NoErrorCaching) {
  DriveFsSearch search(&mock_drivefs_, network_connection_tracker_.get(),
                       &clock_);

  MockMojomQuery mojom_query_1;
  MockMojomQuery mojom_query_2;
  EXPECT_CALL(mock_drivefs_,
              StartSearchQuery(
                  _, MatchQuery(mojom::QueryParameters::QuerySource::kCloudOnly,
                                nullptr, nullptr, true, false)))
      .WillOnce([&](mojo::PendingReceiver<mojom::SearchQuery> query,
                    mojom::QueryParametersPtr query_params) {
        mojom_query_1.Bind(std::move(query));
      })
      .WillOnce([&](mojo::PendingReceiver<mojom::SearchQuery> query,
                    mojom::QueryParametersPtr query_params) {
        mojom_query_2.Bind(std::move(query));
      });

  mojom::QueryParametersPtr params = mojom::QueryParameters::New();
  params->query_source = mojom::QueryParameters::QuerySource::kCloudOnly;
  params->shared_with_me = true;

  bool called = false;
  mojom::QueryParameters::QuerySource source = search.PerformSearch(
      std::move(params),
      base::BindLambdaForTesting(
          [&called](drive::FileError err,
                    std::optional<std::vector<mojom::QueryItemPtr>>) {
            called = true;
            EXPECT_EQ(drive::FileError::FILE_ERROR_FAILED, err);
          }));
  EXPECT_EQ(mojom::QueryParameters::QuerySource::kCloudOnly, source);
  mojom_query_1.TakeCallback().Run(drive::FileError::FILE_ERROR_FAILED,
                                   std::nullopt);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);

  params = mojom::QueryParameters::New();
  params->query_source = mojom::QueryParameters::QuerySource::kCloudOnly;
  params->shared_with_me = true;

  // As previous call failed this one will go to the cloud again.
  called = false;
  source = search.PerformSearch(
      std::move(params),
      base::BindLambdaForTesting(
          [&called](drive::FileError err,
                    std::optional<std::vector<mojom::QueryItemPtr>> items) {
            called = true;
            EXPECT_EQ(drive::FileError::FILE_ERROR_OK, err);
            EXPECT_EQ(3u, items->size());
          }));
  EXPECT_EQ(mojom::QueryParameters::QuerySource::kCloudOnly, source);
  mojom_query_2.TakeCallback().Run(drive::FileError::FILE_ERROR_OK,
                                   PopulateSearch(3));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);
}

TEST_F(DriveFsSearchTest, Search_SearchQueryRemoteDisconnected) {
  DriveFsSearch search(&mock_drivefs_, network_connection_tracker_.get(),
                       &clock_);

  auto mojom_query = std::make_unique<MockMojomQuery>();
  EXPECT_CALL(mock_drivefs_, StartSearchQuery)
      .WillOnce([&](mojo::PendingReceiver<mojom::SearchQuery> query,
                    mojom::QueryParametersPtr query_params) {
        CHECK(mojom_query);
        mojom_query->Bind(std::move(query));
      });

  mojom::QueryParametersPtr params = mojom::QueryParameters::New();
  params->query_source = mojom::QueryParameters::QuerySource::kCloudOnly;

  base::test::TestFuture<drive::FileError,
                         std::optional<std::vector<mojom::QueryItemPtr>>>
      next_page_future;
  mojom::QueryParameters::QuerySource source =
      search.PerformSearch(std::move(params), next_page_future.GetCallback());
  EXPECT_EQ(mojom::QueryParameters::QuerySource::kCloudOnly, source);
  mojom_query.reset();

  EXPECT_THAT(next_page_future.Take(),
              FieldsAre(drive::FileError::FILE_ERROR_ABORT, _));
}

}  // namespace drivefs
