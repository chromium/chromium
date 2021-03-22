// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/arc/arc_app_reinstall_search_provider.h"

#include <map>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/timer/mock_timer.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/ui/app_list/arc/arc_app_test.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/test/test_app_list_controller_delegate.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/arc/mojom/app.mojom.h"
#include "components/arc/test/fake_app_instance.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "extensions/grit/extensions_browser_resources.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"

using testing::ByRef;
using testing::Eq;

class ArcAppReinstallSearchProviderTest
    : public extensions::ExtensionServiceTestBase {
 protected:
  void SetUp() override {
    extensions::ExtensionServiceTestBase::SetUp();
    auto params = CreateDefaultInitParams();
    // We unset the pref_file so that testing_profile will have a
    // testing_pref_service() available.
    params.pref_file = base::FilePath();
    InitializeExtensionService(params);
    service_->Init();

    // Let any async services complete their set-up.
    base::RunLoop().RunUntilIdle();

    // Set up custom things for our tests.
    arc_app_test_.SetUp(profile_.get());
    app_provider_ =
        base::WrapUnique(new app_list::ArcAppReinstallSearchProvider(
            profile_.get(), /*max_result_count=*/2));
    std::unique_ptr<base::MockRepeatingTimer> timer =
        std::make_unique<base::MockRepeatingTimer>();
    mock_timer_ = timer.get();
    app_provider_->SetTimerForTesting(std::move(timer));
    app_candidate_ptr_ = arc::mojom::AppReinstallCandidate::New();
  }

  void TearDown() override {
    mock_timer_ = nullptr;
    app_provider_.reset(nullptr);
    arc_app_test_.TearDown();

    extensions::ExtensionServiceTestBase::TearDown();
  }

  arc::FakeAppInstance* app_instance() { return arc_app_test_.app_instance(); }

  void SendPlayStoreApp() {
    arc::mojom::AppInfo app;
    app.name = "Play Store";
    app.package_name = arc::kPlayStorePackage;
    app.activity = arc::kPlayStoreActivity;

    app_instance()->SendRefreshAppList({app});
  }

  arc::mojom::ArcPackageInfoPtr GetPackagePtr(const std::string& package_name) {
    arc::mojom::ArcPackageInfo package;
    package.package_name = package_name;
    package.sync = false;
    return package.Clone();
  }

  std::string kAppState = "arc_app_reinstall_state";
  void SetStateInt64(Profile* profile,
                     const std::string& package_name,
                     const std::string& key,
                     const int64_t value) {
    const std::string int64_str = base::NumberToString(value);
    DictionaryPrefUpdate update(profile->GetPrefs(), kAppState);
    base::DictionaryValue* const dictionary = update.Get();
    base::Value* package_item =
        dictionary->FindKeyOfType(package_name, base::Value::Type::DICTIONARY);
    if (!package_item) {
      package_item = dictionary->SetKey(
          package_name, base::Value(base::Value::Type::DICTIONARY));
    }

    package_item->SetKey(key, base::Value(int64_str));
  }

  void SetStateTime(Profile* profile,
                    const std::string& package_name,
                    const std::string& key,
                    const base::Time tstamp) {
    const int64_t timestamp =
        tstamp.ToDeltaSinceWindowsEpoch().InMilliseconds();
    SetStateInt64(profile, package_name, key, timestamp);
  }

  bool GetStateInt64(Profile* profile,
                     const std::string& package_name,
                     const std::string& key,
                     int64_t* value) {
    const base::DictionaryValue* dictionary =
        profile->GetPrefs()->GetDictionary(kAppState);
    if (!dictionary)
      return false;
    const base::Value* package_item =
        dictionary->FindKeyOfType(package_name, base::Value::Type::DICTIONARY);
    if (!package_item)
      return false;
    const std::string* value_str = package_item->FindStringKey(key);
    if (!value_str)
      return false;

    if (!base::StringToInt64(*value_str, value)) {
      LOG(ERROR) << "Failed conversion " << *value_str;
      return false;
    }

    return true;
  }

  // Owned by |app_provider_|.
  base::MockRepeatingTimer* mock_timer_;
  ArcAppTest arc_app_test_;
  std::unique_ptr<app_list::ArcAppReinstallSearchProvider> app_provider_;
  arc::mojom::AppReinstallCandidatePtr app_candidate_ptr_;
};

