// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/drivefs/drivefs_search.h"

#include <utility>

#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "chromeos/components/drivefs/mojom/drivefs.mojom-test-utils.h"
#include "chromeos/components/drivefs/mojom/drivefs.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drivefs {
namespace {

using testing::_;

class MockDriveFs : public mojom::DriveFsInterceptorForTesting,
                    public mojom::SearchQuery {
 public:
  MockDriveFs() = default;

  DriveFs* GetForwardingInterface() override {
    NOTREACHED();
    return nullptr;
  }

  MOCK_CONST_METHOD1(OnStartSearchQuery, void(const mojom::QueryParameters&));
  void StartSearchQuery(mojo::PendingReceiver<mojom::SearchQuery> receiver,
                        mojom::QueryParametersPtr query_params) override {
    search_receiver_.reset();
    OnStartSearchQuery(*query_params);
    search_receiver_.Bind(std::move(receiver));
  }

  MOCK_METHOD1(OnGetNextPage,
               drive::FileError(
                   base::Optional<std::vector<mojom::QueryItemPtr>>* items));

  void GetNextPage(GetNextPageCallback callback) override {
    base::Optional<std::vector<mojom::QueryItemPtr>> items;
    auto error = OnGetNextPage(&items);
    std::move(callback).Run(error, std::move(items));
  }

 private:
  mojo::Receiver<mojom::SearchQuery> search_receiver_{this};
  DISALLOW_COPY_AND_ASSIGN(MockDriveFs);
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

}  // namespace

ACTION_P(PopulateSearch, count) {
  if (count < 0)
    return;
  std::vector<mojom::QueryItemPtr> items;
  for (int i = 0; i < count; ++i) {
    items.emplace_back(mojom::QueryItem::New());
    items.back()->metadata = mojom::FileMetadata::New();
    items.back()->metadata->capabilities = mojom::Capabilities::New();
  }
  *arg0 = std::move(items);
}

MATCHER_P5(MatchQuery, source, text, title, shared, offline, "") {
  if (arg.query_source != source)
    return false;
  if (text != nullptr) {
    if (!arg.text_content || *arg.text_content != std::string(text))
      return false;
  } else {
    if (arg.text_content)
      return false;
  }
  if (title != nullptr) {
    if (!arg.title || *arg.title != std::string(title))
      return false;
  } else {
    if (arg.title)
      return false;
  }
  return arg.shared_with_me == shared && arg.available_offline == offline;
}

TEST_F(DriveFsSearchTest, Search) {
  DriveFsSearch search(&mock_drivefs_, network_connection_tracker_.get(),
                       &clock_);

  EXPECT_CALL(mock_drivefs_, OnStartSearchQuery(_));
  EXPECT_CALL(mock_drivefs_, OnGetNextPage(_))
      .WillOnce(testing::DoAll(
          PopulateSearch(3), testing::Return(drive::FileError::FILE_ERROR_OK)));

  mojom::QueryParametersPtr params = mojom::QueryParameters::New();
  params->query_source = mojom::QueryParameters::QuerySource::kLocalOnly;

  bool called = false;
  mojom::QueryParameters::QuerySource source = search.PerformSearch(
      std::move(params),
      base::BindLambdaForTesting(
          [&called](drive::FileError err,
                    base::Optional<std::vector<mojom::QueryItemPtr>> items) {
            called = true;
            EXPECT_EQ(drive::FileError::FILE_ERROR_OK, err);
            EXPECT_EQ(3u, items->size());
          }));
  EXPECT_EQ(mojom::QueryParameters::QuerySource::kLocalOnly, source);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);
}

TEST_F(DriveFsSearchTest, Search_Fail) {
  DriveFsSearch search(&mock_drivefs_, network_connection_tracker_.get(),
                       &clock_);

  EXPECT_CALL(mock_drivefs_, OnStartSearchQuery(_));
  EXPECT_CALL(mock_drivefs_, OnGetNextPage(_))
      .WillOnce(testing::Return(drive::FileError::FILE_ERROR_ACCESS_DENIED));

  mojom::QueryParametersPtr params = mojom::QueryParameters::New();
  params->query_source = mojom::QueryParameters::QuerySource::kCloudOnly;

  bool called = false;
  mojom::QueryParameters::QuerySource source = search.PerformSearch(
      std::move(params),
      base::BindLambdaForTesting(
          [&called](drive::FileError err,
                    base::Optional<std::vector<mojom::QueryItemPtr>> items) {
            called = true;
            EXPECT_EQ(drive::FileError::FILE_ERROR_ACCESS_DENIED, err);
          }));
  EXPECT_EQ(mojom::QueryParameters::QuerySource::kCloudOnly, source);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);
}

