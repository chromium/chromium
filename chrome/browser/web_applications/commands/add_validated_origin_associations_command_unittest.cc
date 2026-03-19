// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/add_validated_origin_associations_command.h"

#include <memory>

#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/test_future.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "chrome/browser/web_applications/scheduler/add_validated_origin_associations_result.h"
#include "chrome/browser/web_applications/test/fake_web_app_origin_association_manager.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registrar_observer.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "components/webapps/common/web_app_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

namespace web_app {

namespace {

using testing::_;

class MockWebAppCommandScheduler : public WebAppCommandScheduler {
 public:
  using WebAppCommandScheduler::WebAppCommandScheduler;
  MOCK_METHOD(void,
              ScheduleResolveWebAppPendingMigrationInfo,
              (base::OnceClosure callback, const base::Location& location),
              (override));
};

class MockWebAppRegistrarObserver : public WebAppRegistrarObserver {
 public:
  MOCK_METHOD(void, OnAppRegistrarDestroyed, (), (override));
  MOCK_METHOD(void,
              OnWebAppEffectiveScopeChanged,
              (const webapps::AppId& app_id, const WebAppScope& new_scope),
              (override));
};

}  // namespace

class AddValidatedOriginAssociationsCommandTest : public WebAppTest {
 public:
  AddValidatedOriginAssociationsCommandTest() = default;
  ~AddValidatedOriginAssociationsCommandTest() override = default;