namespace {
class TestSearchResult : public ChromeSearchResult {
 public:
  void Open(int event_flags) override {}
  void SetId(const std::string& str) {
    // set_id is protected in chromesearchresult.
    ChromeSearchResult::set_id(str);
  }
};
}  // namespace

TEST_F(ArcAppReinstallSearchProviderTest, NoResultWithoutPlayStore) {
  EXPECT_EQ(0u, app_provider_->results().size());
  EXPECT_FALSE(mock_timer_->IsRunning());
}

TEST_F(ArcAppReinstallSearchProviderTest, TestTimerOn) {
  EXPECT_EQ(0, app_instance()->get_app_reinstall_callback_count());
  SendPlayStoreApp();
  EXPECT_TRUE(mock_timer_->IsRunning());
  EXPECT_EQ(1, app_instance()->get_app_reinstall_callback_count());
  mock_timer_->Fire();
  EXPECT_EQ(2, app_instance()->get_app_reinstall_callback_count());
  // We expect no results since there are no apps we added to the candidate
  // list.
  EXPECT_EQ(0u, app_provider_->results().size());

  // Now, stop!
  arc_app_test_.StopArcInstance();
  EXPECT_FALSE(mock_timer_->IsRunning());
}

TEST_F(ArcAppReinstallSearchProviderTest, TestPolicyManagedUser) {
  testing_pref_service()->SetManagedPref(
      prefs::kAppReinstallRecommendationEnabled,
      std::make_unique<base::Value>(false));
  std::vector<arc::mojom::AppReinstallCandidatePtr> candidates;
  candidates.emplace_back(arc::mojom::AppReinstallCandidate::New(
      "com.package.fakepackage1", "Title of first package",
      "http://icon.com/icon1", 15, 4.7));
  app_instance()->SetAppReinstallCandidates(candidates);
  EXPECT_EQ(0, app_instance()->get_app_reinstall_callback_count());
  SendPlayStoreApp();
  // It's a managed user, default of pref to allow for this feature is false.
  // Expect 0 callback executions.
  EXPECT_EQ(0, app_instance()->get_app_reinstall_callback_count());

  // Let's update the pref to true, and see that we end up with a result.
  testing_pref_service()->SetManagedPref(
      prefs::kAppReinstallRecommendationEnabled,
      std::make_unique<base::Value>(true));
  mock_timer_->Fire();
  EXPECT_EQ(1, app_instance()->get_app_reinstall_callback_count());
}

TEST_F(ArcAppReinstallSearchProviderTest, TestResultsWithSearchChanged) {
  std::vector<arc::mojom::AppReinstallCandidatePtr> candidates;
  candidates.emplace_back(arc::mojom::AppReinstallCandidate::New(
      "com.package.fakepackage1", "Title of first package",
      "http://icon.com/icon1", 15, 4.7));
  candidates.emplace_back(arc::mojom::AppReinstallCandidate::New(
      "com.package.fakepackage2", "Title of second package",
      "http://icon.com/icon2", 15, 4.3));
  app_instance()->SetAppReinstallCandidates(candidates);
  EXPECT_EQ(0, app_instance()->get_app_reinstall_callback_count());
  SendPlayStoreApp();
  EXPECT_EQ(1, app_instance()->get_app_reinstall_callback_count());
  // So we should have populated exactly 0 apps, since we expect loading to
  // finish later.
  EXPECT_EQ(0u, app_provider_->results().size());
  EXPECT_EQ(2u, app_provider_->loading_icon_urls_.size());

  // Fake the load of 2 icons.
  app_provider_->OnIconLoaded("http://icon.com/icon1");
  EXPECT_EQ(1u, app_provider_->loading_icon_urls_.size());
  app_provider_->OnIconLoaded("http://icon.com/icon2");
  ASSERT_EQ(2u, app_provider_->results().size());

  EXPECT_EQ(
      "https://play.google.com/store/apps/details?id=com.package.fakepackage1",
      app_provider_->results()[0]->id());

  // Test that we set to 0 results when having a query, or when arc is turned
  // off.
  app_provider_->Start(u"non empty query");
  EXPECT_EQ(0u, app_provider_->results().size());
  // Verify that all icons are still loaded.
  EXPECT_EQ(2u, app_provider_->icon_urls_.size());
  EXPECT_EQ(0u, app_provider_->loading_icon_urls_.size());
  app_provider_->Start(std::u16string());
  EXPECT_EQ(2u, app_provider_->results().size());

  app_instance()->SendInstallationStarted("com.package.fakepackage1");
  EXPECT_EQ(1u, app_provider_->results().size());

  // We should see that only 1 icon is loaded in our final results.
  EXPECT_EQ(1u, app_provider_->icon_urls_.size());

  // When arc is turned off, we should get no results and that we get no
  arc_app_test_.StopArcInstance();
  EXPECT_EQ(0u, app_provider_->results().size());
  // We expect icons to be deleted when arc is stopped.
  EXPECT_EQ(0u, app_provider_->icon_urls_.size());
}

