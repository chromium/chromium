// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <sstream>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "components/services/app_service/app_service_mojom_impl.h"
#include "components/services/app_service/public/cpp/app_capability_access_cache.h"
#include "components/services/app_service/public/cpp/features.h"
#include "components/services/app_service/public/cpp/intent.h"
#include "components/services/app_service/public/cpp/intent_filter_util.h"
#include "components/services/app_service/public/cpp/intent_test_util.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "components/services/app_service/public/cpp/preferred_app.h"
#include "components/services/app_service/public/cpp/preferred_apps_list.h"
#include "components/services/app_service/public/cpp/publisher_base.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/clone_traits.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace apps {

class FakePublisher : public apps::PublisherBase {
 public:
  FakePublisher(AppServiceMojomImpl* impl,
                apps::mojom::AppType app_type,
                std::vector<std::string> initial_app_ids)
      : app_type_(app_type), known_app_ids_(std::move(initial_app_ids)) {
    mojo::PendingRemote<apps::mojom::Publisher> remote;
    receivers_.Add(this, remote.InitWithNewPipeAndPassReceiver());
    impl->RegisterPublisher(std::move(remote), app_type_);
  }

  void PublishMoreApps(std::vector<std::string> app_ids) {
    for (auto& subscriber : subscribers_) {
      CallOnApps(subscriber.get(), app_ids, /*uninstall=*/false);
    }
    for (const auto& app_id : app_ids) {
      known_app_ids_.push_back(app_id);
    }
  }

  void ModifyCapabilityAccess(const std::string& app_id,
                              absl::optional<bool> accessing_camera,
                              absl::optional<bool> accessing_microphone) {
    if (accessing_camera.has_value()) {
      if (accessing_camera.value()) {
        apps_accessing_camera_.insert(app_id);
      } else {
        apps_accessing_camera_.erase(app_id);
      }
    }

    if (accessing_microphone.has_value()) {
      if (accessing_microphone.value()) {
        apps_accessing_microphone_.insert(app_id);
      } else {
        apps_accessing_microphone_.erase(app_id);
      }
    }

    PublisherBase::ModifyCapabilityAccess(
        subscribers_, app_id, accessing_camera, accessing_microphone);
  }

  void UninstallApps(std::vector<std::string> app_ids,
                     AppServiceMojomImpl* impl) {
    for (auto& subscriber : subscribers_) {
      CallOnApps(subscriber.get(), app_ids, /*uninstall=*/true);
    }
    for (const auto& app_id : app_ids) {
      known_app_ids_.push_back(app_id);
      impl->RemovePreferredApp(app_type_, app_id);
    }
  }

  bool AppHasSupportedLinksPreference(const std::string& app_id) {
    return supported_link_apps_.find(app_id) != supported_link_apps_.end();
  }

  std::string load_icon_app_id;

 private:
  void Connect(mojo::PendingRemote<apps::mojom::Subscriber> subscriber_remote,
               apps::mojom::ConnectOptionsPtr opts) override {
    mojo::Remote<apps::mojom::Subscriber> subscriber(
        std::move(subscriber_remote));
    CallOnApps(subscriber.get(), known_app_ids_, /*uninstall=*/false);
    CallOnCapabilityAccesses(subscriber.get(), known_app_ids_);
    subscribers_.Add(std::move(subscriber));
  }

  void Launch(const std::string& app_id,
              int32_t event_flags,
              apps::mojom::LaunchSource launch_source,
              apps::mojom::WindowInfoPtr window_info) override {}

  void OnSupportedLinksPreferenceChanged(const std::string& app_id,
                                         bool open_in_app) override {
    if (open_in_app) {
      supported_link_apps_.insert(app_id);
    } else {
      supported_link_apps_.erase(app_id);
    }
  }

  void CallOnApps(apps::mojom::Subscriber* subscriber,
                  std::vector<std::string>& app_ids,
                  bool uninstall) {
    std::vector<apps::mojom::AppPtr> apps;
    for (const auto& app_id : app_ids) {
      auto app = apps::mojom::App::New();
      app->app_type = app_type_;
      app->app_id = app_id;
      if (uninstall) {
        app->readiness = apps::mojom::Readiness::kUninstalledByUser;
      }
      apps.push_back(std::move(app));
    }
    subscriber->OnApps(std::move(apps), app_type_,
                       false /* should_notify_initialized */);
  }

  void CallOnCapabilityAccesses(apps::mojom::Subscriber* subscriber,
                                std::vector<std::string>& app_ids) {
    std::vector<apps::mojom::CapabilityAccessPtr> capability_accesses;
    for (const auto& app_id : app_ids) {
      auto capability_access = apps::mojom::CapabilityAccess::New();
      capability_access->app_id = app_id;
      if (apps_accessing_camera_.find(app_id) != apps_accessing_camera_.end()) {
        capability_access->camera = apps::mojom::OptionalBool::kTrue;
      }
      if (apps_accessing_microphone_.find(app_id) !=
          apps_accessing_microphone_.end()) {
        capability_access->microphone = apps::mojom::OptionalBool::kTrue;
      }
      capability_accesses.push_back(std::move(capability_access));
    }
    subscriber->OnCapabilityAccesses(std::move(capability_accesses));
  }

