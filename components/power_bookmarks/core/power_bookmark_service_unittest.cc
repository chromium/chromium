// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/guid.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/power_bookmarks/core/power_bookmark_data_provider.h"
#include "components/power_bookmarks/core/power_bookmark_service.h"
#include "components/power_bookmarks/core/powers/power.h"
#include "components/power_bookmarks/core/powers/power_overview.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::IsEmpty;
using testing::IsFalse;
using testing::IsTrue;
using testing::SizeIs;

namespace {
std::unique_ptr<power_bookmarks::Power> MakePower(
    GURL url,
    power_bookmarks::PowerType power_type) {
  std::unique_ptr<power_bookmarks::PowerSpecifics> power_specifics =
      std::make_unique<power_bookmarks::PowerSpecifics>();
  std::unique_ptr<power_bookmarks::Power> power =
      std::make_unique<power_bookmarks::Power>(std::move(power_specifics));
  power->set_url(url);
  power->set_power_type(power_type);
  return power;
}
}  // namespace

namespace power_bookmarks {

// Tests for the power bookmark service.
// In-depth tests for the actual storage can be found in
// `power_bookmark_database_unittest`.
class PowerBookmarkServiceTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_directory_.CreateUniqueTempDir());

    model_ = bookmarks::TestBookmarkClient::CreateModel();
    backend_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(), base::TaskPriority::BEST_EFFORT});

    service_ = std::make_unique<PowerBookmarkService>(
        model_.get(), temp_directory_.GetPath(), backend_task_runner_);
    RunUntilIdle();

    service_->InitPowerBookmarkDatabase();
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

 private:
  std::unique_ptr<PowerBookmarkService> service_;
  std::unique_ptr<bookmarks::BookmarkModel> model_;

  base::ScopedTempDir temp_directory_;
  scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;
  base::test::TaskEnvironment task_environment_;
};

class MockDataProvider : public PowerBookmarkDataProvider {
 public:
  MOCK_METHOD2(AttachMetadataForNewBookmark,
               void(const bookmarks::BookmarkNode* node,
                    PowerBookmarkMeta* meta));
};

class MockObserver : public PowerBookmarkService::Observer {
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
      MakePower(GURL("https://google.com"), PowerType::POWER_TYPE_MOCK),
      success_cb.Get());
  RunUntilIdle();

  base::MockCallback<PowersCallback> powers_cb;
  EXPECT_CALL(powers_cb, Run(SizeIs(1)));

  service()->GetPowersForURL(GURL("https://google.com"),
                             PowerType::POWER_TYPE_UNSPECIFIED,
                             powers_cb.Get());
  RunUntilIdle();
}

TEST_F(PowerBookmarkServiceTest, GetPowerOverviewsForType) {
  base::MockCallback<SuccessCallback> success_cb;
  EXPECT_CALL(success_cb, Run(IsTrue()));
  service()->CreatePower(
      MakePower(GURL("https://google.com"), PowerType::POWER_TYPE_MOCK),
      success_cb.Get());
  RunUntilIdle();

  EXPECT_CALL(success_cb, Run(IsTrue()));
  service()->CreatePower(
      MakePower(GURL("https://google.com"), PowerType::POWER_TYPE_MOCK),
      success_cb.Get());
  RunUntilIdle();

  EXPECT_CALL(success_cb, Run(IsTrue()));
  service()->CreatePower(
      MakePower(GURL("https://boogle.com"), PowerType::POWER_TYPE_MOCK),
      success_cb.Get());
  RunUntilIdle();

  base::MockCallback<PowerOverviewsCallback> cb;
  EXPECT_CALL(cb, Run(SizeIs(2)));

  service()->GetPowerOverviewsForType(PowerType::POWER_TYPE_MOCK, cb.Get());
  RunUntilIdle();
}