TEST_F(ArcAppReinstallSearchProviderTest, TestResultsWithAppsChanged) {
  std::vector<arc::mojom::AppReinstallCandidatePtr> candidates;
  // We do something unlikely and sneaky here: we give the same icon to two
  // apps. Unlikely, but possible.
  candidates.emplace_back(arc::mojom::AppReinstallCandidate::New(
      "com.package.fakepackage1", "Title of first package",
      "http://icon.com/icon1", 15, 4.7));
  candidates.emplace_back(arc::mojom::AppReinstallCandidate::New(
      "com.package.fakepackage2", "Title of second package",
      "http://icon.com/icon1", 15, 4.3));
  app_instance()->SetAppReinstallCandidates(candidates);
  EXPECT_EQ(0, app_instance()->get_app_reinstall_callback_count());
  SendPlayStoreApp();
  EXPECT_EQ(1, app_instance()->get_app_reinstall_callback_count());
  // So we should have populated exactly 0 apps, since we expect loading to
  // finish later.
  EXPECT_EQ(0u, app_provider_->results().size());

  // Fake the load of 1 icon.
  app_provider_->OnIconLoaded("http://icon.com/icon1");
  ASSERT_EQ(2u, app_provider_->results().size());

  EXPECT_EQ(
      "https://play.google.com/store/apps/details?id=com.package.fakepackage1",
      app_provider_->results()[0]->id());

  app_instance()->SendInstallationStarted("com.package.fakepackage1");
  EXPECT_EQ(1u, app_provider_->results().size());
  app_instance()->InstallPackage(GetPackagePtr("com.package.fakepackage1"));
  app_instance()->SendInstallationFinished("com.package.fakepackage1", true);
  EXPECT_EQ(1u, app_provider_->results().size());
  app_instance()->UninstallPackage("com.package.fakepackage1");
  // We expect the uninstall not to go back to the list.
  EXPECT_EQ(1u, app_provider_->results().size());

  // Check for persistence:

  app_provider_ = base::WrapUnique(new app_list::ArcAppReinstallSearchProvider(
      profile_.get(), /*max_result_count=*/2));

  EXPECT_EQ(0u, app_provider_->results().size());
  app_provider_->OnIconLoaded("http://icon.com/icon1");
  EXPECT_EQ(1u, app_provider_->results().size());

  base::HistogramTester histogram_tester;
  // Check that impression counts are read and written appropriately.
  const std::string fake_package2 = "com.package.fakepackage2";

  // should update to 1.
  app_provider_->OnVisibilityChanged(fake_package2, true);

  int64_t loaded_impression_count = 0;
  EXPECT_TRUE(
      GetStateInt64(profile_.get(), fake_package2,
                    app_list::ArcAppReinstallSearchProvider::kImpressionCount,
                    &loaded_impression_count));
  EXPECT_EQ(1, loaded_impression_count);
  EXPECT_EQ(1, histogram_tester.GetBucketCount(
                   "Arc.AppListRecommendedImp.AllImpression", 1));
  EXPECT_EQ(1, histogram_tester.GetBucketCount(
                   "Arc.AppListRecommendedImp.CountedImpression", 1));

  // An immediate re-show does nothing, but the "all impression count" is
  // increased.
  app_provider_->OnVisibilityChanged(fake_package2, true);
  EXPECT_EQ(2, histogram_tester.GetBucketCount(
                   "Arc.AppListRecommendedImp.AllImpression", 1));
  EXPECT_EQ(1, histogram_tester.GetBucketCount(
                   "Arc.AppListRecommendedImp.CountedImpression", 1));
  loaded_impression_count = 0;
  EXPECT_TRUE(
      GetStateInt64(profile_.get(), fake_package2,
                    app_list::ArcAppReinstallSearchProvider::kImpressionCount,
                    &loaded_impression_count));
  EXPECT_EQ(1, loaded_impression_count);

  // But, setting impression time back does.
  for (int i = 0; i < 4; ++i) {
    SetStateInt64(profile_.get(), fake_package2,
                  app_list::ArcAppReinstallSearchProvider::kImpressionTime, 0);
    app_provider_->OnVisibilityChanged(fake_package2, true);
  }
  loaded_impression_count = 0;
  EXPECT_TRUE(
      GetStateInt64(profile_.get(), fake_package2,
                    app_list::ArcAppReinstallSearchProvider::kImpressionCount,
                    &loaded_impression_count));
  EXPECT_EQ(5, loaded_impression_count);
  EXPECT_EQ(6, histogram_tester.GetBucketCount(
                   "Arc.AppListRecommendedImp.AllImpression", 1));
  EXPECT_EQ(5, histogram_tester.GetBucketCount(
                   "Arc.AppListRecommendedImp.CountedImpression", 1));

  SetStateInt64(profile_.get(), fake_package2,
                app_list::ArcAppReinstallSearchProvider::kImpressionCount, 50);
  app_provider_->UpdateResults();
  EXPECT_EQ(0u, app_provider_->results().size());
  SetStateInt64(profile_.get(), fake_package2,
                app_list::ArcAppReinstallSearchProvider::kImpressionCount, 0);
  app_provider_->UpdateResults();
  app_provider_->OnIconLoaded("http://icon.com/icon1");
  EXPECT_EQ(1u, app_provider_->results().size());

  // If uninstalled recently, avoid.
  const std::string uninstall_time = "uninstall_time";
  SetStateInt64(profile_.get(), fake_package2, uninstall_time,
                base::Time::Now().ToDeltaSinceWindowsEpoch().InMilliseconds());
  app_provider_->UpdateResults();
  EXPECT_EQ(0u, app_provider_->results().size());
}