TEST_F(DriveFsSearchTest, Search_OnlineToOffline) {
  DriveFsSearch search(&mock_drivefs_, network_connection_tracker_.get(),
                       &clock_);

  network_connection_tracker_->SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_NONE);

  EXPECT_CALL(mock_drivefs_, OnStartSearchQuery(_));
  EXPECT_CALL(mock_drivefs_, OnGetNextPage(_))
      .WillOnce(testing::DoAll(
          PopulateSearch(3), testing::Return(drive::FileError::FILE_ERROR_OK)));

  mojom::QueryParametersPtr params = mojom::QueryParameters::New();
  params->query_source = mojom::QueryParameters::QuerySource::kCloudOnly;

  bool called = false;
  mojom::QueryParameters::QuerySource source = search.PerformSearch(
      std::move(params),
      base::BindLambdaForTesting(
          [&called](drive::FileError err,
                    base::Optional<std::vector<mojom::QueryItemPtr>> items) {
            called = true;
            EXPECT_EQ(drive::FileError::FILE_ERROR_OK, err);
            EXPECT_EQ(3u, items->size());
          }));
  EXPECT_EQ(mojom::QueryParameters::QuerySource::kLocalOnly, source);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);
}

TEST_F(DriveFsSearchTest, Search_OnlineToOfflineFallback) {
  DriveFsSearch search(&mock_drivefs_, network_connection_tracker_.get(),
                       &clock_);

  EXPECT_CALL(mock_drivefs_,
              OnStartSearchQuery(
                  MatchQuery(mojom::QueryParameters::QuerySource::kCloudOnly,
                             "foobar", nullptr, false, false)));
  EXPECT_CALL(mock_drivefs_,
              OnStartSearchQuery(
                  MatchQuery(mojom::QueryParameters::QuerySource::kLocalOnly,
                             nullptr, "foobar", false, false)));

  EXPECT_CALL(mock_drivefs_, OnGetNextPage(_))
      .WillOnce(testing::Return(drive::FileError::FILE_ERROR_NO_CONNECTION))
      .WillOnce(testing::DoAll(
          PopulateSearch(3), testing::Return(drive::FileError::FILE_ERROR_OK)));

  mojom::QueryParametersPtr params = mojom::QueryParameters::New();
  params->query_source = mojom::QueryParameters::QuerySource::kCloudOnly;
  params->text_content = "foobar";

  bool called = false;
  mojom::QueryParameters::QuerySource source = search.PerformSearch(
      std::move(params),
      base::BindLambdaForTesting(
          [&called](drive::FileError err,
                    base::Optional<std::vector<mojom::QueryItemPtr>> items) {
            called = true;
            EXPECT_EQ(drive::FileError::FILE_ERROR_OK, err);
            EXPECT_EQ(3u, items->size());
          }));
  EXPECT_EQ(mojom::QueryParameters::QuerySource::kCloudOnly, source);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);
}

