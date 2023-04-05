// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/uuid.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/power_bookmarks/common/power.h"
#include "components/power_bookmarks/common/power_bookmark_observer.h"
#include "components/power_bookmarks/common/power_overview.h"
#include "components/power_bookmarks/common/power_test_util.h"
#include "components/power_bookmarks/common/search_params.h"
#include "components/power_bookmarks/core/power_bookmark_data_provider.h"
#include "components/power_bookmarks/core/power_bookmark_features.h"
#include "components/power_bookmarks/core/power_bookmark_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::IsEmpty;
using testing::IsFalse;
using testing::IsTrue;
using testing::SizeIs;

namespace power_bookmarks {

// Tests for the power bookmark service.
// In-depth tests for the actual storage can be found in
// `power_bookmark_database_unittest`.
class PowerBookmarkServiceTest : public testing::Test {
 protected:
  void SetUp() override {
    test_features_.InitAndEnableFeature(kPowerBookmarkBackend);

    ASSERT_TRUE(temp_directory_.CreateUniqueTempDir());

    model_ = bookmarks::TestBookmarkClient::CreateModel();
    backend_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(), base::TaskPriority::BEST_EFFORT});

    service_ = std::make_unique<PowerBookmarkService>(
        model_.get(), temp_directory_.GetPath(),
        task_environment_.GetMainThreadTaskRunner(), backend_task_runner_);
    RunUntilIdle();
  }

  void TearDown() override {
    ResetService();
    RunUntilIdle();

    EXPECT_TRUE(temp_directory_.Delete());
  }

  void ResetService() { service_.reset(); }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  PowerBookmarkService* service() { return service_.get(); }

  bookmarks::BookmarkModel* model() { return model_.get(); }

  base::HistogramTester* histogram() { return &histogram_; }

 private:
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList test_features_;

  std::unique_ptr<PowerBookmarkService> service_;
  std::unique_ptr<bookmarks::BookmarkModel> model_;

  base::ScopedTempDir temp_directory_;
  scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;
  base::HistogramTester histogram_;
};

class MockDataProvider : public PowerBookmarkDataProvider {
 public:
  MOCK_METHOD2(AttachMetadataForNewBookmark,
               void(const bookmarks::BookmarkNode* node,
                    PowerBookmarkMeta* meta));
};

class MockObserver : public PowerBookmarkObserver {
 public:
  MOCK_METHOD0(OnPowersChanged, void());
};

TEST_F(PowerBookmarkServiceTest, AddDataProvider) {
  MockDataProvider data_provider;
  service()->AddDataProvider(&data_provider);

  EXPECT_CALL(data_provider, AttachMetadataForNewBookmark(_, _));

  model()->AddNewURL(model()->bookmark_bar_node(), 0, u"Title",
                     GURL("https://example.com"));
}

TEST_F(PowerBookmarkServiceTest, RemoveDataProvider) {
  MockDataProvider data_provider;
  service()->AddDataProvider(&data_provider);
  service()->RemoveDataProvider(&data_provider);
  EXPECT_CALL(data_provider, AttachMetadataForNewBookmark(_, _)).Times(0);

  model()->AddNewURL(model()->bookmark_bar_node(), 0, u"Title",
                     GURL("https://example.com"));
}

TEST_F(PowerBookmarkServiceTest, AddDataProviderNoNewBookmark) {
  MockDataProvider data_provider;
  service()->AddDataProvider(&data_provider);
  EXPECT_CALL(data_provider,
              AttachMetadataForNewBookmark(testing::_, testing::_))
      .Times(0);

  // Data providers should only be called when new bookmarks are added.
  model()->AddURL(model()->bookmark_bar_node(), 0, u"Title",
                  GURL("https://example.com"));
}

TEST_F(PowerBookmarkServiceTest, Shutdown) {
  ResetService();
  // No errors should be thrown when shutting down the backend.
}