TEST_F(ArcAppReinstallSearchProviderTest, TestShouldShowAnything) {
  EXPECT_TRUE(app_provider_->ShouldShowAnything());
  std::map<std::string, std::string> feature_params;
  feature_params["interaction_grace_hours"] = "72";
  feature_params["impression_count_limit"] = "5";
  const std::string fake_package2 = "com.package.fakepackage2";
  const std::string fake_package3 = "com.package.fakepackage3";
  const std::string fake_package4 = "com.package.fakepackage4";

  base::test::ScopedFeatureList list;
  list.InitWithFeaturesAndParameters(
      {{app_list_features::kEnableAppReinstallZeroState, feature_params}}, {});
  EXPECT_TRUE(app_provider_->ShouldShowAnything());
  SetStateTime(profile_.get(), fake_package2,
               app_list::ArcAppReinstallSearchProvider::kInstallTime,
               base::Time::Now() - base::TimeDelta::FromSeconds(30));
  // Expect this to now say we shouldn't show, since a package was installed
  // well within an install grace.
  EXPECT_FALSE(app_provider_->ShouldShowAnything());
  SetStateTime(profile_.get(), fake_package2,
               app_list::ArcAppReinstallSearchProvider::kInstallTime,
               base::Time::Now() - base::TimeDelta::FromDays(30));
  EXPECT_TRUE(app_provider_->ShouldShowAnything());

  // Testing for opens: if an a recommendation is opened within the grace
  // period, we won't show anything. That's 72 hours (per configuration here).
  SetStateTime(profile_.get(), fake_package3,
               app_list::ArcAppReinstallSearchProvider::kOpenTime,
               base::Time::Now() - base::TimeDelta::FromSeconds(30));
  EXPECT_FALSE(app_provider_->ShouldShowAnything());
  SetStateTime(profile_.get(), fake_package3,
               app_list::ArcAppReinstallSearchProvider::kOpenTime,
               base::Time::Now() - base::TimeDelta::FromDays(30));
  EXPECT_TRUE(app_provider_->ShouldShowAnything());

  // Testing for impression counts: If we've shown a result more than the
  // feature param "impression_count_limit", and the latest time we've shown it
  // is within the grace period, do not show anything.
  SetStateInt64(profile_.get(), fake_package4,
                app_list::ArcAppReinstallSearchProvider::kImpressionCount, 10);
  // no impression time is set, show.
  EXPECT_TRUE(app_provider_->ShouldShowAnything());
  // shown recently.
  SetStateTime(profile_.get(), fake_package4,
               app_list::ArcAppReinstallSearchProvider::kImpressionTime,
               base::Time::Now() - base::TimeDelta::FromSeconds(30));
  EXPECT_FALSE(app_provider_->ShouldShowAnything());
  // shown long ago.
  SetStateTime(profile_.get(), fake_package4,
               app_list::ArcAppReinstallSearchProvider::kImpressionTime,
               base::Time::Now() - base::TimeDelta::FromDays(30));
  EXPECT_TRUE(app_provider_->ShouldShowAnything());

  // Shown recently, but not frequently.
  SetStateInt64(profile_.get(), fake_package4,
                app_list::ArcAppReinstallSearchProvider::kImpressionCount, 3);
  SetStateTime(profile_.get(), fake_package4,
               app_list::ArcAppReinstallSearchProvider::kImpressionTime,
               base::Time::Now() - base::TimeDelta::FromSeconds(30));
  EXPECT_TRUE(app_provider_->ShouldShowAnything());
}

