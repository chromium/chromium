// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/drivefs/drivefs_search_query.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/memory/weak_ptr.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chromeos/ash/components/drivefs/drivefs_search_query_delegate.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom.h"
#include "components/drive/file_errors.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drivefs {

namespace {

using ::testing::_;
using ::testing::AllOf;
using ::testing::Eq;
using ::testing::Field;
using ::testing::FieldsAre;
using ::testing::Optional;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::SizeIs;

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

class MockDelegate : public DriveFsSearchQueryDelegate {
 public:
  base::WeakPtr<MockDelegate> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  MOCK_METHOD(bool, IsOffline, (), (override));

  MOCK_METHOD(void, UpdateLastSharedWithMeResponse, (), (override));
  MOCK_METHOD(bool, WithinQueryCacheTtl, (), (override));

  MOCK_METHOD(void,
              StartMojoSearchQuery,
              (mojo::PendingReceiver<mojom::SearchQuery> query,
               mojom::QueryParametersPtr query_params),
              (override));

 private:
  base::WeakPtrFactory<MockDelegate> weak_ptr_factory_{this};
};

class DriveFsSearchQueryTest : public testing::Test {
 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
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

TEST_F(DriveFsSearchQueryTest, Search) {
  testing::NiceMock<MockDelegate> delegate;
  MockMojomQuery mojom_query;

  EXPECT_CALL(delegate, StartMojoSearchQuery)
      .WillOnce([&](mojo::PendingReceiver<mojom::SearchQuery> query,
                    mojom::QueryParametersPtr query_params) {
        mojom_query.Bind(std::move(query));
      });

  mojom::QueryParametersPtr params = mojom::QueryParameters::New();
  params->query_source = mojom::QueryParameters::QuerySource::kLocalOnly;

  DriveFsSearchQuery query(delegate.GetWeakPtr(), std::move(params));
  EXPECT_EQ(query.source(), mojom::QueryParameters::QuerySource::kLocalOnly);

  base::test::TestFuture<drive::FileError,
                         std::optional<std::vector<mojom::QueryItemPtr>>>
      next_page_future;
  query.GetNextPage(next_page_future.GetCallback());

  mojom_query.TakeCallback().Run(drive::FileError::FILE_ERROR_OK,
                                 PopulateSearch(3));

  EXPECT_THAT(next_page_future.Take(),
              FieldsAre(drive::FileError::FILE_ERROR_OK, Optional(SizeIs(3))));
}

TEST_F(DriveFsSearchQueryTest, Search_Fail) {
  testing::NiceMock<MockDelegate> delegate;
  MockMojomQuery mojom_query;

  EXPECT_CALL(delegate, StartMojoSearchQuery)
      .WillOnce([&](mojo::PendingReceiver<mojom::SearchQuery> query,
                    mojom::QueryParametersPtr query_params) {
        mojom_query.Bind(std::move(query));
      });

  mojom::QueryParametersPtr params = mojom::QueryParameters::New();
  params->query_source = mojom::QueryParameters::QuerySource::kCloudOnly;

  DriveFsSearchQuery query(delegate.GetWeakPtr(), std::move(params));
  EXPECT_EQ(query.source(), mojom::QueryParameters::QuerySource::kCloudOnly);

  base::test::TestFuture<drive::FileError,
                         std::optional<std::vector<mojom::QueryItemPtr>>>
      next_page_future;
  query.GetNextPage(next_page_future.GetCallback());

  mojom_query.TakeCallback().Run(drive::FileError::FILE_ERROR_ACCESS_DENIED,
                                 {});

  EXPECT_THAT(next_page_future.Take(),
              FieldsAre(drive::FileError::FILE_ERROR_ACCESS_DENIED, _));
}

TEST_F(DriveFsSearchQueryTest, SearchWithMultiplePages) {
  testing::NiceMock<MockDelegate> delegate;
  MockMojomQuery mojom_query;

  EXPECT_CALL(delegate, StartMojoSearchQuery)
      .WillOnce([&](mojo::PendingReceiver<mojom::SearchQuery> query,
                    mojom::QueryParametersPtr query_params) {
        mojom_query.Bind(std::move(query));
      });

  mojom::QueryParametersPtr params = mojom::QueryParameters::New();
  params->query_source = mojom::QueryParameters::QuerySource::kLocalOnly;

  DriveFsSearchQuery query(delegate.GetWeakPtr(), std::move(params));
  EXPECT_EQ(query.source(), mojom::QueryParameters::QuerySource::kLocalOnly);

  base::test::TestFuture<drive::FileError,
                         std::optional<std::vector<mojom::QueryItemPtr>>>
      next_page_future;
  query.GetNextPage(next_page_future.GetCallback());
  mojom_query.TakeCallback().Run(
      drive::FileError::FILE_ERROR_OK_WITH_MORE_RESULTS, PopulateSearch(3));
  EXPECT_THAT(next_page_future.Take(),
              FieldsAre(drive::FileError::FILE_ERROR_OK_WITH_MORE_RESULTS,
                        Optional(SizeIs(3))));

  query.GetNextPage(next_page_future.GetCallback());
  mojom_query.TakeCallback().Run(
      drive::FileError::FILE_ERROR_OK_WITH_MORE_RESULTS, PopulateSearch(3));
  EXPECT_THAT(next_page_future.Take(),
              FieldsAre(drive::FileError::FILE_ERROR_OK_WITH_MORE_RESULTS,
                        Optional(SizeIs(3))));

  query.GetNextPage(next_page_future.GetCallback());
  mojom_query.TakeCallback().Run(drive::FileError::FILE_ERROR_OK,
                                 PopulateSearch(1));
  EXPECT_THAT(next_page_future.Take(),
              FieldsAre(drive::FileError::FILE_ERROR_OK, Optional(SizeIs(1))));

  query.GetNextPage(next_page_future.GetCallback());
  mojom_query.TakeCallback().Run(drive::FileError::FILE_ERROR_OK,
                                 PopulateSearch(0));
  EXPECT_THAT(next_page_future.Take(),
              FieldsAre(drive::FileError::FILE_ERROR_OK, Optional(SizeIs(0))));
}

TEST_F(DriveFsSearchQueryTest, OnlineToOffline) {
  testing::NiceMock<MockDelegate> delegate;
  MockMojomQuery mojom_query;

  EXPECT_CALL(delegate, IsOffline).WillOnce(Return(true));
  EXPECT_CALL(delegate, StartMojoSearchQuery)
      .WillOnce([&](mojo::PendingReceiver<mojom::SearchQuery> query,
                    mojom::QueryParametersPtr query_params) {
        mojom_query.Bind(std::move(query));
      });

  mojom::QueryParametersPtr params = mojom::QueryParameters::New();
  params->query_source = mojom::QueryParameters::QuerySource::kCloudOnly;

  DriveFsSearchQuery query(delegate.GetWeakPtr(), std::move(params));
  EXPECT_EQ(query.source(), mojom::QueryParameters::QuerySource::kLocalOnly);
}

TEST_F(DriveFsSearchQueryTest, Search_OnlineToOfflineFallbackFirstPage) {
  testing::NiceMock<MockDelegate> delegate;
  MockMojomQuery cloud_mojom_query;
  MockMojomQuery local_mojom_query;

  {
    testing::InSequence seq;
    EXPECT_CALL(
        delegate,
        StartMojoSearchQuery(
            _, Pointee(AllOf(
                   Field("query_source", &mojom::QueryParameters::query_source,
                         mojom::QueryParameters::QuerySource::kCloudOnly),
                   Field("text_content", &mojom::QueryParameters::text_content,
                         "foobar")))))
        .WillOnce([&](mojo::PendingReceiver<mojom::SearchQuery> query,
                      mojom::QueryParametersPtr query_params) {
          cloud_mojom_query.Bind(std::move(query));
        });
    EXPECT_CALL(
        delegate,
        StartMojoSearchQuery(
            _, Pointee(AllOf(
                   Field("query_source", &mojom::QueryParameters::query_source,
                         mojom::QueryParameters::QuerySource::kLocalOnly),
                   Field("title", &mojom::QueryParameters::title, "foobar")))))
        .WillOnce([&](mojo::PendingReceiver<mojom::SearchQuery> query,
                      mojom::QueryParametersPtr query_params) {
          local_mojom_query.Bind(std::move(query));
        });
  }

  mojom::QueryParametersPtr params = mojom::QueryParameters::New();
  params->query_source = mojom::QueryParameters::QuerySource::kCloudOnly;
  params->text_content = "foobar";

  DriveFsSearchQuery query(delegate.GetWeakPtr(), std::move(params));
  EXPECT_EQ(query.source(), mojom::QueryParameters::QuerySource::kCloudOnly);

  base::test::TestFuture<drive::FileError,
                         std::optional<std::vector<mojom::QueryItemPtr>>>
      next_page_future;
  query.GetNextPage(next_page_future.GetCallback());
  cloud_mojom_query.TakeCallback().Run(
      drive::FileError::FILE_ERROR_NO_CONNECTION, {});

  MockMojomQuery::GetNextPageCallback local_mojom_callback =
      local_mojom_query.TakeCallback();
  EXPECT_EQ(query.source(), mojom::QueryParameters::QuerySource::kLocalOnly);
  std::move(local_mojom_callback)
      .Run(drive::FileError::FILE_ERROR_OK, PopulateSearch(3));

  EXPECT_THAT(next_page_future.Take(),
              FieldsAre(drive::FileError::FILE_ERROR_OK, Optional(SizeIs(3))));
}

TEST_F(DriveFsSearchQueryTest,
       Search_OnlineToOfflineFallbackFirstPageAbort_NullDelegate) {
  auto delegate = std::make_unique<testing::NiceMock<MockDelegate>>();
  MockMojomQuery cloud_mojom_query;

  EXPECT_CALL(
      *delegate,
      StartMojoSearchQuery(
          _, Pointee(AllOf(
                 Field("query_source", &mojom::QueryParameters::query_source,
                       mojom::QueryParameters::QuerySource::kCloudOnly),
                 Field("text_content", &mojom::QueryParameters::text_content,
                       "foobar")))))
      .WillOnce([&](mojo::PendingReceiver<mojom::SearchQuery> query,
                    mojom::QueryParametersPtr query_params) {
        cloud_mojom_query.Bind(std::move(query));
      });

  mojom::QueryParametersPtr params = mojom::QueryParameters::New();
  params->query_source = mojom::QueryParameters::QuerySource::kCloudOnly;
  params->text_content = "foobar";

  DriveFsSearchQuery query(delegate->GetWeakPtr(), std::move(params));
  EXPECT_EQ(query.source(), mojom::QueryParameters::QuerySource::kCloudOnly);

  base::test::TestFuture<drive::FileError,
                         std::optional<std::vector<mojom::QueryItemPtr>>>
      next_page_future;
  query.GetNextPage(next_page_future.GetCallback());
  MockMojomQuery::GetNextPageCallback callback =
      cloud_mojom_query.TakeCallback();
  delegate.reset();
  std::move(callback).Run(drive::FileError::FILE_ERROR_NO_CONNECTION,
                          std::nullopt);

  EXPECT_THAT(next_page_future.Take(),
              FieldsAre(drive::FileError::FILE_ERROR_ABORT, Eq(std::nullopt)));
  EXPECT_EQ(query.source(), mojom::QueryParameters::QuerySource::kLocalOnly);
}

TEST_F(DriveFsSearchQueryTest,
       Search_OnlineToOfflineFallbackFirstPageAbort_NoopStartSearchQuery) {
  testing::NiceMock<MockDelegate> delegate;
  MockMojomQuery cloud_mojom_query;

  {
    testing::InSequence seq;
    EXPECT_CALL(
        delegate,
        StartMojoSearchQuery(
            _, Pointee(AllOf(
                   Field("query_source", &mojom::QueryParameters::query_source,
                         mojom::QueryParameters::QuerySource::kCloudOnly),
                   Field("text_content", &mojom::QueryParameters::text_content,
                         "foobar")))))
        .WillOnce([&](mojo::PendingReceiver<mojom::SearchQuery> query,
                      mojom::QueryParametersPtr query_params) {
          cloud_mojom_query.Bind(std::move(query));
        });
    EXPECT_CALL(
        delegate,
        StartMojoSearchQuery(
            _, Pointee(AllOf(
                   Field("query_source", &mojom::QueryParameters::query_source,
                         mojom::QueryParameters::QuerySource::kLocalOnly),
                   Field("title", &mojom::QueryParameters::title, "foobar")))))
        .Times(1);
  }

  mojom::QueryParametersPtr params = mojom::QueryParameters::New();
  params->query_source = mojom::QueryParameters::QuerySource::kCloudOnly;
  params->text_content = "foobar";

  DriveFsSearchQuery query(delegate.GetWeakPtr(), std::move(params));
  EXPECT_EQ(query.source(), mojom::QueryParameters::QuerySource::kCloudOnly);

  base::test::TestFuture<drive::FileError,
                         std::optional<std::vector<mojom::QueryItemPtr>>>
      next_page_future;
  query.GetNextPage(next_page_future.GetCallback());
  cloud_mojom_query.TakeCallback().Run(
      drive::FileError::FILE_ERROR_NO_CONNECTION, {});

  EXPECT_THAT(next_page_future.Take(),
              FieldsAre(drive::FileError::FILE_ERROR_ABORT, Eq(std::nullopt)));
  EXPECT_EQ(query.source(), mojom::QueryParameters::QuerySource::kLocalOnly);
}

TEST_F(DriveFsSearchQueryTest,
       Search_OnlineToOfflineFallbackFirstPageAbort_Disconnected) {
  testing::NiceMock<MockDelegate> delegate;
  MockMojomQuery cloud_mojom_query;
  auto local_mojom_query = std::make_unique<MockMojomQuery>();

  {
    testing::InSequence seq;
    EXPECT_CALL(
        delegate,
        StartMojoSearchQuery(
            _, Pointee(AllOf(
                   Field("query_source", &mojom::QueryParameters::query_source,
                         mojom::QueryParameters::QuerySource::kCloudOnly),
                   Field("text_content", &mojom::QueryParameters::text_content,
                         "foobar")))))
        .WillOnce([&](mojo::PendingReceiver<mojom::SearchQuery> query,
                      mojom::QueryParametersPtr query_params) {
          cloud_mojom_query.Bind(std::move(query));
        });
    EXPECT_CALL(
        delegate,
        StartMojoSearchQuery(
            _, Pointee(AllOf(
                   Field("query_source", &mojom::QueryParameters::query_source,
                         mojom::QueryParameters::QuerySource::kLocalOnly),
                   Field("title", &mojom::QueryParameters::title, "foobar")))))
        .WillOnce([&](mojo::PendingReceiver<mojom::SearchQuery> query,
                      mojom::QueryParametersPtr query_params) {
          CHECK(local_mojom_query);
          local_mojom_query->Bind(std::move(query));
        });
  }

  mojom::QueryParametersPtr params = mojom::QueryParameters::New();
  params->query_source = mojom::QueryParameters::QuerySource::kCloudOnly;
  params->text_content = "foobar";

  DriveFsSearchQuery query(delegate.GetWeakPtr(), std::move(params));
  EXPECT_EQ(query.source(), mojom::QueryParameters::QuerySource::kCloudOnly);

  base::test::TestFuture<drive::FileError,
                         std::optional<std::vector<mojom::QueryItemPtr>>>
      next_page_future;
  query.GetNextPage(next_page_future.GetCallback());
  cloud_mojom_query.TakeCallback().Run(
      drive::FileError::FILE_ERROR_NO_CONNECTION, {});

  MockMojomQuery::GetNextPageCallback local_mojom_callback =
      local_mojom_query->TakeCallback();
  local_mojom_query.reset();

  EXPECT_THAT(next_page_future.Take(),
              FieldsAre(drive::FileError::FILE_ERROR_ABORT, Eq(std::nullopt)));
  EXPECT_EQ(query.source(), mojom::QueryParameters::QuerySource::kLocalOnly);
}

TEST_F(DriveFsSearchQueryTest,
       Search_OnlineToOfflineFallbackErrorThenFirstPage) {
  testing::NiceMock<MockDelegate> delegate;
  MockMojomQuery cloud_mojom_query;
  MockMojomQuery local_mojom_query;

  {
    testing::InSequence seq;
    EXPECT_CALL(
        delegate,
        StartMojoSearchQuery(
            _, Pointee(AllOf(
                   Field("query_source", &mojom::QueryParameters::query_source,
                         mojom::QueryParameters::QuerySource::kCloudOnly),
                   Field("text_content", &mojom::QueryParameters::text_content,
                         "foobar")))))
        .WillOnce([&](mojo::PendingReceiver<mojom::SearchQuery> query,
                      mojom::QueryParametersPtr query_params) {
          cloud_mojom_query.Bind(std::move(query));
        });
    EXPECT_CALL(
        delegate,
        StartMojoSearchQuery(
            _, Pointee(AllOf(
                   Field("query_source", &mojom::QueryParameters::query_source,
                         mojom::QueryParameters::QuerySource::kLocalOnly),
                   Field("title", &mojom::QueryParameters::title, "foobar")))))
        .WillOnce([&](mojo::PendingReceiver<mojom::SearchQuery> query,
                      mojom::QueryParametersPtr query_params) {
          local_mojom_query.Bind(std::move(query));
        });
  }

  mojom::QueryParametersPtr params = mojom::QueryParameters::New();
  params->query_source = mojom::QueryParameters::QuerySource::kCloudOnly;
  params->text_content = "foobar";

  DriveFsSearchQuery query(delegate.GetWeakPtr(), std::move(params));
  EXPECT_EQ(query.source(), mojom::QueryParameters::QuerySource::kCloudOnly);

  base::test::TestFuture<drive::FileError,
                         std::optional<std::vector<mojom::QueryItemPtr>>>
      next_page_future;
  query.GetNextPage(next_page_future.GetCallback());
  cloud_mojom_query.TakeCallback().Run(drive::FileError::FILE_ERROR_IN_USE,
                                       std::nullopt);
  EXPECT_THAT(next_page_future.Take(),
              FieldsAre(drive::FileError::FILE_ERROR_IN_USE, Eq(std::nullopt)));
  EXPECT_EQ(query.source(), mojom::QueryParameters::QuerySource::kCloudOnly);

  query.GetNextPage(next_page_future.GetCallback());
  cloud_mojom_query.TakeCallback().Run(
      drive::FileError::FILE_ERROR_NO_CONNECTION, std::nullopt);

  MockMojomQuery::GetNextPageCallback local_mojom_callback =
      local_mojom_query.TakeCallback();
  EXPECT_EQ(query.source(), mojom::QueryParameters::QuerySource::kLocalOnly);
  std::move(local_mojom_callback)
      .Run(drive::FileError::FILE_ERROR_OK, PopulateSearch(3));

  EXPECT_THAT(next_page_future.Take(),
              FieldsAre(drive::FileError::FILE_ERROR_OK, Optional(SizeIs(3))));
}

TEST_F(DriveFsSearchQueryTest,
       Search_OnlineToOfflineFallbackErrorThenFirstPageAbort_NullDelegate) {
  auto delegate = std::make_unique<testing::NiceMock<MockDelegate>>();
  MockMojomQuery cloud_mojom_query;

  EXPECT_CALL(
      *delegate,
      StartMojoSearchQuery(
          _, Pointee(AllOf(
                 Field("query_source", &mojom::QueryParameters::query_source,
                       mojom::QueryParameters::QuerySource::kCloudOnly),
                 Field("text_content", &mojom::QueryParameters::text_content,
                       "foobar")))))
      .WillOnce([&](mojo::PendingReceiver<mojom::SearchQuery> query,
                    mojom::QueryParametersPtr query_params) {
        cloud_mojom_query.Bind(std::move(query));
      });

  mojom::QueryParametersPtr params = mojom::QueryParameters::New();
  params->query_source = mojom::QueryParameters::QuerySource::kCloudOnly;
  params->text_content = "foobar";

  DriveFsSearchQuery query(delegate->GetWeakPtr(), std::move(params));
  EXPECT_EQ(query.source(), mojom::QueryParameters::QuerySource::kCloudOnly);

  base::test::TestFuture<drive::FileError,
                         std::optional<std::vector<mojom::QueryItemPtr>>>
      next_page_future;
  query.GetNextPage(next_page_future.GetCallback());
  cloud_mojom_query.TakeCallback().Run(drive::FileError::FILE_ERROR_IN_USE,
                                       std::nullopt);
  EXPECT_THAT(next_page_future.Take(),
              FieldsAre(drive::FileError::FILE_ERROR_IN_USE, Eq(std::nullopt)));
  EXPECT_EQ(query.source(), mojom::QueryParameters::QuerySource::kCloudOnly);

  query.GetNextPage(next_page_future.GetCallback());
  MockMojomQuery::GetNextPageCallback callback =
      cloud_mojom_query.TakeCallback();
  delegate.reset();
  std::move(callback).Run(drive::FileError::FILE_ERROR_NO_CONNECTION,
                          std::nullopt);

  EXPECT_THAT(next_page_future.Take(),
              FieldsAre(drive::FileError::FILE_ERROR_ABORT, Eq(std::nullopt)));
  EXPECT_EQ(query.source(), mojom::QueryParameters::QuerySource::kLocalOnly);
}

TEST_F(
    DriveFsSearchQueryTest,
    Search_OnlineToOfflineFallbackErrorThenFirstPageAbort_NoopStartSearchQuery) {
  testing::NiceMock<MockDelegate> delegate;
  MockMojomQuery cloud_mojom_query;

  {
    testing::InSequence seq;
    EXPECT_CALL(
        delegate,
        StartMojoSearchQuery(
            _, Pointee(AllOf(
                   Field("query_source", &mojom::QueryParameters::query_source,
                         mojom::QueryParameters::QuerySource::kCloudOnly),
                   Field("text_content", &mojom::QueryParameters::text_content,
                         "foobar")))))
        .WillOnce([&](mojo::PendingReceiver<mojom::SearchQuery> query,
                      mojom::QueryParametersPtr query_params) {
          cloud_mojom_query.Bind(std::move(query));
        });
    EXPECT_CALL(
        delegate,
        StartMojoSearchQuery(
            _, Pointee(AllOf(
                   Field("query_source", &mojom::QueryParameters::query_source,
                         mojom::QueryParameters::QuerySource::kLocalOnly),
                   Field("title", &mojom::QueryParameters::title, "foobar")))))
        .Times(1);
  }

  mojom::QueryParametersPtr params = mojom::QueryParameters::New();
  params->query_source = mojom::QueryParameters::QuerySource::kCloudOnly;
  params->text_content = "foobar";

  DriveFsSearchQuery query(delegate.GetWeakPtr(), std::move(params));
  EXPECT_EQ(query.source(), mojom::QueryParameters::QuerySource::kCloudOnly);

  base::test::TestFuture<drive::FileError,
                         std::optional<std::vector<mojom::QueryItemPtr>>>
      next_page_future;
  query.GetNextPage(next_page_future.GetCallback());
  cloud_mojom_query.TakeCallback().Run(drive::FileError::FILE_ERROR_IN_USE,
                                       std::nullopt);
  EXPECT_THAT(next_page_future.Take(),
              FieldsAre(drive::FileError::FILE_ERROR_IN_USE, Eq(std::nullopt)));
  EXPECT_EQ(query.source(), mojom::QueryParameters::QuerySource::kCloudOnly);

  query.GetNextPage(next_page_future.GetCallback());
  cloud_mojom_query.TakeCallback().Run(
      drive::FileError::FILE_ERROR_NO_CONNECTION, {});

  EXPECT_THAT(next_page_future.Take(),
              FieldsAre(drive::FileError::FILE_ERROR_ABORT, Eq(std::nullopt)));
  EXPECT_EQ(query.source(), mojom::QueryParameters::QuerySource::kLocalOnly);
}

TEST_F(DriveFsSearchQueryTest,
       Search_OnlineToOfflineFallbackErrorThenFirstPageAbort_Disconnected) {
  testing::NiceMock<MockDelegate> delegate;
  MockMojomQuery cloud_mojom_query;
  auto local_mojom_query = std::make_unique<MockMojomQuery>();

  {
    testing::InSequence seq;
    EXPECT_CALL(
        delegate,
        StartMojoSearchQuery(
            _, Pointee(AllOf(
                   Field("query_source", &mojom::QueryParameters::query_source,
                         mojom::QueryParameters::QuerySource::kCloudOnly),
                   Field("text_content", &mojom::QueryParameters::text_content,
                         "foobar")))))
        .WillOnce([&](mojo::PendingReceiver<mojom::SearchQuery> query,
                      mojom::QueryParametersPtr query_params) {
          cloud_mojom_query.Bind(std::move(query));
        });
    EXPECT_CALL(
        delegate,
        StartMojoSearchQuery(
            _, Pointee(AllOf(
                   Field("query_source", &mojom::QueryParameters::query_source,
                         mojom::QueryParameters::QuerySource::kLocalOnly),
                   Field("title", &mojom::QueryParameters::title, "foobar")))))
        .WillOnce([&](mojo::PendingReceiver<mojom::SearchQuery> query,
                      mojom::QueryParametersPtr query_params) {
          CHECK(local_mojom_query);
          local_mojom_query->Bind(std::move(query));
        });
  }

  mojom::QueryParametersPtr params = mojom::QueryParameters::New();
  params->query_source = mojom::QueryParameters::QuerySource::kCloudOnly;
  params->text_content = "foobar";

  DriveFsSearchQuery query(delegate.GetWeakPtr(), std::move(params));
  EXPECT_EQ(query.source(), mojom::QueryParameters::QuerySource::kCloudOnly);

  base::test::TestFuture<drive::FileError,
                         std::optional<std::vector<mojom::QueryItemPtr>>>
      next_page_future;
  query.GetNextPage(next_page_future.GetCallback());
  cloud_mojom_query.TakeCallback().Run(drive::FileError::FILE_ERROR_IN_USE,
                                       std::nullopt);
  EXPECT_THAT(next_page_future.Take(),
              FieldsAre(drive::FileError::FILE_ERROR_IN_USE, Eq(std::nullopt)));
  EXPECT_EQ(query.source(), mojom::QueryParameters::QuerySource::kCloudOnly);

  query.GetNextPage(next_page_future.GetCallback());
  cloud_mojom_query.TakeCallback().Run(
      drive::FileError::FILE_ERROR_NO_CONNECTION, {});

  MockMojomQuery::GetNextPageCallback local_mojom_callback =
      local_mojom_query->TakeCallback();
  local_mojom_query.reset();

  EXPECT_THAT(next_page_future.Take(),
              FieldsAre(drive::FileError::FILE_ERROR_ABORT, Eq(std::nullopt)));
  EXPECT_EQ(query.source(), mojom::QueryParameters::QuerySource::kLocalOnly);
}

TEST_F(DriveFsSearchQueryTest,
       OnlineSearchDoesNotFallBackToOfflineAfterFirstPage) {
  testing::NiceMock<MockDelegate> delegate;
  MockMojomQuery mojom_query;

  EXPECT_CALL(
      delegate,
      StartMojoSearchQuery(
          _, Pointee(AllOf(
                 Field("query_source", &mojom::QueryParameters::query_source,
                       mojom::QueryParameters::QuerySource::kCloudOnly),
                 Field("text_content", &mojom::QueryParameters::text_content,
                       "foobar")))))
      .WillOnce([&](mojo::PendingReceiver<mojom::SearchQuery> query,
                    mojom::QueryParametersPtr query_params) {
        mojom_query.Bind(std::move(query));
      });

  mojom::QueryParametersPtr params = mojom::QueryParameters::New();
  params->query_source = mojom::QueryParameters::QuerySource::kCloudOnly;
  params->text_content = "foobar";

  DriveFsSearchQuery query(delegate.GetWeakPtr(), std::move(params));
  EXPECT_EQ(query.source(), mojom::QueryParameters::QuerySource::kCloudOnly);

  base::test::TestFuture<drive::FileError,
                         std::optional<std::vector<mojom::QueryItemPtr>>>
      next_page_future;
  query.GetNextPage(next_page_future.GetCallback());
  mojom_query.TakeCallback().Run(
      drive::FileError::FILE_ERROR_OK_WITH_MORE_RESULTS, PopulateSearch(3));
  EXPECT_THAT(next_page_future.Take(),
              FieldsAre(drive::FileError::FILE_ERROR_OK_WITH_MORE_RESULTS,
                        Optional(SizeIs(3))));
  EXPECT_EQ(query.source(), mojom::QueryParameters::QuerySource::kCloudOnly);

  query.GetNextPage(next_page_future.GetCallback());
  mojom_query.TakeCallback().Run(drive::FileError::FILE_ERROR_NO_CONNECTION,
                                 std::nullopt);
  EXPECT_THAT(
      next_page_future.Take(),
      FieldsAre(drive::FileError::FILE_ERROR_NO_CONNECTION, Eq(std::nullopt)));
  EXPECT_EQ(query.source(), mojom::QueryParameters::QuerySource::kCloudOnly);
}

TEST_F(DriveFsSearchQueryTest, SharedWithMeCaching_WithinTtl) {
  testing::NiceMock<MockDelegate> delegate;
  MockMojomQuery mojom_query;

  EXPECT_CALL(delegate, WithinQueryCacheTtl).WillOnce(Return(true));
  EXPECT_CALL(
      delegate,
      StartMojoSearchQuery(
          _, Pointee(AllOf(
                 Field("query_source", &mojom::QueryParameters::query_source,
                       mojom::QueryParameters::QuerySource::kLocalOnly),
                 Field("shared_with_me",
                       &mojom::QueryParameters::shared_with_me, true)))))
      .WillOnce([&](mojo::PendingReceiver<mojom::SearchQuery> query,
                    mojom::QueryParametersPtr query_params) {
        mojom_query.Bind(std::move(query));
      });

  mojom::QueryParametersPtr params = mojom::QueryParameters::New();
  params->query_source = mojom::QueryParameters::QuerySource::kCloudOnly;
  params->shared_with_me = true;

  DriveFsSearchQuery query(delegate.GetWeakPtr(), std::move(params));
  EXPECT_EQ(query.source(), mojom::QueryParameters::QuerySource::kLocalOnly);
}

TEST_F(DriveFsSearchQueryTest, SharedWithMeCaching_NotWithinTtl) {
  testing::NiceMock<MockDelegate> delegate;
  MockMojomQuery mojom_query;

  EXPECT_CALL(delegate, WithinQueryCacheTtl).WillOnce(Return(false));
  EXPECT_CALL(
      delegate,
      StartMojoSearchQuery(
          _, Pointee(AllOf(
                 Field("query_source", &mojom::QueryParameters::query_source,
                       mojom::QueryParameters::QuerySource::kCloudOnly),
                 Field("shared_with_me",
                       &mojom::QueryParameters::shared_with_me, true)))))
      .WillOnce([&](mojo::PendingReceiver<mojom::SearchQuery> query,
                    mojom::QueryParametersPtr query_params) {
        mojom_query.Bind(std::move(query));
      });

  mojom::QueryParametersPtr params = mojom::QueryParameters::New();
  params->query_source = mojom::QueryParameters::QuerySource::kCloudOnly;
  params->shared_with_me = true;

  DriveFsSearchQuery query(delegate.GetWeakPtr(), std::move(params));
  EXPECT_EQ(query.source(), mojom::QueryParameters::QuerySource::kCloudOnly);
}

TEST_F(DriveFsSearchQueryTest, Search_UpdatesLastShareWithMeResponseOnSuccess) {
  testing::NiceMock<MockDelegate> delegate;
  MockMojomQuery mojom_query;

  EXPECT_CALL(delegate, WithinQueryCacheTtl).WillOnce(Return(false));
  EXPECT_CALL(delegate, UpdateLastSharedWithMeResponse).Times(1);
  EXPECT_CALL(
      delegate,
      StartMojoSearchQuery(
          _, Pointee(AllOf(
                 Field("query_source", &mojom::QueryParameters::query_source,
                       mojom::QueryParameters::QuerySource::kCloudOnly),
                 Field("shared_with_me",
                       &mojom::QueryParameters::shared_with_me, true)))))
      .WillOnce([&](mojo::PendingReceiver<mojom::SearchQuery> query,
                    mojom::QueryParametersPtr query_params) {
        mojom_query.Bind(std::move(query));
      });

  mojom::QueryParametersPtr params = mojom::QueryParameters::New();
  params->query_source = mojom::QueryParameters::QuerySource::kCloudOnly;
  params->shared_with_me = true;

  DriveFsSearchQuery query(delegate.GetWeakPtr(), std::move(params));
  EXPECT_EQ(query.source(), mojom::QueryParameters::QuerySource::kCloudOnly);
  base::test::TestFuture<drive::FileError,
                         std::optional<std::vector<mojom::QueryItemPtr>>>
      next_page_future;
  query.GetNextPage(next_page_future.GetCallback());
  mojom_query.TakeCallback().Run(drive::FileError::FILE_ERROR_OK,
                                 PopulateSearch(3));

  EXPECT_THAT(next_page_future.Take(),
              FieldsAre(drive::FileError::FILE_ERROR_OK, Optional(SizeIs(3))));
}

TEST_F(DriveFsSearchQueryTest,
       Search_DoesNotUpdatesLastShareWithMeResponseOnError) {
  testing::NiceMock<MockDelegate> delegate;
  MockMojomQuery mojom_query;

  EXPECT_CALL(delegate, WithinQueryCacheTtl).WillOnce(Return(false));
  EXPECT_CALL(delegate, UpdateLastSharedWithMeResponse).Times(0);
  EXPECT_CALL(
      delegate,
      StartMojoSearchQuery(
          _, Pointee(AllOf(
                 Field("query_source", &mojom::QueryParameters::query_source,
                       mojom::QueryParameters::QuerySource::kCloudOnly),
                 Field("shared_with_me",
                       &mojom::QueryParameters::shared_with_me, true)))))
      .WillOnce([&](mojo::PendingReceiver<mojom::SearchQuery> query,
                    mojom::QueryParametersPtr query_params) {
        mojom_query.Bind(std::move(query));
      });

  mojom::QueryParametersPtr params = mojom::QueryParameters::New();
  params->query_source = mojom::QueryParameters::QuerySource::kCloudOnly;
  params->shared_with_me = true;

  DriveFsSearchQuery query(delegate.GetWeakPtr(), std::move(params));
  EXPECT_EQ(query.source(), mojom::QueryParameters::QuerySource::kCloudOnly);
  base::test::TestFuture<drive::FileError,
                         std::optional<std::vector<mojom::QueryItemPtr>>>
      next_page_future;
  query.GetNextPage(next_page_future.GetCallback());
  mojom_query.TakeCallback().Run(drive::FileError::FILE_ERROR_FAILED, {});

  EXPECT_THAT(next_page_future.Take(),
              FieldsAre(drive::FileError::FILE_ERROR_FAILED, _));
}

TEST_F(DriveFsSearchQueryTest,
       GetNextPageCallsCallbackOnUnboundByNullDelegate) {
  mojom::QueryParametersPtr params = mojom::QueryParameters::New();
  params->query_source = mojom::QueryParameters::QuerySource::kLocalOnly;

  DriveFsSearchQuery query(nullptr, std::move(params));
  EXPECT_EQ(query.source(), mojom::QueryParameters::QuerySource::kLocalOnly);

  base::test::TestFuture<drive::FileError,
                         std::optional<std::vector<mojom::QueryItemPtr>>>
      next_page_future;
  query.GetNextPage(next_page_future.GetCallback());

  EXPECT_THAT(next_page_future.Take(),
              FieldsAre(drive::FileError::FILE_ERROR_ABORT, Eq(std::nullopt)));
}

TEST_F(DriveFsSearchQueryTest,
       GetNextPageCallsCallbackOnUnboundByNoOpStartSearchQuery) {
  testing::NiceMock<MockDelegate> delegate;

  EXPECT_CALL(delegate, StartMojoSearchQuery).Times(1);

  mojom::QueryParametersPtr params = mojom::QueryParameters::New();
  params->query_source = mojom::QueryParameters::QuerySource::kLocalOnly;

  DriveFsSearchQuery query(delegate.GetWeakPtr(), std::move(params));
  EXPECT_EQ(query.source(), mojom::QueryParameters::QuerySource::kLocalOnly);

  base::test::TestFuture<drive::FileError,
                         std::optional<std::vector<mojom::QueryItemPtr>>>
      next_page_future;
  query.GetNextPage(next_page_future.GetCallback());

  EXPECT_THAT(next_page_future.Take(),
              FieldsAre(drive::FileError::FILE_ERROR_ABORT, Eq(std::nullopt)));
}

TEST_F(DriveFsSearchQueryTest, GetNextPageCallsCallbackOnDisconnect) {
  testing::NiceMock<MockDelegate> delegate;
  auto mojom_query = std::make_unique<MockMojomQuery>();

  EXPECT_CALL(delegate, StartMojoSearchQuery)
      .WillOnce([&](mojo::PendingReceiver<mojom::SearchQuery> query,
                    mojom::QueryParametersPtr query_params) {
        CHECK(mojom_query);
        mojom_query->Bind(std::move(query));
      });

  mojom::QueryParametersPtr params = mojom::QueryParameters::New();
  params->query_source = mojom::QueryParameters::QuerySource::kLocalOnly;

  DriveFsSearchQuery query(delegate.GetWeakPtr(), std::move(params));
  EXPECT_EQ(query.source(), mojom::QueryParameters::QuerySource::kLocalOnly);

  base::test::TestFuture<drive::FileError,
                         std::optional<std::vector<mojom::QueryItemPtr>>>
      next_page_future;
  query.GetNextPage(next_page_future.GetCallback());
  mojom_query.reset();

  EXPECT_THAT(next_page_future.Take(),
              FieldsAre(drive::FileError::FILE_ERROR_ABORT, Eq(std::nullopt)));
}

}  // namespace
}  // namespace drivefs