TEST_F(PowerBookmarkServiceTest, GetPowersForURL) {
  base::MockCallback<SuccessCallback> success_cb;
  EXPECT_CALL(success_cb, Run(IsTrue()));

  service()->CreatePower(
      MakePower(GURL("https://google.com"),
                sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK),
      success_cb.Get());
  RunUntilIdle();

  base::MockCallback<PowersCallback> powers_cb;
  EXPECT_CALL(powers_cb, Run(SizeIs(1)));

  service()->GetPowersForURL(
      GURL("https://google.com"),
      sync_pb::PowerBookmarkSpecifics::POWER_TYPE_UNSPECIFIED, powers_cb.Get());
  RunUntilIdle();
}

TEST_F(PowerBookmarkServiceTest, GetPowerOverviewsForType) {
  base::MockCallback<SuccessCallback> success_cb;
  EXPECT_CALL(success_cb, Run(IsTrue()));
  service()->CreatePower(
      MakePower(GURL("https://google.com"),
                sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK),
      success_cb.Get());
  RunUntilIdle();

  EXPECT_CALL(success_cb, Run(IsTrue()));
  service()->CreatePower(
      MakePower(GURL("https://google.com"),
                sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK),
      success_cb.Get());
  RunUntilIdle();

  EXPECT_CALL(success_cb, Run(IsTrue()));
  service()->CreatePower(
      MakePower(GURL("https://boogle.com"),
                sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK),
      success_cb.Get());
  RunUntilIdle();

  base::MockCallback<PowerOverviewsCallback> cb;
  EXPECT_CALL(cb, Run(SizeIs(2)));

  service()->GetPowerOverviewsForType(
      sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK, cb.Get());
  RunUntilIdle();
}

TEST_F(PowerBookmarkServiceTest, SearchPowers) {
  base::MockCallback<SuccessCallback> success_cb;
  EXPECT_CALL(success_cb, Run(IsTrue())).Times(3);

  service()->CreatePower(
      MakePower(GURL("https://example.com/a1.html"),
                sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK),
      success_cb.Get());
  service()->CreatePower(
      MakePower(GURL("https://example.com/b1.html"),
                sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK),
      success_cb.Get());
  service()->CreatePower(
      MakePower(GURL("https://example.com/a2.html"),
                sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK),
      success_cb.Get());
  RunUntilIdle();

  base::MockCallback<PowersCallback> powers_cb;
  EXPECT_CALL(powers_cb, Run(SizeIs(2)));

  SearchParams search_params{.query = "/a"};
  service()->SearchPowers(search_params, powers_cb.Get());
  RunUntilIdle();
}

TEST_F(PowerBookmarkServiceTest, SearchPowersNoteText) {
  base::MockCallback<SuccessCallback> success_cb;
  EXPECT_CALL(success_cb, Run(IsTrue())).Times(2);

  {
    std::unique_ptr<sync_pb::PowerEntity> note_entity =
        std::make_unique<sync_pb::PowerEntity>();
    note_entity->mutable_note_entity()->set_plain_text("lorem ipsum");
    service()->CreatePower(
        MakePower(GURL("https://example.com/a1.html"),
                  sync_pb::PowerBookmarkSpecifics::POWER_TYPE_NOTE,
                  std::move(note_entity)),
        success_cb.Get());
  }
  {
    std::unique_ptr<sync_pb::PowerEntity> note_entity =
        std::make_unique<sync_pb::PowerEntity>();
    note_entity->mutable_note_entity()->set_plain_text("not a match");
    service()->CreatePower(
        MakePower(GURL("https://example.com/a2.html"),
                  sync_pb::PowerBookmarkSpecifics::POWER_TYPE_NOTE,
                  std::move(note_entity)),
        success_cb.Get());
  }
  RunUntilIdle();

  base::MockCallback<PowersCallback> powers_cb;
  EXPECT_CALL(powers_cb, Run(SizeIs(1)));

  SearchParams search_params{.query = "lorem"};
  service()->SearchPowers(search_params, powers_cb.Get());
  RunUntilIdle();
}