  void SetUp() override {
    WebAppTest::SetUp();

    auto origin_association_manager =
        std::make_unique<FakeWebAppOriginAssociationManager>();
    fake_origin_association_manager_ = origin_association_manager.get();
    fake_provider().SetOriginAssociationManager(
        std::move(origin_association_manager));

    auto scheduler =
        std::make_unique<testing::NiceMock<MockWebAppCommandScheduler>>(
            *profile());
    mock_scheduler_ = scheduler.get();
    fake_provider().SetScheduler(std::move(scheduler));

    clock_ = std::make_unique<base::SimpleTestClock>();
    clock_->SetNow(base::Time::Now());

    fake_provider().SetClockForTesting(clock_.get());

    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  void TearDown() override {
    fake_origin_association_manager_ = nullptr;
    mock_scheduler_ = nullptr;
    WebAppTest::TearDown();
  }

  webapps::AppId InstallApp(const GURL& start_url,
                            const ScopeExtensions& scope_extensions) {
    auto info = WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
    info->title = u"Test App";
    info->scope_extensions = scope_extensions;
    return test::InstallWebApp(profile(), std::move(info));
  }

  FakeWebAppOriginAssociationManager* fake_origin_association_manager() {
    return fake_origin_association_manager_;
  }

  base::SimpleTestClock& clock() { return *clock_.get(); }

  MockWebAppCommandScheduler& mock_scheduler() { return *mock_scheduler_; }

 private:
  raw_ptr<FakeWebAppOriginAssociationManager> fake_origin_association_manager_ =
      nullptr;
  raw_ptr<MockWebAppCommandScheduler> mock_scheduler_ = nullptr;
  std::unique_ptr<base::SimpleTestClock> clock_;
};

TEST_F(AddValidatedOriginAssociationsCommandTest, Success) {
  GURL start_url("https://example.com/");
  ScopeExtensionInfo extension = ScopeExtensionInfo::CreateForScope(
      GURL("https://example.org/scope"), /*has_origin_wildcard=*/false);

  // Install will not validate scope extensions.
  fake_origin_association_manager()->set_pass_through(false);
  webapps::AppId app_id = InstallApp(start_url, {extension});
  clock().Advance(base::Days(1));

  base::HistogramTester tester;
  fake_origin_association_manager()->set_pass_through(true);

  MockWebAppRegistrarObserver observer;
  base::ScopedObservation<WebAppRegistrar, WebAppRegistrarObserver> observation(
      &observer);
  observation.Observe(&provider().registrar_unsafe());

  EXPECT_CALL(observer, OnWebAppEffectiveScopeChanged(app_id, _));

  base::test::TestFuture<AddValidatedOriginAssociationsResult> future;

  provider().scheduler().ScheduleAddValidatedOriginAssociations(
      app_id, future.GetCallback());
  ASSERT_EQ(AddValidatedOriginAssociationsResult::kSuccess, future.Get());

  tester.ExpectUniqueSample("WebApp.AddValidatedOriginAssociations",
                            AddValidatedOriginAssociationsResult::kSuccess, 1);

  const WebApp* app = provider().registrar_unsafe().GetAppById(app_id);
  EXPECT_FALSE(app->validated_scope_extensions().empty());
  EXPECT_EQ(extension, *app->validated_scope_extensions().begin());
  EXPECT_TRUE(app->origin_association_last_validation_check_time().has_value());
}

TEST_F(AddValidatedOriginAssociationsCommandTest, UnvalidatedItemsRemain) {
  GURL start_url("https://example.com/");

  // Fake manager defaults to returning empty associations, which means failure
  // if we have unvalidated extensions.
  fake_origin_association_manager()->set_pass_through(false);

  ScopeExtensionInfo extension = ScopeExtensionInfo::CreateForScope(
      GURL("https://example.org/scope"), /*has_origin_wildcard=*/false);

  webapps::AppId app_id = InstallApp(start_url, {extension});
  {
    const WebApp* app = provider().registrar_unsafe().GetAppById(app_id);
    EXPECT_TRUE(app->validated_scope_extensions().empty());
    EXPECT_TRUE(
        app->origin_association_last_validation_check_time().has_value());
  }

  base::HistogramTester tester;
  clock().Advance(base::Days(1));
  base::test::TestFuture<AddValidatedOriginAssociationsResult> future;
  provider().scheduler().ScheduleAddValidatedOriginAssociations(
      app_id, future.GetCallback());
  ASSERT_EQ(AddValidatedOriginAssociationsResult::kUnvalidatedItemsRemain,
            future.Get());

  tester.ExpectUniqueSample(
      "WebApp.AddValidatedOriginAssociations",
      AddValidatedOriginAssociationsResult::kUnvalidatedItemsRemain, 1);

  const WebApp* app = provider().registrar_unsafe().GetAppById(app_id);
  EXPECT_TRUE(app->validated_scope_extensions().empty());
  EXPECT_TRUE(app->origin_association_last_validation_check_time().has_value());
}

TEST_F(AddValidatedOriginAssociationsCommandTest, ThrottledAfterInstall) {
  GURL start_url("https://example.com/");
  ScopeExtensionInfo extension = ScopeExtensionInfo::CreateForScope(
      GURL("https://example.org/scope"), /*has_origin_wildcard=*/false);

  webapps::AppId app_id = InstallApp(start_url, {extension});

  // Fake manager defaults to returning empty associations, which means failure
  // if we have unvalidated extensions.
  fake_origin_association_manager()->set_pass_through(false);

  base::HistogramTester tester;
  base::test::TestFuture<AddValidatedOriginAssociationsResult> future;
  provider().scheduler().ScheduleAddValidatedOriginAssociations(
      app_id, future.GetCallback());
  ASSERT_EQ(AddValidatedOriginAssociationsResult::kThrottled, future.Get());

  tester.ExpectUniqueSample("WebApp.AddValidatedOriginAssociations",
                            AddValidatedOriginAssociationsResult::kThrottled,
                            1);

  const WebApp* app = provider().registrar_unsafe().GetAppById(app_id);
  EXPECT_TRUE(app->validated_scope_extensions().empty());
  EXPECT_TRUE(app->origin_association_last_validation_check_time().has_value());
}

TEST_F(AddValidatedOriginAssociationsCommandTest, ThrottledAfterRevalidate) {
  GURL start_url("https://example.com/");
  ScopeExtensionInfo extension = ScopeExtensionInfo::CreateForScope(
      GURL("https://example.org/scope"), /*has_origin_wildcard=*/false);

  fake_origin_association_manager()->set_pass_through(false);

  webapps::AppId app_id = InstallApp(start_url, {extension});
  clock().Advance(base::Days(1));

  base::HistogramTester tester;
  base::test::TestFuture<AddValidatedOriginAssociationsResult> future;
  provider().scheduler().ScheduleAddValidatedOriginAssociations(
      app_id, future.GetCallback());
  ASSERT_EQ(AddValidatedOriginAssociationsResult::kUnvalidatedItemsRemain,
            future.Get());
  tester.ExpectUniqueSample(
      "WebApp.AddValidatedOriginAssociations",
      AddValidatedOriginAssociationsResult::kUnvalidatedItemsRemain, 1);

  base::HistogramTester tester2;
  base::test::TestFuture<AddValidatedOriginAssociationsResult> future2;
  provider().scheduler().ScheduleAddValidatedOriginAssociations(
      app_id, future2.GetCallback());
  EXPECT_EQ(AddValidatedOriginAssociationsResult::kThrottled, future2.Get());
  tester2.ExpectUniqueSample("WebApp.AddValidatedOriginAssociations",
                             AddValidatedOriginAssociationsResult::kThrottled,
                             1);

  const WebApp* app = provider().registrar_unsafe().GetAppById(app_id);
  EXPECT_TRUE(app->validated_scope_extensions().empty());
  EXPECT_TRUE(app->origin_association_last_validation_check_time().has_value());
}

TEST_F(AddValidatedOriginAssociationsCommandTest, ThrottledWhenNoTimeValue) {
  GURL start_url("https://example.com/");
  ScopeExtensionInfo extension = ScopeExtensionInfo::CreateForScope(
      GURL("https://example.org/scope"), /*has_origin_wildcard=*/false);

  webapps::AppId app_id = InstallApp(start_url, {extension});

  // Clear the time value.
  {
    ScopedRegistryUpdate update = provider().sync_bridge_unsafe().BeginUpdate();
    WebApp* app_to_update = update->UpdateApp(app_id);
    app_to_update->SetOriginAssociationLastValidationCheckTime(std::nullopt);
  }

  {
    const WebApp* app = provider().registrar_unsafe().GetAppById(app_id);
    EXPECT_FALSE(
        app->origin_association_last_validation_check_time().has_value());
  }

  base::HistogramTester tester;
  base::test::TestFuture<AddValidatedOriginAssociationsResult> future;
  provider().scheduler().ScheduleAddValidatedOriginAssociations(
      app_id, future.GetCallback());
  ASSERT_EQ(AddValidatedOriginAssociationsResult::kThrottled, future.Get());

  tester.ExpectUniqueSample("WebApp.AddValidatedOriginAssociations",
                            AddValidatedOriginAssociationsResult::kThrottled,
                            1);

  const WebApp* app = provider().registrar_unsafe().GetAppById(app_id);
  EXPECT_TRUE(app->origin_association_last_validation_check_time().has_value());
  // The time should be within the last 24 hours.
  EXPECT_GE(*app->origin_association_last_validation_check_time(),
            clock().Now());
}

TEST_F(AddValidatedOriginAssociationsCommandTest, NotThrottleAfterDay) {
  GURL start_url("https://example.com/");
  ScopeExtensionInfo extension = ScopeExtensionInfo::CreateForScope(
      GURL("https://example.org/scope"), /*has_origin_wildcard=*/false);

  fake_origin_association_manager()->set_pass_through(false);
  webapps::AppId app_id = InstallApp(start_url, {extension});
  clock().Advance(base::Days(1));

  base::HistogramTester tester;
  base::test::TestFuture<AddValidatedOriginAssociationsResult> future;
  provider().scheduler().ScheduleAddValidatedOriginAssociations(
      app_id, future.GetCallback());
  ASSERT_EQ(AddValidatedOriginAssociationsResult::kUnvalidatedItemsRemain,
            future.Get());
  tester.ExpectUniqueSample(
      "WebApp.AddValidatedOriginAssociations",
      AddValidatedOriginAssociationsResult::kUnvalidatedItemsRemain, 1);

  fake_origin_association_manager()->set_pass_through(true);
  clock().Advance(base::Days(1) + base::Seconds(1));

  base::HistogramTester tester2;

  base::test::TestFuture<AddValidatedOriginAssociationsResult> future2;
  provider().scheduler().ScheduleAddValidatedOriginAssociations(
      app_id, future2.GetCallback());
  EXPECT_EQ(AddValidatedOriginAssociationsResult::kSuccess, future2.Get());

  tester2.ExpectUniqueSample("WebApp.AddValidatedOriginAssociations",
                             AddValidatedOriginAssociationsResult::kSuccess, 1);

  const WebApp* app = provider().registrar_unsafe().GetAppById(app_id);
  EXPECT_FALSE(app->validated_scope_extensions().empty());
  EXPECT_TRUE(app->origin_association_last_validation_check_time().has_value());
}

TEST_F(AddValidatedOriginAssociationsCommandTest, NotNeededEmpty) {
  GURL start_url("https://example.com/");
  // No scope extensions.
  webapps::AppId app_id = InstallApp(start_url, {});

  base::HistogramTester tester;
  base::test::TestFuture<AddValidatedOriginAssociationsResult> future;
  provider().scheduler().ScheduleAddValidatedOriginAssociations(
      app_id, future.GetCallback());
  ASSERT_EQ(AddValidatedOriginAssociationsResult::kNotNeeded, future.Get());
  tester.ExpectUniqueSample("WebApp.AddValidatedOriginAssociations",
                            AddValidatedOriginAssociationsResult::kNotNeeded,
                            1);
}

TEST_F(AddValidatedOriginAssociationsCommandTest, NotNeededValidated) {
  fake_origin_association_manager()->set_pass_through(true);

  GURL start_url("https://example.com/");
  ScopeExtensionInfo extension = ScopeExtensionInfo::CreateForScope(
      GURL("https://example.org/scope"), /*has_origin_wildcard=*/false);

  webapps::AppId app_id = InstallApp(start_url, {extension});
  clock().Advance(base::Days(1));

  {
    const WebApp* app = provider().registrar_unsafe().GetAppById(app_id);
    EXPECT_FALSE(app->validated_scope_extensions().empty());
    EXPECT_FALSE(app->scope_extensions().empty());
  }

  base::HistogramTester tester;
  base::test::TestFuture<AddValidatedOriginAssociationsResult> future;

  provider().scheduler().ScheduleAddValidatedOriginAssociations(
      app_id, future.GetCallback());
  ASSERT_EQ(AddValidatedOriginAssociationsResult::kNotNeeded, future.Get());
  tester.ExpectUniqueSample("WebApp.AddValidatedOriginAssociations",
                            AddValidatedOriginAssociationsResult::kNotNeeded,
                            1);
}

TEST_F(AddValidatedOriginAssociationsCommandTest, AppDisabled) {
  base::HistogramTester tester;
  base::test::TestFuture<AddValidatedOriginAssociationsResult> future;
  provider().scheduler().ScheduleAddValidatedOriginAssociations(
      "non-existent-app-id", future.GetCallback());
  ASSERT_EQ(AddValidatedOriginAssociationsResult::kWebAppNotInstalled,
            future.Get());
  tester.ExpectUniqueSample(
      "WebApp.AddValidatedOriginAssociations",
      AddValidatedOriginAssociationsResult::kWebAppNotInstalled, 1);
}

TEST_F(AddValidatedOriginAssociationsCommandTest, MigrationSourcesSuccess) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      blink::features::kWebAppMigrationApi);