TEST_F(ArcAppReinstallSearchProviderTest, TestResultListComparison) {
  std::vector<std::unique_ptr<ChromeSearchResult>> a, b;
  EXPECT_TRUE(app_list::ArcAppReinstallSearchProvider::ResultsIdentical(a, b));
  a.emplace_back(new TestSearchResult);

  // different sizes.
  EXPECT_FALSE(app_list::ArcAppReinstallSearchProvider::ResultsIdentical(a, b));
  b.emplace_back(new TestSearchResult);
  EXPECT_TRUE(app_list::ArcAppReinstallSearchProvider::ResultsIdentical(a, b));
  // Different Titles.
  a[0]->SetTitle(u"fake_title");
  EXPECT_FALSE(app_list::ArcAppReinstallSearchProvider::ResultsIdentical(a, b));
  b[0]->SetTitle(u"fake_title");
  EXPECT_TRUE(app_list::ArcAppReinstallSearchProvider::ResultsIdentical(a, b));

  // Different ID.
  static_cast<TestSearchResult*>(a[0].get())->SetId("id");
  EXPECT_FALSE(app_list::ArcAppReinstallSearchProvider::ResultsIdentical(a, b));
  static_cast<TestSearchResult*>(b[0].get())->SetId(a[0]->id());
  EXPECT_TRUE(app_list::ArcAppReinstallSearchProvider::ResultsIdentical(a, b));

  // Different relevance
  a[0]->set_relevance(0.7);
  EXPECT_FALSE(app_list::ArcAppReinstallSearchProvider::ResultsIdentical(a, b));
  b[0]->set_relevance(a[0]->relevance());
  EXPECT_TRUE(app_list::ArcAppReinstallSearchProvider::ResultsIdentical(a, b));

  // Different Rating.
  a[0]->SetRating(0.3);
  EXPECT_FALSE(app_list::ArcAppReinstallSearchProvider::ResultsIdentical(a, b));
  b[0]->SetRating(a[0]->rating());
  EXPECT_TRUE(app_list::ArcAppReinstallSearchProvider::ResultsIdentical(a, b));

  // for icon equality.
  gfx::ImageSkia aimg =
                     *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
                         IDR_APP_DEFAULT_ICON),
                 bimg =
                     *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
                         IDR_EXTENSION_DEFAULT_ICON);
  ASSERT_FALSE(aimg.BackedBySameObjectAs(bimg));
  a[0]->SetIcon(aimg);
  b[0]->SetIcon(bimg);
  EXPECT_FALSE(app_list::ArcAppReinstallSearchProvider::ResultsIdentical(a, b));
  b[0]->SetIcon(aimg);
  EXPECT_TRUE(app_list::ArcAppReinstallSearchProvider::ResultsIdentical(a, b));
}