TEST_F(PowerBookmarkServiceTest, CreatePower) {
  base::MockCallback<SuccessCallback> cb;
  EXPECT_CALL(cb, Run(IsTrue()));

  service()->CreatePower(
      MakePower(GURL("https://google.com"), PowerType::POWER_TYPE_MOCK),
      cb.Get());
  RunUntilIdle();

  PowersCallback powers_cb = base::BindLambdaForTesting(
      [&](std::vector<std::unique_ptr<Power>> powers) {
        ASSERT_EQ(1u, powers.size());
        ASSERT_FALSE(powers[0]->time_added().is_null());
        ASSERT_FALSE(powers[0]->time_modified().is_null());
      });
  service()->GetPowersForURL(GURL("https://google.com"),
                             PowerType::POWER_TYPE_UNSPECIFIED,
                             std::move(powers_cb));
  RunUntilIdle();
}

TEST_F(PowerBookmarkServiceTest, ShouldNotCreatePowerIfPresent) {
  base::MockCallback<SuccessCallback> cb;
  EXPECT_CALL(cb, Run(IsTrue()));
  auto power1 =
      MakePower(GURL("https://google.com"), PowerType::POWER_TYPE_MOCK);
  power1->set_guid(base::GUID::GenerateRandomV4());
  auto power2 = power1->Clone();
  service()->CreatePower(std::move(power1), cb.Get());
  RunUntilIdle();

  EXPECT_CALL(cb, Run(IsFalse()));
  service()->CreatePower(std::move(power2), cb.Get());
  RunUntilIdle();

  PowersCallback powers_cb = base::BindLambdaForTesting(
      [&](std::vector<std::unique_ptr<Power>> powers) {
        ASSERT_EQ(1u, powers.size());
        ASSERT_FALSE(powers[0]->time_added().is_null());
        ASSERT_FALSE(powers[0]->time_modified().is_null());
      });
  service()->GetPowersForURL(GURL("https://google.com"),
                             PowerType::POWER_TYPE_UNSPECIFIED,
                             std::move(powers_cb));
  RunUntilIdle();
}

TEST_F(PowerBookmarkServiceTest, CreatePowerShouldNotUpdateTimeIfPresent) {
  base::MockCallback<SuccessCallback> cb;
  EXPECT_CALL(cb, Run(IsTrue()));
  auto power1 =
      MakePower(GURL("https://google.com"), PowerType::POWER_TYPE_MOCK);
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
  service()->GetPowersForURL(GURL("https://google.com"),
                             PowerType::POWER_TYPE_UNSPECIFIED,
                             std::move(powers_cb));
  RunUntilIdle();
}

TEST_F(PowerBookmarkServiceTest, UpdatePower) {
  base::MockCallback<SuccessCallback> cb;
  EXPECT_CALL(cb, Run(IsTrue()));
  auto power1 =
      MakePower(GURL("https://google.com"), PowerType::POWER_TYPE_MOCK);
  power1->set_guid(base::GUID::GenerateRandomV4());
  auto power2 = power1->Clone();
  service()->CreatePower(std::move(power1), cb.Get());
  RunUntilIdle();

  EXPECT_CALL(cb, Run(IsTrue()));
  service()->UpdatePower(std::move(power2), cb.Get());
  RunUntilIdle();

  PowersCallback powers_cb = base::BindLambdaForTesting(
      [&](std::vector<std::unique_ptr<Power>> powers) {
        ASSERT_EQ(1u, powers.size());
        ASSERT_FALSE(powers[0]->time_modified().is_null());
      });
  service()->GetPowersForURL(GURL("https://google.com"),
                             PowerType::POWER_TYPE_UNSPECIFIED,
                             std::move(powers_cb));
  RunUntilIdle();
}

TEST_F(PowerBookmarkServiceTest, ShouldNotUpdatePowerIfNotPresent) {
  base::MockCallback<SuccessCallback> cb;
  EXPECT_CALL(cb, Run(IsFalse()));
  service()->UpdatePower(
      MakePower(GURL("https://google.com"), PowerType::POWER_TYPE_MOCK),
      cb.Get());
  RunUntilIdle();

  PowersCallback powers_cb = base::BindLambdaForTesting(
      [&](std::vector<std::unique_ptr<Power>> powers) {
        ASSERT_EQ(0u, powers.size());
      });
  service()->GetPowersForURL(GURL("https://google.com"),
                             PowerType::POWER_TYPE_UNSPECIFIED,
                             std::move(powers_cb));
  RunUntilIdle();
}