TEST_F(PowerBookmarkServiceTest, SearchPowerOverviews) {
  base::MockCallback<SuccessCallback> success_cb;
  EXPECT_CALL(success_cb, Run(IsTrue())).Times(3);

  service()->CreatePower(
      MakePower(GURL("https://example.com/a1.html"),
                sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK),
      success_cb.Get());
  service()->CreatePower(
      MakePower(GURL("https://example.com/b1.html"),
                sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK),
      success_cb.Get());
  service()->CreatePower(
      MakePower(GURL("https://example.com/a1.html"),
                sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK),
      success_cb.Get());
  RunUntilIdle();

  base::MockCallback<PowerOverviewsCallback> result_cb;
  EXPECT_CALL(result_cb, Run(SizeIs(1)));

  SearchParams search_params{.query = "/a"};
  service()->SearchPowerOverviews(search_params, result_cb.Get());
  RunUntilIdle();
}

TEST_F(PowerBookmarkServiceTest, CreatePower) {
  base::MockCallback<SuccessCallback> cb;
  EXPECT_CALL(cb, Run(IsTrue()));

  service()->CreatePower(
      MakePower(GURL("https://google.com"),
                sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK),
      cb.Get());
  RunUntilIdle();

  histogram()->ExpectBucketCount("PowerBookmarks.PowerCreated.Success", true,
                                 1);
  histogram()->ExpectTotalCount("PowerBookmarks.PowerCreated.Success", 1);
  histogram()->ExpectBucketCount(
      "PowerBookmarks.PowerCreated.PowerType",
      sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK, 1);
  histogram()->ExpectTotalCount("PowerBookmarks.PowerCreated.PowerType", 1);

  PowersCallback powers_cb = base::BindLambdaForTesting(
      [&](std::vector<std::unique_ptr<Power>> powers) {
        ASSERT_EQ(1u, powers.size());
        ASSERT_FALSE(powers[0]->time_added().is_null());
        ASSERT_FALSE(powers[0]->time_modified().is_null());
      });
  service()->GetPowersForURL(
      GURL("https://google.com"),
      sync_pb::PowerBookmarkSpecifics::POWER_TYPE_UNSPECIFIED,
      std::move(powers_cb));
  RunUntilIdle();
}

TEST_F(PowerBookmarkServiceTest, ShouldNotCreatePowerIfPresent) {
  base::MockCallback<SuccessCallback> cb;
  EXPECT_CALL(cb, Run(IsTrue()));
  auto power1 = MakePower(GURL("https://google.com"),
                          sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK);
  auto power2 = power1->Clone();
  service()->CreatePower(std::move(power1), cb.Get());
  RunUntilIdle();

  EXPECT_CALL(cb, Run(IsFalse()));
  service()->CreatePower(std::move(power2), cb.Get());
  RunUntilIdle();

  histogram()->ExpectBucketCount("PowerBookmarks.PowerCreated.Success", false,
                                 1);
  histogram()->ExpectTotalCount("PowerBookmarks.PowerCreated.Success", 2);

  PowersCallback powers_cb = base::BindLambdaForTesting(
      [&](std::vector<std::unique_ptr<Power>> powers) {
        ASSERT_EQ(1u, powers.size());
        ASSERT_FALSE(powers[0]->time_added().is_null());
        ASSERT_FALSE(powers[0]->time_modified().is_null());
      });
  service()->GetPowersForURL(
      GURL("https://google.com"),
      sync_pb::PowerBookmarkSpecifics::POWER_TYPE_UNSPECIFIED,
      std::move(powers_cb));
  RunUntilIdle();
}