TEST_F(DriveFsSearchTest, Search_SharedWithMeCaching) {
  DriveFsSearch search(&mock_drivefs_, network_connection_tracker_.get(),
                       &clock_);

  EXPECT_CALL(mock_drivefs_,
              OnStartSearchQuery(
                  MatchQuery(mojom::QueryParameters::QuerySource::kCloudOnly,
                             nullptr, nullptr, true, false)))
      .Times(2);
  EXPECT_CALL(mock_drivefs_,
              OnStartSearchQuery(
                  MatchQuery(mojom::QueryParameters::QuerySource::kLocalOnly,
                             nullptr, nullptr, true, false)))
      .Times(1);

  EXPECT_CALL(mock_drivefs_, OnGetNextPage(_))
      .WillOnce(testing::DoAll(
          PopulateSearch(3), testing::Return(drive::FileError::FILE_ERROR_OK)))
      .WillOnce(testing::DoAll(
          PopulateSearch(3), testing::Return(drive::FileError::FILE_ERROR_OK)))
      .WillOnce(testing::DoAll(
          PopulateSearch(3), testing::Return(drive::FileError::FILE_ERROR_OK)));

  mojom::QueryParametersPtr params = mojom::QueryParameters::New();
  params->query_source = mojom::QueryParameters::QuerySource::kCloudOnly;
  params->shared_with_me = true;

  bool called = false;
  mojom::QueryParameters::QuerySource source = search.PerformSearch(
      std::move(params),
      base::BindLambdaForTesting(
          [&called](drive::FileError err,
                    base::Optional<std::vector<mojom::QueryItemPtr>> items) {
            called = true;
            EXPECT_EQ(drive::FileError::FILE_ERROR_OK, err);
            EXPECT_EQ(3u, items->size());
          }));
  EXPECT_EQ(mojom::QueryParameters::QuerySource::kCloudOnly, source);
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
                    base::Optional<std::vector<mojom::QueryItemPtr>> items) {
            called = true;
            EXPECT_EQ(drive::FileError::FILE_ERROR_OK, err);
            EXPECT_EQ(3u, items->size());
          }));
  EXPECT_EQ(mojom::QueryParameters::QuerySource::kLocalOnly, source);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);

  // Time has passed...
  clock_.Advance(base::TimeDelta::FromHours(1));

  params = mojom::QueryParameters::New();
  params->query_source = mojom::QueryParameters::QuerySource::kCloudOnly;
  params->shared_with_me = true;

  called = false;
  source = search.PerformSearch(
      std::move(params),
      base::BindLambdaForTesting(
          [&called](drive::FileError err,
                    base::Optional<std::vector<mojom::QueryItemPtr>> items) {
            called = true;
            EXPECT_EQ(drive::FileError::FILE_ERROR_OK, err);
            EXPECT_EQ(3u, items->size());
          }));
  EXPECT_EQ(mojom::QueryParameters::QuerySource::kCloudOnly, source);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);
}

TEST_F(DriveFsSearchTest, Search_NoErrorCaching) {
  DriveFsSearch search(&mock_drivefs_, network_connection_tracker_.get(),
                       &clock_);

  EXPECT_CALL(mock_drivefs_,
              OnStartSearchQuery(
                  MatchQuery(mojom::QueryParameters::QuerySource::kCloudOnly,
                             nullptr, nullptr, true, false)))
      .Times(2);
  EXPECT_CALL(mock_drivefs_,
              OnStartSearchQuery(
                  MatchQuery(mojom::QueryParameters::QuerySource::kLocalOnly,
                             nullptr, nullptr, true, false)))
      .Times(0);

  EXPECT_CALL(mock_drivefs_, OnGetNextPage(_))
      .WillOnce(
          testing::DoAll(PopulateSearch(0),
                         testing::Return(drive::FileError::FILE_ERROR_FAILED)))
      .WillOnce(testing::DoAll(
          PopulateSearch(3), testing::Return(drive::FileError::FILE_ERROR_OK)));

  mojom::QueryParametersPtr params = mojom::QueryParameters::New();
  params->query_source = mojom::QueryParameters::QuerySource::kCloudOnly;
  params->shared_with_me = true;

  bool called = false;
  mojom::QueryParameters::QuerySource source = search.PerformSearch(
      std::move(params),
      base::BindLambdaForTesting(
          [&called](drive::FileError err,
                    base::Optional<std::vector<mojom::QueryItemPtr>>) {
            called = true;
            EXPECT_EQ(drive::FileError::FILE_ERROR_FAILED, err);
          }));
  EXPECT_EQ(mojom::QueryParameters::QuerySource::kCloudOnly, source);
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
                    base::Optional<std::vector<mojom::QueryItemPtr>> items) {
            called = true;
            EXPECT_EQ(drive::FileError::FILE_ERROR_OK, err);
            EXPECT_EQ(3u, items->size());
          }));
  EXPECT_EQ(mojom::QueryParameters::QuerySource::kCloudOnly, source);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);
}

}  // namespace drivefs