  GURL start_url("https://example.com/");
  auto info = WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
  info->title = u"Test App";
  webapps::ManifestId manifest_id = info->manifest_id();
  webapps::AppId app_id = test::InstallWebApp(profile(), std::move(info));

  MigrationSource migration_source(manifest_id, MigrationBehavior::kForce,
                                   GURL("https://example.com/subpath"));

  {
    ScopedRegistryUpdate update = provider().sync_bridge_unsafe().BeginUpdate();
    WebApp* app_to_update = update->UpdateApp(app_id);
    app_to_update->SetUnvalidatedMigrationSources({migration_source});
  }

  base::HistogramTester tester;
  fake_origin_association_manager()->set_pass_through(true);
  clock().Advance(base::Days(1) + base::Seconds(1));

  EXPECT_CALL(mock_scheduler(), ScheduleResolveWebAppPendingMigrationInfo(_, _))
      .Times(1);

  base::test::TestFuture<AddValidatedOriginAssociationsResult> future;
  provider().scheduler().ScheduleAddValidatedOriginAssociations(
      app_id, future.GetCallback());
  ASSERT_EQ(AddValidatedOriginAssociationsResult::kSuccess, future.Get());
  tester.ExpectUniqueSample("WebApp.AddValidatedOriginAssociations",
                            AddValidatedOriginAssociationsResult::kSuccess, 1);

  const WebApp* app = provider().registrar_unsafe().GetAppById(app_id);
  EXPECT_FALSE(app->validated_migration_sources().empty());
  EXPECT_EQ(migration_source, *app->validated_migration_sources().begin());

  // Check that is not needed.
  base::HistogramTester tester2;
  clock().Advance(base::Days(1) + base::Seconds(1));
  base::test::TestFuture<AddValidatedOriginAssociationsResult> future2;
  provider().scheduler().ScheduleAddValidatedOriginAssociations(
      app_id, future2.GetCallback());
  EXPECT_EQ(AddValidatedOriginAssociationsResult::kNotNeeded, future2.Get());
  tester2.ExpectUniqueSample("WebApp.AddValidatedOriginAssociations",
                             AddValidatedOriginAssociationsResult::kNotNeeded,
                             1);
}

}  // namespace web_app