TEST_F(PowerBookmarkServiceTest, CreatePowerShouldNotUpdateTimeIfPresent) {
  base::MockCallback<SuccessCallback> cb;
  EXPECT_CALL(cb, Run(IsTrue()));
  auto power1 = MakePower(GURL("https://google.com"),
                          sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK);
  base::Time time = base::Time::FromDeltaSinceWindowsEpoch(base::Days(1));
  power1->set_time_added(time);
  power1->set_time_modified(time);
  service()->CreatePower(std::move(power1), cb.Get());
  RunUntilIdle();

  PowersCallback powers_cb = base::BindLambdaForTesting(
      [&](std::vector<std::unique_ptr<Power>> powers) {
        ASSERT_EQ(1u, powers.size());
        ASSERT_EQ(time, powers[0]->time_added());
        ASSERT_EQ(time, powers[0]->time_modified());
      });
  service()->GetPowersForURL(
      GURL("https://google.com"),
      sync_pb::PowerBookmarkSpecifics::POWER_TYPE_UNSPECIFIED,
      std::move(powers_cb));
  RunUntilIdle();
}

TEST_F(PowerBookmarkServiceTest, UpdatePower) {
  base::MockCallback<SuccessCallback> cb;
  EXPECT_CALL(cb, Run(IsTrue()));
  auto power1 = MakePower(GURL("https://google.com"),
                          sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK);
  auto power2 = power1->Clone();
  service()->CreatePower(std::move(power1), cb.Get());
  RunUntilIdle();

  EXPECT_CALL(cb, Run(IsTrue()));
  service()->UpdatePower(std::move(power2), cb.Get());
  RunUntilIdle();

  histogram()->ExpectBucketCount("PowerBookmarks.PowerUpdated.Success", true,
                                 1);
  histogram()->ExpectTotalCount("PowerBookmarks.PowerUpdated.Success", 1);
  histogram()->ExpectBucketCount(
      "PowerBookmarks.PowerUpdated.PowerType",
      sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK, 1);
  histogram()->ExpectTotalCount("PowerBookmarks.PowerUpdated.PowerType", 1);

  PowersCallback powers_cb = base::BindLambdaForTesting(
      [&](std::vector<std::unique_ptr<Power>> powers) {
        ASSERT_EQ(1u, powers.size());
        ASSERT_FALSE(powers[0]->time_modified().is_null());
      });
  service()->GetPowersForURL(
      GURL("https://google.com"),
      sync_pb::PowerBookmarkSpecifics::POWER_TYPE_UNSPECIFIED,
      std::move(powers_cb));
  RunUntilIdle();
}

TEST_F(PowerBookmarkServiceTest, ShouldNotUpdatePowerIfNotPresent) {
  base::MockCallback<SuccessCallback> cb;
  EXPECT_CALL(cb, Run(IsFalse()));
  service()->UpdatePower(
      MakePower(GURL("https://google.com"),
                sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK),
      cb.Get());
  RunUntilIdle();

  histogram()->ExpectBucketCount("PowerBookmarks.PowerUpdated.Success", false,
                                 1);
  histogram()->ExpectTotalCount("PowerBookmarks.PowerUpdated.Success", 1);

  PowersCallback powers_cb = base::BindLambdaForTesting(
      [&](std::vector<std::unique_ptr<Power>> powers) {
        ASSERT_EQ(0u, powers.size());
      });
  service()->GetPowersForURL(
      GURL("https://google.com"),
      sync_pb::PowerBookmarkSpecifics::POWER_TYPE_UNSPECIFIED,
      std::move(powers_cb));
  RunUntilIdle();
}

TEST_F(PowerBookmarkServiceTest, DeletePower) {
  base::MockCallback<SuccessCallback> success_cb;
  EXPECT_CALL(success_cb, Run(IsTrue()));

  base::Uuid guid = base::Uuid::GenerateRandomV4();
  auto power = MakePower(GURL("https://google.com"),
                         sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK);
  power->set_guid(guid);
  service()->CreatePower(std::move(power), success_cb.Get());
  RunUntilIdle();

  base::MockCallback<PowersCallback> powers_cb;
  EXPECT_CALL(powers_cb, Run(SizeIs(1)));
  service()->GetPowersForURL(
      GURL("https://google.com"),
      sync_pb::PowerBookmarkSpecifics::POWER_TYPE_UNSPECIFIED, powers_cb.Get());
  RunUntilIdle();

  EXPECT_CALL(success_cb, Run(IsTrue()));
  service()->DeletePower(guid, success_cb.Get());
  RunUntilIdle();
  histogram()->ExpectBucketCount("PowerBookmarks.PowerDeleted.Success", true,
                                 1);
  histogram()->ExpectTotalCount("PowerBookmarks.PowerDeleted.Success", 1);

  EXPECT_CALL(powers_cb, Run(SizeIs(0)));
  service()->GetPowersForURL(
      GURL("https://google.com"),
      sync_pb::PowerBookmarkSpecifics::POWER_TYPE_UNSPECIFIED, powers_cb.Get());
  RunUntilIdle();
}

