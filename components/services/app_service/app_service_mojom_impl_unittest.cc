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
    }
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

 private:
  void OnApps(std::vector<apps::mojom::AppPtr> deltas,
              apps::mojom::AppType app_type,
              bool should_notify_initialized) override {
    for (const auto& delta : deltas) {
      app_ids_seen_.insert(delta->app_id);
    }
  }

  void OnCapabilityAccesses(
      std::vector<apps::mojom::CapabilityAccessPtr> deltas) override {
    cache_.OnCapabilityAccesses(std::move(deltas));
  }

  void Clone(mojo::PendingReceiver<apps::mojom::Subscriber> receiver) override {
    receivers_.Add(this, std::move(receiver));
  }

  mojo::ReceiverSet<apps::mojom::Subscriber> receivers_;
  std::set<std::string> app_ids_seen_;
  AppCapabilityAccessCache cache_;
};

class AppServiceMojomImplTest : public testing::Test {
 protected:
  AppServiceMojomImplTest() {
    scoped_feature_list_.InitWithFeatures(
        {}, {kAppServiceCapabilityAccessWithoutMojom});
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

}  // namespace apps
