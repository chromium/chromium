// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/app_service_mojom_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/threading/scoped_blocking_call.h"
#include "components/services/app_service/public/cpp/features.h"
#include "components/services/app_service/public/cpp/intent_filter_util.h"
#include "components/services/app_service/public/cpp/preferred_apps_list.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "mojo/public/cpp/bindings/clone_traits.h"

namespace {

void Connect(apps::mojom::Publisher* publisher,
             apps::mojom::Subscriber* subscriber) {
  mojo::PendingRemote<apps::mojom::Subscriber> clone;
  subscriber->Clone(clone.InitWithNewPipeAndPassReceiver());
  // TODO: replace nullptr with a ConnectOptions.
  publisher->Connect(std::move(clone), nullptr);
}

}  // namespace

namespace apps {

AppServiceMojomImpl::AppServiceMojomImpl(
    const base::FilePath& profile_dir,
    base::OnceClosure read_completed_for_testing,
    base::OnceClosure write_completed_for_testing) {
  if (!base::FeatureList::IsEnabled(kAppServicePreferredAppsWithoutMojom)) {
    preferred_apps_impl_ = std::make_unique<PreferredAppsImpl>(
        this, profile_dir, std::move(read_completed_for_testing),
        std::move(write_completed_for_testing));
  }
}

AppServiceMojomImpl::~AppServiceMojomImpl() = default;

void AppServiceMojomImpl::BindReceiver(
    mojo::PendingReceiver<apps::mojom::AppService> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void AppServiceMojomImpl::FlushMojoCallsForTesting() {
  subscribers_.FlushForTesting();
  receivers_.FlushForTesting();
}

void AppServiceMojomImpl::RegisterPublisher(
    mojo::PendingRemote<apps::mojom::Publisher> publisher_remote,
    apps::mojom::AppType app_type) {
  mojo::Remote<apps::mojom::Publisher> publisher(std::move(publisher_remote));
  // Connect the new publisher with every registered subscriber.
  for (auto& subscriber : subscribers_) {
    ::Connect(publisher.get(), subscriber.get());
  }

  // Check that no previous publisher has registered for the same app_type.
  CHECK(publishers_.find(app_type) == publishers_.end());

  // Add the new publisher to the set.
  publisher.set_disconnect_handler(
      base::BindOnce(&AppServiceMojomImpl::OnPublisherDisconnected,
                     base::Unretained(this), app_type));
  auto result = publishers_.emplace(app_type, std::move(publisher));
  CHECK(result.second);
}

void AppServiceMojomImpl::RegisterSubscriber(
    mojo::PendingRemote<apps::mojom::Subscriber> subscriber_remote,
    apps::mojom::ConnectOptionsPtr opts) {
  // Connect the new subscriber with every registered publisher.
  mojo::Remote<apps::mojom::Subscriber> subscriber(
      std::move(subscriber_remote));
  for (const auto& iter : publishers_) {
    ::Connect(iter.second.get(), subscriber.get());
  }

  // TODO: store the opts somewhere.

  // Initialise the Preferred Apps in the Subscribers on register.
  if (preferred_apps_impl_ &&
      preferred_apps_impl_->preferred_apps_list_.IsInitialized()) {
    subscriber->InitializePreferredApps(
        ConvertPreferredAppsToMojomPreferredApps(
            preferred_apps_impl_->preferred_apps_list_.GetValue()));
  }

  // Add the new subscriber to the set.
  subscribers_.Add(std::move(subscriber));
}

void AppServiceMojomImpl::Launch(apps::mojom::AppType app_type,
                                 const std::string& app_id,
                                 int32_t event_flags,
                                 apps::mojom::LaunchSource launch_source,
                                 apps::mojom::WindowInfoPtr window_info) {
  auto iter = publishers_.find(app_type);
  if (iter == publishers_.end()) {
    return;
  }
  iter->second->Launch(app_id, event_flags, launch_source,
                       std::move(window_info));
}
void AppServiceMojomImpl::LaunchAppWithFiles(
    apps::mojom::AppType app_type,
    const std::string& app_id,
    int32_t event_flags,
    apps::mojom::LaunchSource launch_source,
    apps::mojom::FilePathsPtr file_paths) {
  CHECK(file_paths);
  auto iter = publishers_.find(app_type);
  if (iter == publishers_.end()) {
    return;
  }
  iter->second->LaunchAppWithFiles(app_id, event_flags, launch_source,
                                   std::move(file_paths));
}

void AppServiceMojomImpl::LaunchAppWithIntent(
    apps::mojom::AppType app_type,
    const std::string& app_id,
    int32_t event_flags,
    apps::mojom::IntentPtr intent,
    apps::mojom::LaunchSource launch_source,
    apps::mojom::WindowInfoPtr window_info,
    LaunchAppWithIntentCallback callback) {
  auto iter = publishers_.find(app_type);
  if (iter == publishers_.end()) {
    std::move(callback).Run(/*success=*/false);
    return;
  }
  iter->second->LaunchAppWithIntent(app_id, event_flags, std::move(intent),
                                    launch_source, std::move(window_info),
                                    std::move(callback));
}

void AppServiceMojomImpl::SetPermission(apps::mojom::AppType app_type,
                                        const std::string& app_id,
                                        apps::mojom::PermissionPtr permission) {
  auto iter = publishers_.find(app_type);
  if (iter == publishers_.end()) {
    return;
  }
  iter->second->SetPermission(app_id, std::move(permission));
}

void AppServiceMojomImpl::Uninstall(
    apps::mojom::AppType app_type,
    const std::string& app_id,
    apps::mojom::UninstallSource uninstall_source,
    bool clear_site_data,
    bool report_abuse) {
  auto iter = publishers_.find(app_type);
  if (iter == publishers_.end()) {
    return;
  }
  iter->second->Uninstall(app_id, uninstall_source, clear_site_data,
                          report_abuse);
}

void AppServiceMojomImpl::PauseApp(apps::mojom::AppType app_type,
                                   const std::string& app_id) {
  auto iter = publishers_.find(app_type);
  if (iter == publishers_.end()) {
    return;
  }
  iter->second->PauseApp(app_id);
}

void AppServiceMojomImpl::UnpauseApp(apps::mojom::AppType app_type,
                                     const std::string& app_id) {
  auto iter = publishers_.find(app_type);
  if (iter == publishers_.end()) {
    return;
  }
  iter->second->UnpauseApp(app_id);
}

void AppServiceMojomImpl::StopApp(apps::mojom::AppType app_type,
                                  const std::string& app_id) {
  auto iter = publishers_.find(app_type);
  if (iter == publishers_.end()) {
    return;
  }
  iter->second->StopApp(app_id);
}

void AppServiceMojomImpl::GetMenuModel(apps::mojom::AppType app_type,
                                       const std::string& app_id,
                                       apps::mojom::MenuType menu_type,
                                       int64_t display_id,
                                       GetMenuModelCallback callback) {
  auto iter = publishers_.find(app_type);
  if (iter == publishers_.end()) {
    std::move(callback).Run(apps::mojom::MenuItems::New());
    return;
  }

  iter->second->GetMenuModel(app_id, menu_type, display_id,
                             std::move(callback));
}

void AppServiceMojomImpl::ExecuteContextMenuCommand(
    apps::mojom::AppType app_type,
    const std::string& app_id,
    int command_id,
    const std::string& shortcut_id,
    int64_t display_id) {
  auto iter = publishers_.find(app_type);
  if (iter == publishers_.end()) {
    return;
  }

  iter->second->ExecuteContextMenuCommand(app_id, command_id, shortcut_id,
                                          display_id);
}

void AppServiceMojomImpl::OpenNativeSettings(apps::mojom::AppType app_type,
                                             const std::string& app_id) {
  auto iter = publishers_.find(app_type);
  if (iter == publishers_.end()) {
    return;
  }
  iter->second->OpenNativeSettings(app_id);
}

void AppServiceMojomImpl::AddPreferredApp(
    apps::mojom::AppType app_type,
    const std::string& app_id,
    apps::mojom::IntentFilterPtr intent_filter,
    apps::mojom::IntentPtr intent,
    bool from_publisher) {
  if (preferred_apps_impl_) {
    preferred_apps_impl_->AddPreferredApp(
        ConvertMojomAppTypToAppType(app_type), app_id,
        ConvertMojomIntentFilterToIntentFilter(intent_filter),
        ConvertMojomIntentToIntent(intent), from_publisher);
  }
}

void AppServiceMojomImpl::RemovePreferredApp(apps::mojom::AppType app_type,
                                             const std::string& app_id) {
  if (preferred_apps_impl_) {
    preferred_apps_impl_->RemovePreferredApp(app_id);
  }
}

void AppServiceMojomImpl::SetSupportedLinksPreference(
    apps::mojom::AppType app_type,
    const std::string& app_id,
    std::vector<apps::mojom::IntentFilterPtr> all_link_filters) {
  if (preferred_apps_impl_) {
    preferred_apps_impl_->SetSupportedLinksPreference(
        ConvertMojomAppTypToAppType(app_type), app_id,
        ConvertMojomIntentFiltersToIntentFilters(all_link_filters));
  }
}

void AppServiceMojomImpl::RemoveSupportedLinksPreference(
    apps::mojom::AppType app_type,
    const std::string& app_id) {
  if (preferred_apps_impl_) {
    preferred_apps_impl_->RemoveSupportedLinksPreference(
        ConvertMojomAppTypToAppType(app_type), app_id);
  }
}

void AppServiceMojomImpl::SetResizeLocked(apps::mojom::AppType app_type,
                                          const std::string& app_id,
                                          mojom::OptionalBool locked) {
  auto iter = publishers_.find(app_type);
  if (iter == publishers_.end()) {
    return;
  }
  iter->second->SetResizeLocked(app_id, locked);
}

void AppServiceMojomImpl::SetWindowMode(apps::mojom::AppType app_type,
                                        const std::string& app_id,
                                        apps::mojom::WindowMode window_mode) {
  auto iter = publishers_.find(app_type);
  if (iter == publishers_.end()) {
    return;
  }
  iter->second->SetWindowMode(app_id, window_mode);
}

void AppServiceMojomImpl::SetRunOnOsLoginMode(
    apps::mojom::AppType app_type,
    const std::string& app_id,
    apps::mojom::RunOnOsLoginMode run_on_os_login_mode) {
  auto iter = publishers_.find(app_type);
  if (iter == publishers_.end()) {
    return;
  }
  iter->second->SetRunOnOsLoginMode(app_id, run_on_os_login_mode);
}

void AppServiceMojomImpl::InitializePreferredAppsForAllSubscribers() {
  if (!preferred_apps_impl_) {
    return;
  }

  for (auto& subscriber : subscribers_) {
    subscriber->InitializePreferredApps(
        ConvertPreferredAppsToMojomPreferredApps(
            preferred_apps_impl_->preferred_apps_list_.GetValue()));
  }
}

void AppServiceMojomImpl::OnPreferredAppsChanged(
    PreferredAppChangesPtr changes) {
  for (auto& subscriber : subscribers_) {
    subscriber->OnPreferredAppsChanged(
        ConvertPreferredAppChangesToMojomPreferredAppChanges(changes));
  }
}

void AppServiceMojomImpl::OnPreferredAppSet(
    const std::string& app_id,
    IntentFilterPtr intent_filter,
    IntentPtr intent,
    ReplacedAppPreferences replaced_app_preferences) {
  for (const auto& iter : publishers_) {
    iter.second->OnPreferredAppSet(
        app_id, ConvertIntentFilterToMojomIntentFilter(intent_filter),
        ConvertIntentToMojomIntent(intent),
        ConvertReplacedAppPreferencesToMojomReplacedAppPreferences(
            replaced_app_preferences));
  }
}

void AppServiceMojomImpl::OnSupportedLinksPreferenceChanged(
    const std::string& app_id,
    bool open_in_app) {
  for (const auto& iter : publishers_) {
    iter.second->OnSupportedLinksPreferenceChanged(app_id, open_in_app);
  }
}

void AppServiceMojomImpl::OnSupportedLinksPreferenceChanged(
    AppType app_type,
    const std::string& app_id,
    bool open_in_app) {
  publishers_[ConvertAppTypeToMojomAppType(app_type)]
      ->OnSupportedLinksPreferenceChanged(app_id, open_in_app);
}

bool AppServiceMojomImpl::HasPublisher(AppType app_type) {
  return base::Contains(publishers_, ConvertAppTypeToMojomAppType(app_type));
}

PreferredAppsList& AppServiceMojomImpl::GetPreferredAppsListForTesting() {
  return preferred_apps_impl_->preferred_apps_list_;
}

void AppServiceMojomImpl::OnPublisherDisconnected(
    apps::mojom::AppType app_type) {
  publishers_.erase(app_type);
}

}  // namespace apps