  apps::mojom::AppType app_type_;
  std::vector<std::string> known_app_ids_;
  std::set<std::string> apps_accessing_camera_;
  std::set<std::string> apps_accessing_microphone_;
  std::set<std::string> supported_link_apps_;
  mojo::ReceiverSet<apps::mojom::Publisher> receivers_;
  mojo::RemoteSet<apps::mojom::Subscriber> subscribers_;
};

class FakeSubscriber : public apps::mojom::Subscriber {
 public:
  explicit FakeSubscriber(AppServiceMojomImpl* impl) {
    mojo::PendingRemote<apps::mojom::Subscriber> remote;
    receivers_.Add(this, remote.InitWithNewPipeAndPassReceiver());
    impl->RegisterSubscriber(std::move(remote), nullptr);
  }

  std::string AppIdsSeen() {
    std::stringstream ss;
    for (const auto& app_id : app_ids_seen_) {
      ss << app_id;
    }
    return ss.str();
  }

  std::string AppIdsAccessingCamera() {
    std::stringstream ss;
    for (const auto& app_id : cache_.GetAppsAccessingCamera()) {
      ss << app_id;
    }
    return ss.str();
  }

  std::string AppIdsAccessingMicrophone() {
    std::stringstream ss;
    for (const auto& app_id : cache_.GetAppsAccessingMicrophone()) {
      ss << app_id;
    }
    return ss.str();
  }

  PreferredAppsList& PreferredApps() { return preferred_apps_; }

 private:
  void OnApps(std::vector<apps::mojom::AppPtr> deltas,
              apps::mojom::AppType app_type,
              bool should_notify_initialized) override {
    for (const auto& delta : deltas) {
      app_ids_seen_.insert(delta->app_id);
      if (delta->readiness == apps::mojom::Readiness::kUninstalledByUser) {
        preferred_apps_.DeleteAppId(delta->app_id);
      }
    }
  }

  void OnCapabilityAccesses(
      std::vector<apps::mojom::CapabilityAccessPtr> deltas) override {
    cache_.OnCapabilityAccesses(std::move(deltas));
  }

  void Clone(mojo::PendingReceiver<apps::mojom::Subscriber> receiver) override {
    receivers_.Add(this, std::move(receiver));
  }

  void OnPreferredAppsChanged(
      apps::mojom::PreferredAppChangesPtr changes) override {
    preferred_apps_.ApplyBulkUpdate(
        ConvertMojomPreferredAppChangesToPreferredAppChanges(changes));
  }

  void InitializePreferredApps(
      std::vector<apps::mojom::PreferredAppPtr> mojom_preferred_apps) override {
    preferred_apps_.Init(
        ConvertMojomPreferredAppsToPreferredApps(mojom_preferred_apps));
  }

  mojo::ReceiverSet<apps::mojom::Subscriber> receivers_;
  std::set<std::string> app_ids_seen_;
  AppCapabilityAccessCache cache_;
  apps::PreferredAppsList preferred_apps_;
};

class AppServiceMojomImplTest : public testing::Test {
 protected:
  AppServiceMojomImplTest() {
    scoped_feature_list_.InitWithFeatures(
        {}, {kAppServicePreferredAppsWithoutMojom,
             kAppServiceCapabilityAccessWithoutMojom});
  }