TEST_F(PowerBookmarkServiceTest, DeletePower) {
  base::MockCallback<SuccessCallback> success_cb;
  EXPECT_CALL(success_cb, Run(IsTrue()));

  base::GUID guid = base::GUID::GenerateRandomV4();
  auto power =
      MakePower(GURL("https://google.com"), PowerType::POWER_TYPE_MOCK);
  power->set_guid(guid);
  service()->CreatePower(std::move(power), success_cb.Get());
  RunUntilIdle();

  base::MockCallback<PowersCallback> powers_cb;
  EXPECT_CALL(powers_cb, Run(SizeIs(1)));
  service()->GetPowersForURL(GURL("https://google.com"),
                             PowerType::POWER_TYPE_UNSPECIFIED,
                             powers_cb.Get());
  RunUntilIdle();

  EXPECT_CALL(success_cb, Run(IsTrue()));
  service()->DeletePower(guid, success_cb.Get());
  RunUntilIdle();

  EXPECT_CALL(powers_cb, Run(SizeIs(0)));
  service()->GetPowersForURL(GURL("https://google.com"),
                             PowerType::POWER_TYPE_UNSPECIFIED,
                             powers_cb.Get());
  RunUntilIdle();
}

TEST_F(PowerBookmarkServiceTest, DeletePowersForURL) {
  base::MockCallback<SuccessCallback> success_cb;
  EXPECT_CALL(success_cb, Run(IsTrue()));

  service()->CreatePower(
      MakePower(GURL("https://google.com"), PowerType::POWER_TYPE_MOCK),
      success_cb.Get());
  RunUntilIdle();

  EXPECT_CALL(success_cb, Run(IsTrue()));
  service()->CreatePower(
      MakePower(GURL("https://google.com"), PowerType::POWER_TYPE_MOCK),
      success_cb.Get());
  RunUntilIdle();

  base::MockCallback<PowersCallback> powers_cb;
  EXPECT_CALL(powers_cb, Run(SizeIs(2)));

  service()->GetPowersForURL(GURL("https://google.com"),
                             PowerType::POWER_TYPE_UNSPECIFIED,
                             powers_cb.Get());
  RunUntilIdle();

  EXPECT_CALL(success_cb, Run(IsTrue()));
  service()->DeletePowersForURL(GURL("https://google.com"),
                                PowerType::POWER_TYPE_UNSPECIFIED,
                                success_cb.Get());
  RunUntilIdle();

  EXPECT_CALL(powers_cb, Run(SizeIs(0)));
  service()->GetPowersForURL(GURL("https://google.com"),
                             PowerType::POWER_TYPE_UNSPECIFIED,
                             powers_cb.Get());
  RunUntilIdle();
}

TEST_F(PowerBookmarkServiceTest, ObserverCalled) {
  MockObserver obs;
  service()->AddObserver(&obs);
  EXPECT_CALL(obs, OnPowersChanged());

  base::MockCallback<SuccessCallback> success_cb;
  EXPECT_CALL(success_cb, Run(IsTrue()));

  base::GUID guid = base::GUID::GenerateRandomV4();
  std::unique_ptr<PowerSpecifics> power_specifics =
      std::make_unique<PowerSpecifics>();
  auto power =
      MakePower(GURL("https://google.com"), PowerType::POWER_TYPE_MOCK);
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
  service()->DeletePowersForURL(GURL("https://google.com"),
                                PowerType::POWER_TYPE_MOCK, success_cb.Get());
  RunUntilIdle();

  service()->RemoveObserver(&obs);
  EXPECT_CALL(obs, OnPowersChanged()).Times(0);
  EXPECT_CALL(success_cb, Run(IsTrue()));

  service()->DeletePower(guid, success_cb.Get());
  RunUntilIdle();
}
}  // namespace power_bookmarks