TEST_F(PowerBookmarkServiceTest, DeletePowersForURL) {
  base::MockCallback<SuccessCallback> success_cb;
  EXPECT_CALL(success_cb, Run(IsTrue()));

  service()->CreatePower(
      MakePower(GURL("https://google.com"),
                sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK),
      success_cb.Get());
  RunUntilIdle();

  EXPECT_CALL(success_cb, Run(IsTrue()));
  service()->CreatePower(
      MakePower(GURL("https://google.com"),
                sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK),
      success_cb.Get());
  RunUntilIdle();

  base::MockCallback<PowersCallback> powers_cb;
  EXPECT_CALL(powers_cb, Run(SizeIs(2)));

  service()->GetPowersForURL(
      GURL("https://google.com"),
      sync_pb::PowerBookmarkSpecifics::POWER_TYPE_UNSPECIFIED, powers_cb.Get());
  RunUntilIdle();

  EXPECT_CALL(success_cb, Run(IsTrue()));
  service()->DeletePowersForURL(
      GURL("https://google.com"),
      sync_pb::PowerBookmarkSpecifics::POWER_TYPE_UNSPECIFIED,
      success_cb.Get());
  RunUntilIdle();
  histogram()->ExpectBucketCount("PowerBookmarks.PowersDeletedForURL.Success",
                                 true, 1);
  histogram()->ExpectTotalCount("PowerBookmarks.PowersDeletedForURL.Success",
                                1);
  histogram()->ExpectBucketCount(
      "PowerBookmarks.PowersDeletedForURL.PowerType",
      sync_pb::PowerBookmarkSpecifics::POWER_TYPE_UNSPECIFIED, 1);
  histogram()->ExpectTotalCount("PowerBookmarks.PowersDeletedForURL.PowerType",
                                1);

  EXPECT_CALL(powers_cb, Run(SizeIs(0)));
  service()->GetPowersForURL(
      GURL("https://google.com"),
      sync_pb::PowerBookmarkSpecifics::POWER_TYPE_UNSPECIFIED, powers_cb.Get());
  RunUntilIdle();
}

TEST_F(PowerBookmarkServiceTest, ObserverCalled) {
  MockObserver obs;
  service()->AddObserver(&obs);
  EXPECT_CALL(obs, OnPowersChanged());

  base::MockCallback<SuccessCallback> success_cb;
  EXPECT_CALL(success_cb, Run(IsTrue()));

  base::Uuid guid = base::Uuid::GenerateRandomV4();
  auto power = MakePower(GURL("https://google.com"),
                         sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK);
  power->set_guid(guid);
  auto power2 = power->Clone();
  service()->CreatePower(std::move(power), success_cb.Get());
  RunUntilIdle();

  EXPECT_CALL(obs, OnPowersChanged());
  EXPECT_CALL(success_cb, Run(IsTrue()));

  service()->UpdatePower(std::move(power2), success_cb.Get());
  RunUntilIdle();

  EXPECT_CALL(obs, OnPowersChanged());
  EXPECT_CALL(success_cb, Run(IsTrue()));
  service()->DeletePowersForURL(
      GURL("https://google.com"),
      sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK, success_cb.Get());
  RunUntilIdle();

  service()->RemoveObserver(&obs);
  EXPECT_CALL(obs, OnPowersChanged()).Times(0);
  EXPECT_CALL(success_cb, Run(IsTrue()));

  service()->DeletePower(guid, success_cb.Get());
  RunUntilIdle();
}
}  // namespace power_bookmarks