  content::BrowserTaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(AppServiceMojomImplTest, PubSub) {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  AppServiceMojomImpl impl(temp_dir_.GetPath());

  // Start with one subscriber.
  FakeSubscriber sub0(&impl);
  impl.FlushMojoCallsForTesting();
  EXPECT_EQ("", sub0.AppIdsSeen());
  EXPECT_EQ("", sub0.AppIdsAccessingCamera());
  EXPECT_EQ("", sub0.AppIdsAccessingMicrophone());

  // Add one publisher.
  FakePublisher pub0(&impl, apps::mojom::AppType::kArc,
                     std::vector<std::string>{"A", "B"});
  impl.FlushMojoCallsForTesting();
  EXPECT_EQ("AB", sub0.AppIdsSeen());
  EXPECT_EQ("", sub0.AppIdsAccessingCamera());
  EXPECT_EQ("", sub0.AppIdsAccessingMicrophone());

  pub0.ModifyCapabilityAccess("B", absl::nullopt, absl::nullopt);
  impl.FlushMojoCallsForTesting();
  EXPECT_EQ("", sub0.AppIdsAccessingCamera());
  EXPECT_EQ("", sub0.AppIdsAccessingMicrophone());

  pub0.ModifyCapabilityAccess("B", true, absl::nullopt);
  impl.FlushMojoCallsForTesting();
  EXPECT_EQ("B", sub0.AppIdsAccessingCamera());
  EXPECT_EQ("", sub0.AppIdsAccessingMicrophone());

  // Have that publisher publish more apps.
  pub0.PublishMoreApps(std::vector<std::string>{"C", "D", "E"});
  impl.FlushMojoCallsForTesting();
  EXPECT_EQ("ABCDE", sub0.AppIdsSeen());

  pub0.ModifyCapabilityAccess("D", true, true);
  impl.FlushMojoCallsForTesting();
  EXPECT_EQ("BD", sub0.AppIdsAccessingCamera());
  EXPECT_EQ("D", sub0.AppIdsAccessingMicrophone());

  // Add a second publisher.
  FakePublisher pub1(&impl, apps::mojom::AppType::kBuiltIn,
                     std::vector<std::string>{"m"});
  impl.FlushMojoCallsForTesting();
  EXPECT_EQ("ABCDEm", sub0.AppIdsSeen());

  // Have both publishers publish more apps.
  pub0.PublishMoreApps(std::vector<std::string>{"F"});
  pub1.PublishMoreApps(std::vector<std::string>{"n"});
  pub0.ModifyCapabilityAccess("B", false, absl::nullopt);
  pub1.ModifyCapabilityAccess("n", absl::nullopt, true);
  impl.FlushMojoCallsForTesting();
  EXPECT_EQ("ABCDEFmn", sub0.AppIdsSeen());
  EXPECT_EQ("D", sub0.AppIdsAccessingCamera());
  EXPECT_EQ("Dn", sub0.AppIdsAccessingMicrophone());

  // Add a second subscriber.
  FakeSubscriber sub1(&impl);
  impl.FlushMojoCallsForTesting();
  EXPECT_EQ("ABCDEFmn", sub0.AppIdsSeen());
  EXPECT_EQ("ABCDEFmn", sub1.AppIdsSeen());
  EXPECT_EQ("D", sub0.AppIdsAccessingCamera());
  EXPECT_EQ("Dn", sub0.AppIdsAccessingMicrophone());
  EXPECT_EQ("D", sub1.AppIdsAccessingCamera());
  EXPECT_EQ("Dn", sub1.AppIdsAccessingMicrophone());

  // Publish more apps.
  pub0.ModifyCapabilityAccess("D", false, false);
  pub1.PublishMoreApps(std::vector<std::string>{"o", "p", "q"});
  pub1.ModifyCapabilityAccess("n", true, absl::nullopt);
  impl.FlushMojoCallsForTesting();
  EXPECT_EQ("ABCDEFmnopq", sub0.AppIdsSeen());
  EXPECT_EQ("ABCDEFmnopq", sub1.AppIdsSeen());
  EXPECT_EQ("n", sub0.AppIdsAccessingCamera());
  EXPECT_EQ("n", sub0.AppIdsAccessingMicrophone());
  EXPECT_EQ("n", sub1.AppIdsAccessingCamera());
  EXPECT_EQ("n", sub1.AppIdsAccessingMicrophone());

  // Add a third publisher.
  FakePublisher pub2(&impl, apps::mojom::AppType::kCrostini,
                     std::vector<std::string>{"$"});
  impl.FlushMojoCallsForTesting();
  EXPECT_EQ("$ABCDEFmnopq", sub0.AppIdsSeen());
  EXPECT_EQ("$ABCDEFmnopq", sub1.AppIdsSeen());

  // Publish more apps.
  pub2.PublishMoreApps(std::vector<std::string>{"&"});
  pub1.PublishMoreApps(std::vector<std::string>{"r"});
  pub0.PublishMoreApps(std::vector<std::string>{"G"});
  pub1.ModifyCapabilityAccess("n", false, false);
  pub2.ModifyCapabilityAccess("&", true, false);
  impl.FlushMojoCallsForTesting();
  EXPECT_EQ("$&ABCDEFGmnopqr", sub0.AppIdsSeen());
  EXPECT_EQ("$&ABCDEFGmnopqr", sub1.AppIdsSeen());
  EXPECT_EQ("&", sub0.AppIdsAccessingCamera());
  EXPECT_EQ("", sub0.AppIdsAccessingMicrophone());
  EXPECT_EQ("&", sub1.AppIdsAccessingCamera());
  EXPECT_EQ("", sub1.AppIdsAccessingMicrophone());
}

TEST_F(AppServiceMojomImplTest, PreferredApps) {
  // Test Initialize.
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  AppServiceMojomImpl impl(temp_dir_.GetPath());
  impl.GetPreferredAppsListForTesting().Init();

  const char kAppId1[] = "abcdefg";
  const char kAppId2[] = "aaaaaaa";
  GURL filter_url = GURL("https://www.google.com/abc");
  auto intent_filter = apps_util::CreateIntentFilterForUrlScope(filter_url);

  impl.GetPreferredAppsListForTesting().AddPreferredApp(
      kAppId1, ConvertMojomIntentFilterToIntentFilter(intent_filter));

  // Add one subscriber.
  FakeSubscriber sub0(&impl);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(sub0.PreferredApps().GetValue(),
            impl.GetPreferredAppsListForTesting().GetValue());

  // Add another subscriber.
  FakeSubscriber sub1(&impl);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(sub1.PreferredApps().GetValue(),
            impl.GetPreferredAppsListForTesting().GetValue());

  FakePublisher pub0(&impl, apps::mojom::AppType::kArc,
                     std::vector<std::string>{kAppId1, kAppId2});
  task_environment_.RunUntilIdle();

  // Test sync preferred app to all subscribers.
  filter_url = GURL("https://www.abc.com/");
  GURL another_filter_url = GURL("https://www.test.com/");
  intent_filter = apps_util::CreateIntentFilterForUrlScope(filter_url);
  auto another_intent_filter =
      apps_util::CreateIntentFilterForUrlScope(another_filter_url);

  task_environment_.RunUntilIdle();
  EXPECT_EQ(absl::nullopt,
            sub0.PreferredApps().FindPreferredAppForUrl(filter_url));
  EXPECT_EQ(absl::nullopt,
            sub1.PreferredApps().FindPreferredAppForUrl(filter_url));
  EXPECT_EQ(absl::nullopt,
            sub0.PreferredApps().FindPreferredAppForUrl(another_filter_url));
  EXPECT_EQ(absl::nullopt,
            sub1.PreferredApps().FindPreferredAppForUrl(another_filter_url));

  impl.AddPreferredApp(
      apps::mojom::AppType::kUnknown, kAppId2, intent_filter->Clone(),
      apps_util::CreateIntentFromUrl(filter_url), /*from_publisher=*/true);
  impl.AddPreferredApp(apps::mojom::AppType::kUnknown, kAppId2,
                       another_intent_filter->Clone(),
                       apps_util::CreateIntentFromUrl(another_filter_url),
                       /*from_publisher=*/true);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(kAppId2, sub0.PreferredApps().FindPreferredAppForUrl(filter_url));
  EXPECT_EQ(kAppId2, sub1.PreferredApps().FindPreferredAppForUrl(filter_url));
  EXPECT_EQ(kAppId2,
            sub0.PreferredApps().FindPreferredAppForUrl(another_filter_url));
  EXPECT_EQ(kAppId2,
            sub1.PreferredApps().FindPreferredAppForUrl(another_filter_url));

  // Test that uninstall removes all the settings for the app.
  pub0.UninstallApps(std::vector<std::string>{kAppId2}, &impl);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(absl::nullopt,
            sub0.PreferredApps().FindPreferredAppForUrl(filter_url));
  EXPECT_EQ(absl::nullopt,
            sub1.PreferredApps().FindPreferredAppForUrl(filter_url));
  EXPECT_EQ(absl::nullopt,
            sub0.PreferredApps().FindPreferredAppForUrl(another_filter_url));
  EXPECT_EQ(absl::nullopt,
            sub1.PreferredApps().FindPreferredAppForUrl(another_filter_url));

  impl.AddPreferredApp(
      apps::mojom::AppType::kUnknown, kAppId2, intent_filter->Clone(),
      apps_util::CreateIntentFromUrl(filter_url), /*from_publisher=*/true);
  impl.AddPreferredApp(apps::mojom::AppType::kUnknown, kAppId2,
                       another_intent_filter->Clone(),
                       apps_util::CreateIntentFromUrl(another_filter_url),
                       /*from_publisher=*/true);
  task_environment_.RunUntilIdle();

  EXPECT_EQ(kAppId2, sub0.PreferredApps().FindPreferredAppForUrl(filter_url));
  EXPECT_EQ(kAppId2, sub1.PreferredApps().FindPreferredAppForUrl(filter_url));
  EXPECT_EQ(kAppId2,
            sub0.PreferredApps().FindPreferredAppForUrl(another_filter_url));
  EXPECT_EQ(kAppId2,
            sub1.PreferredApps().FindPreferredAppForUrl(another_filter_url));
}

// Tests that writing a preferred app value before the PreferredAppsList is
// initialized queues the write for after initialization.
TEST_F(AppServiceMojomImplTest, PreferredAppsWriteBeforeInit) {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

  base::RunLoop run_loop_read;
  AppServiceMojomImpl impl(temp_dir_.GetPath(), run_loop_read.QuitClosure());
  GURL filter_url("https://www.abc.com/");

  std::string kAppId1 = "aaa";
  std::string kAppId2 = "bbb";

  impl.AddPreferredApp(apps::mojom::AppType::kArc, kAppId1,
                       apps_util::CreateIntentFilterForMimeType("image/png"),
                       nullptr,
                       /*from_publisher=*/false);

  std::vector<apps::mojom::IntentFilterPtr> filters;
  filters.push_back(apps_util::CreateIntentFilterForUrlScope(filter_url));
  impl.SetSupportedLinksPreference(apps::mojom::AppType::kArc, kAppId2,
                                   std::move(filters));

  // Wait for the preferred apps list initialization to read from disk.
  run_loop_read.Run();

  // Both changes to the PreferredAppsList should have been applied.
  std::vector<GURL> filesystem_urls(
      {GURL("filesystem:chrome://foo/image.png")});
  std::vector<std::string> mime_types({"image/png"});
  ASSERT_EQ(kAppId1,
            impl.GetPreferredAppsListForTesting().FindPreferredAppForIntent(
                apps_util::MakeShareIntent(filesystem_urls, mime_types)));
  ASSERT_EQ(
      kAppId2,
      impl.GetPreferredAppsListForTesting().FindPreferredAppForUrl(filter_url));
}

TEST_F(AppServiceMojomImplTest, PreferredAppsPersistency) {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

  const char kAppId1[] = "abcdefg";
  GURL filter_url = GURL("https://www.google.com/abc");
  auto intent_filter = apps_util::CreateIntentFilterForUrlScope(filter_url);
  {
    base::RunLoop run_loop_read;
    base::RunLoop run_loop_write;
    AppServiceMojomImpl impl(temp_dir_.GetPath(), run_loop_read.QuitClosure(),
                             run_loop_write.QuitClosure());
    impl.FlushMojoCallsForTesting();
    run_loop_read.Run();
    impl.AddPreferredApp(apps::mojom::AppType::kUnknown, kAppId1,
                         intent_filter->Clone(),
                         apps_util::CreateIntentFromUrl(filter_url),
                         /*from_publisher=*/false);
    run_loop_write.Run();
    impl.FlushMojoCallsForTesting();
  }
  // Create a new impl to initialize preferred apps from the disk.
  {
    base::RunLoop run_loop_read;
    AppServiceMojomImpl impl(temp_dir_.GetPath(), run_loop_read.QuitClosure());
    impl.FlushMojoCallsForTesting();
    run_loop_read.Run();
    EXPECT_EQ(kAppId1,
              impl.GetPreferredAppsListForTesting().FindPreferredAppForUrl(
                  filter_url));
  }
}

TEST_F(AppServiceMojomImplTest, PreferredAppsSetSupportedLinks) {
  // Test Initialize.
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  AppServiceMojomImpl impl(temp_dir_.GetPath());
  impl.GetPreferredAppsListForTesting().Init();

  const char kAppId1[] = "abcdefg";
  const char kAppId2[] = "hijklmn";
  const char kAppId3[] = "opqrstu";

  auto intent_filter_a =
      apps_util::CreateIntentFilterForUrlScope(GURL("https://www.a.com/"));
  auto intent_filter_b =
      apps_util::CreateIntentFilterForUrlScope(GURL("https://www.b.com/"));
  auto intent_filter_c =
      apps_util::CreateIntentFilterForUrlScope(GURL("https://www.c.com/"));

  FakeSubscriber sub0(&impl);
  task_environment_.RunUntilIdle();

  FakePublisher pub0(&impl, apps::mojom::AppType::kArc,
                     std::vector<std::string>{kAppId1, kAppId2, kAppId3});
  task_environment_.RunUntilIdle();

  std::vector<apps::mojom::IntentFilterPtr> app_1_filters;
  app_1_filters.push_back(intent_filter_a.Clone());
  app_1_filters.push_back(intent_filter_b.Clone());
  impl.SetSupportedLinksPreference(apps::mojom::AppType::kArc, kAppId1,
                                   std::move(app_1_filters));

  std::vector<apps::mojom::IntentFilterPtr> app_2_filters;
  app_2_filters.push_back(intent_filter_c.Clone());
  impl.SetSupportedLinksPreference(apps::mojom::AppType::kArc, kAppId2,
                                   std::move(app_2_filters));

  task_environment_.RunUntilIdle();

  EXPECT_TRUE(pub0.AppHasSupportedLinksPreference(kAppId1));
  EXPECT_TRUE(pub0.AppHasSupportedLinksPreference(kAppId2));
  EXPECT_FALSE(pub0.AppHasSupportedLinksPreference(kAppId3));

  EXPECT_EQ(kAppId1, sub0.PreferredApps().FindPreferredAppForUrl(
                         GURL("https://www.a.com/")));
  EXPECT_EQ(kAppId1, sub0.PreferredApps().FindPreferredAppForUrl(
                         GURL("https://www.b.com/")));
  EXPECT_EQ(kAppId2, sub0.PreferredApps().FindPreferredAppForUrl(
                         GURL("https://www.c.com/")));

  // App 3 overlaps with both App 1 and 2. Both previous apps should have all
  // their supported link filters removed.
  std::vector<apps::mojom::IntentFilterPtr> app_3_filters;
  app_3_filters.push_back(intent_filter_b.Clone());
  app_3_filters.push_back(intent_filter_c.Clone());
  impl.SetSupportedLinksPreference(apps::mojom::AppType::kArc, kAppId3,
                                   std::move(app_3_filters));

  task_environment_.RunUntilIdle();
  EXPECT_FALSE(pub0.AppHasSupportedLinksPreference(kAppId1));
  EXPECT_FALSE(pub0.AppHasSupportedLinksPreference(kAppId2));
  EXPECT_TRUE(pub0.AppHasSupportedLinksPreference(kAppId3));

  EXPECT_EQ(absl::nullopt, sub0.PreferredApps().FindPreferredAppForUrl(
                               GURL("https://www.a.com/")));
  EXPECT_EQ(kAppId3, sub0.PreferredApps().FindPreferredAppForUrl(
                         GURL("https://www.b.com/")));
  EXPECT_EQ(kAppId3, sub0.PreferredApps().FindPreferredAppForUrl(
                         GURL("https://www.c.com/")));

  // Setting App 3 as preferred again should not change anything.
  app_3_filters = std::vector<apps::mojom::IntentFilterPtr>();
  app_3_filters.push_back(intent_filter_b.Clone());
  app_3_filters.push_back(intent_filter_c.Clone());
  impl.SetSupportedLinksPreference(apps::mojom::AppType::kArc, kAppId3,
                                   std::move(app_3_filters));
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(pub0.AppHasSupportedLinksPreference(kAppId3));
  EXPECT_EQ(kAppId3, sub0.PreferredApps().FindPreferredAppForUrl(
                         GURL("https://www.c.com/")));

  impl.RemoveSupportedLinksPreference(apps::mojom::AppType::kArc, kAppId3);
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(pub0.AppHasSupportedLinksPreference(kAppId3));
  EXPECT_EQ(absl::nullopt, sub0.PreferredApps().FindPreferredAppForUrl(
                               GURL("https://www.c.com/")));
}

// Test that app with overlapped works properly.
TEST_F(AppServiceMojomImplTest, PreferredAppsOverlap) {
  // Test Initialize.
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  AppServiceMojomImpl impl(temp_dir_.GetPath());
  impl.GetPreferredAppsListForTesting().Init();

  const char kAppId1[] = "abcdefg";
  const char kAppId2[] = "hijklmn";

  GURL filter_url_1 = GURL("https://www.google.com/abc");
  GURL filter_url_2 = GURL("http://www.google.com.au/abc");
  GURL filter_url_3 = GURL("https://www.abc.com/abc");

  auto intent_filter_1 = apps_util::CreateIntentFilterForUrlScope(filter_url_1);
  apps_util::AddConditionValue(
      apps::mojom::ConditionType::kScheme, filter_url_2.scheme(),
      apps::mojom::PatternMatchType::kLiteral, intent_filter_1);
  apps_util::AddConditionValue(
      apps::mojom::ConditionType::kHost, filter_url_2.host(),
      apps::mojom::PatternMatchType::kLiteral, intent_filter_1);

  auto intent_filter_2 = apps_util::CreateIntentFilterForUrlScope(filter_url_3);
  apps_util::AddConditionValue(
      apps::mojom::ConditionType::kScheme, filter_url_2.scheme(),
      apps::mojom::PatternMatchType::kLiteral, intent_filter_2);
  apps_util::AddConditionValue(
      apps::mojom::ConditionType::kHost, filter_url_2.host(),
      apps::mojom::PatternMatchType::kLiteral, intent_filter_2);

  auto intent_filter_3 = apps_util::CreateIntentFilterForUrlScope(filter_url_1);

  FakeSubscriber sub0(&impl);
  task_environment_.RunUntilIdle();

  EXPECT_EQ(absl::nullopt,
            sub0.PreferredApps().FindPreferredAppForUrl(filter_url_1));
  EXPECT_EQ(absl::nullopt,
            sub0.PreferredApps().FindPreferredAppForUrl(filter_url_2));
  EXPECT_EQ(absl::nullopt,
            sub0.PreferredApps().FindPreferredAppForUrl(filter_url_3));
  EXPECT_EQ(0U, impl.GetPreferredAppsListForTesting().GetEntrySize());
  EXPECT_EQ(0U, sub0.PreferredApps().GetEntrySize());

  impl.AddPreferredApp(
      apps::mojom::AppType::kArc, kAppId1, intent_filter_1->Clone(),
      apps_util::CreateIntentFromUrl(filter_url_1), /*from_publisher=*/true);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(kAppId1, sub0.PreferredApps().FindPreferredAppForUrl(filter_url_1));
  EXPECT_EQ(kAppId1, sub0.PreferredApps().FindPreferredAppForUrl(filter_url_2));
  EXPECT_EQ(absl::nullopt,
            sub0.PreferredApps().FindPreferredAppForUrl(filter_url_3));
  EXPECT_EQ(1U, impl.GetPreferredAppsListForTesting().GetEntrySize());
  EXPECT_EQ(1U, sub0.PreferredApps().GetEntrySize());

  // Add preferred app with intent filter overlap with existing entry for
  // another app will reset the preferred app setting for the other app.
  impl.AddPreferredApp(
      apps::mojom::AppType::kArc, kAppId2, intent_filter_2->Clone(),
      apps_util::CreateIntentFromUrl(filter_url_1), /*from_publisher=*/true);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(absl::nullopt,
            sub0.PreferredApps().FindPreferredAppForUrl(filter_url_1));
  EXPECT_EQ(kAppId2, sub0.PreferredApps().FindPreferredAppForUrl(filter_url_2));
  EXPECT_EQ(kAppId2, sub0.PreferredApps().FindPreferredAppForUrl(filter_url_3));
  EXPECT_EQ(1U, impl.GetPreferredAppsListForTesting().GetEntrySize());
  EXPECT_EQ(1U, sub0.PreferredApps().GetEntrySize());
}

// Test that app with overlapped supported links works properly.
TEST_F(AppServiceMojomImplTest, PreferredAppsOverlapSupportedLink) {
  // Test Initialize.
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  AppServiceMojomImpl impl(temp_dir_.GetPath());
  impl.GetPreferredAppsListForTesting().Init();

  const char kAppId1[] = "abcdefg";
  const char kAppId2[] = "hijklmn";

  GURL filter_url_1 = GURL("https://www.google.com/abc");
  GURL filter_url_2 = GURL("http://www.google.com.au/abc");
  GURL filter_url_3 = GURL("https://www.abc.com/abc");

  auto intent_filter_1 = apps_util::CreateIntentFilterForUrlScope(filter_url_1);
  apps_util::AddConditionValue(
      apps::mojom::ConditionType::kScheme, filter_url_2.scheme(),
      apps::mojom::PatternMatchType::kLiteral, intent_filter_1);
  apps_util::AddConditionValue(
      apps::mojom::ConditionType::kHost, filter_url_2.host(),
      apps::mojom::PatternMatchType::kLiteral, intent_filter_1);

  auto intent_filter_2 = apps_util::CreateIntentFilterForUrlScope(filter_url_3);
  apps_util::AddConditionValue(
      apps::mojom::ConditionType::kScheme, filter_url_2.scheme(),
      apps::mojom::PatternMatchType::kLiteral, intent_filter_2);
  apps_util::AddConditionValue(
      apps::mojom::ConditionType::kHost, filter_url_2.host(),
      apps::mojom::PatternMatchType::kLiteral, intent_filter_2);

  auto intent_filter_3 = apps_util::CreateIntentFilterForUrlScope(filter_url_1);

  std::vector<apps::mojom::IntentFilterPtr> app_1_filters;
  app_1_filters.push_back(std::move(intent_filter_1));
  app_1_filters.push_back(std::move(intent_filter_2));
  std::vector<apps::mojom::IntentFilterPtr> app_2_filters;
  app_2_filters.push_back(std::move(intent_filter_3));

  FakeSubscriber sub0(&impl);
  task_environment_.RunUntilIdle();

  FakePublisher pub0(&impl, apps::mojom::AppType::kArc,
                     std::vector<std::string>{kAppId1, kAppId2});
  task_environment_.RunUntilIdle();

  EXPECT_EQ(absl::nullopt,
            sub0.PreferredApps().FindPreferredAppForUrl(filter_url_1));
  EXPECT_EQ(absl::nullopt,
            sub0.PreferredApps().FindPreferredAppForUrl(filter_url_2));
  EXPECT_EQ(absl::nullopt,
            sub0.PreferredApps().FindPreferredAppForUrl(filter_url_3));
  EXPECT_EQ(0U, impl.GetPreferredAppsListForTesting().GetEntrySize());
  EXPECT_EQ(0U, sub0.PreferredApps().GetEntrySize());

  // Test that add preferred app with overlapped filters for same app will
  // add all entries.
  impl.SetSupportedLinksPreference(apps::mojom::AppType::kArc, kAppId1,
                                   mojo::Clone(app_1_filters));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(kAppId1, sub0.PreferredApps().FindPreferredAppForUrl(filter_url_1));
  EXPECT_EQ(kAppId1, sub0.PreferredApps().FindPreferredAppForUrl(filter_url_2));
  EXPECT_EQ(kAppId1, sub0.PreferredApps().FindPreferredAppForUrl(filter_url_3));
  EXPECT_TRUE(pub0.AppHasSupportedLinksPreference(kAppId1));
  EXPECT_FALSE(pub0.AppHasSupportedLinksPreference(kAppId2));
  EXPECT_EQ(2U, impl.GetPreferredAppsListForTesting().GetEntrySize());
  EXPECT_EQ(2U, sub0.PreferredApps().GetEntrySize());

  // Test that add preferred app with another app that has overlapped filter
  // will clear all entries from the original app.
  impl.SetSupportedLinksPreference(apps::mojom::AppType::kArc, kAppId2,
                                   mojo::Clone(app_2_filters));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(kAppId2, sub0.PreferredApps().FindPreferredAppForUrl(filter_url_1));
  EXPECT_EQ(absl::nullopt,
            sub0.PreferredApps().FindPreferredAppForUrl(filter_url_2));
  EXPECT_EQ(absl::nullopt,
            sub0.PreferredApps().FindPreferredAppForUrl(filter_url_3));
  EXPECT_FALSE(pub0.AppHasSupportedLinksPreference(kAppId1));
  EXPECT_TRUE(pub0.AppHasSupportedLinksPreference(kAppId2));
  EXPECT_EQ(1U, impl.GetPreferredAppsListForTesting().GetEntrySize());
  EXPECT_EQ(1U, sub0.PreferredApps().GetEntrySize());

  // Test that setting back to app 1 works.
  impl.SetSupportedLinksPreference(apps::mojom::AppType::kArc, kAppId1,
                                   mojo::Clone(app_1_filters));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(kAppId1, sub0.PreferredApps().FindPreferredAppForUrl(filter_url_1));
  EXPECT_EQ(kAppId1, sub0.PreferredApps().FindPreferredAppForUrl(filter_url_2));
  EXPECT_EQ(kAppId1, sub0.PreferredApps().FindPreferredAppForUrl(filter_url_3));
  EXPECT_TRUE(pub0.AppHasSupportedLinksPreference(kAppId1));
  EXPECT_FALSE(pub0.AppHasSupportedLinksPreference(kAppId2));
  EXPECT_EQ(2U, impl.GetPreferredAppsListForTesting().GetEntrySize());
  EXPECT_EQ(2U, sub0.PreferredApps().GetEntrySize());
}

// Test that duplicated entry will not be added.
TEST_F(AppServiceMojomImplTest, PreferredAppsDuplicated) {
  // Test Initialize.
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  AppServiceMojomImpl impl(temp_dir_.GetPath());
  impl.GetPreferredAppsListForTesting().Init();

  const char kAppId1[] = "abcdefg";

  GURL filter_url = GURL("https://www.google.com/abc");

  auto intent_filter = apps_util::CreateIntentFilterForUrlScope(filter_url);

  FakeSubscriber sub0(&impl);
  task_environment_.RunUntilIdle();

  EXPECT_EQ(absl::nullopt,
            sub0.PreferredApps().FindPreferredAppForUrl(filter_url));
  EXPECT_EQ(0U, impl.GetPreferredAppsListForTesting().GetEntrySize());
  EXPECT_EQ(0U, sub0.PreferredApps().GetEntrySize());

  impl.AddPreferredApp(
      apps::mojom::AppType::kArc, kAppId1, intent_filter->Clone(),
      apps_util::CreateIntentFromUrl(filter_url), /*from_publisher=*/true);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(kAppId1, sub0.PreferredApps().FindPreferredAppForUrl(filter_url));
  EXPECT_EQ(1U, impl.GetPreferredAppsListForTesting().GetEntrySize());
  EXPECT_EQ(1U, sub0.PreferredApps().GetEntrySize());

  impl.AddPreferredApp(
      apps::mojom::AppType::kArc, kAppId1, intent_filter->Clone(),
      apps_util::CreateIntentFromUrl(filter_url), /*from_publisher=*/true);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(kAppId1, sub0.PreferredApps().FindPreferredAppForUrl(filter_url));
  EXPECT_EQ(1U, impl.GetPreferredAppsListForTesting().GetEntrySize());
  EXPECT_EQ(1U, sub0.PreferredApps().GetEntrySize());
}

// Test that duplicated entry will not be added for supported links.
TEST_F(AppServiceMojomImplTest, PreferredAppsDuplicatedSupportedLink) {
  // Test Initialize.
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  AppServiceMojomImpl impl(temp_dir_.GetPath());
  impl.GetPreferredAppsListForTesting().Init();

  const char kAppId1[] = "abcdefg";

  GURL filter_url_1 = GURL("https://www.google.com/abc");
  GURL filter_url_2 = GURL("http://www.google.com.au/abc");
  GURL filter_url_3 = GURL("https://www.abc.com/abc");

  auto intent_filter_1 = apps_util::CreateIntentFilterForUrlScope(filter_url_1);

  auto intent_filter_2 = apps_util::CreateIntentFilterForUrlScope(filter_url_2);

  auto intent_filter_3 = apps_util::CreateIntentFilterForUrlScope(filter_url_3);

  std::vector<apps::mojom::IntentFilterPtr> app_1_filters;
  app_1_filters.push_back(std::move(intent_filter_1));
  app_1_filters.push_back(std::move(intent_filter_2));
  app_1_filters.push_back(std::move(intent_filter_3));

  FakeSubscriber sub0(&impl);
  task_environment_.RunUntilIdle();

  FakePublisher pub0(&impl, apps::mojom::AppType::kArc,
                     std::vector<std::string>{kAppId1});
  task_environment_.RunUntilIdle();

  EXPECT_EQ(absl::nullopt,
            sub0.PreferredApps().FindPreferredAppForUrl(filter_url_1));
  EXPECT_EQ(absl::nullopt,
            sub0.PreferredApps().FindPreferredAppForUrl(filter_url_2));
  EXPECT_EQ(absl::nullopt,
            sub0.PreferredApps().FindPreferredAppForUrl(filter_url_3));
  EXPECT_EQ(0U, impl.GetPreferredAppsListForTesting().GetEntrySize());
  EXPECT_EQ(0U, sub0.PreferredApps().GetEntrySize());

  impl.SetSupportedLinksPreference(apps::mojom::AppType::kArc, kAppId1,
                                   mojo::Clone(app_1_filters));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(kAppId1, sub0.PreferredApps().FindPreferredAppForUrl(filter_url_1));
  EXPECT_EQ(kAppId1, sub0.PreferredApps().FindPreferredAppForUrl(filter_url_2));
  EXPECT_EQ(kAppId1, sub0.PreferredApps().FindPreferredAppForUrl(filter_url_3));
  EXPECT_TRUE(pub0.AppHasSupportedLinksPreference(kAppId1));

  EXPECT_EQ(3U, impl.GetPreferredAppsListForTesting().GetEntrySize());
  EXPECT_EQ(3U, sub0.PreferredApps().GetEntrySize());

  impl.SetSupportedLinksPreference(apps::mojom::AppType::kArc, kAppId1,
                                   mojo::Clone(app_1_filters));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(kAppId1, sub0.PreferredApps().FindPreferredAppForUrl(filter_url_1));
  EXPECT_EQ(kAppId1, sub0.PreferredApps().FindPreferredAppForUrl(filter_url_2));
  EXPECT_EQ(kAppId1, sub0.PreferredApps().FindPreferredAppForUrl(filter_url_3));
  EXPECT_TRUE(pub0.AppHasSupportedLinksPreference(kAppId1));

  EXPECT_EQ(3U, impl.GetPreferredAppsListForTesting().GetEntrySize());
  EXPECT_EQ(3U, sub0.PreferredApps().GetEntrySize());
}

}  // namespace apps
