// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/app_service_mojom_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/threading/scoped_blocking_call.h"
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
    base::OnceClosure write_completed_for_testing)
    : preferred_apps_(this,
                      profile_dir,
                      std::move(read_completed_for_testing),
                      std::move(write_completed_for_testing)) {}

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
  if (preferred_apps_.preferred_apps_list_.IsInitialized()) {
    subscriber->InitializePreferredApps(
        preferred_apps_.preferred_apps_list_.GetValue());
  }

  // Add the new subscriber to the set.
  subscribers_.Add(std::move(subscriber));
}

void AppServiceMojomImpl::LoadIcon(apps::mojom::AppType app_type,
                                   const std::string& app_id,
                                   apps::mojom::IconKeyPtr icon_key,
                                   apps::mojom::IconType icon_type,
                                   int32_t size_hint_in_dip,
                                   bool allow_placeholder_icon,
                                   LoadIconCallback callback) {
  auto iter = publishers_.find(app_type);
  if (iter == publishers_.end()) {
    std::move(callback).Run(apps::mojom::IconValue::New());
    return;
  }
  iter->second->LoadIcon(app_id, std::move(icon_key), icon_type,
                         size_hint_in_dip, allow_placeholder_icon,
                         std::move(callback));
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
  preferred_apps_.AddPreferredApp(app_type, app_id, std::move(intent_filter),
                                  std::move(intent), from_publisher);
}

void AppServiceMojomImpl::RemovePreferredApp(apps::mojom::AppType app_type,
                                             const std::string& app_id) {
  preferred_apps_.RemovePreferredApp(app_type, app_id);
}

void AppServiceMojomImpl::RemovePreferredAppForFilter(
    apps::mojom::AppType app_type,
    const std::string& app_id,
    apps::mojom::IntentFilterPtr intent_filter) {
  preferred_apps_.RemovePreferredAppForFilter(app_type, app_id,
                                              std::move(intent_filter));
}

void AppServiceMojomImpl::SetSupportedLinksPreference(
    apps::mojom::AppType app_type,
    const std::string& app_id,
    std::vector<apps::mojom::IntentFilterPtr> all_link_filters) {
  preferred_apps_.SetSupportedLinksPreference(app_type, app_id,
                                              std::move(all_link_filters));
}

void AppServiceMojomImpl::RemoveSupportedLinksPreference(
    apps::mojom::AppType app_type,
    const std::string& app_id) {
  preferred_apps_.RemoveSupportedLinksPreference(app_type, app_id);
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
  for (auto& subscriber : subscribers_) {
    subscriber->InitializePreferredApps(
        preferred_apps_.preferred_apps_list_.GetValue());
  }
}

void AppServiceMojomImpl::OnPreferredAppsChanged(
    apps::mojom::PreferredAppChangesPtr changes) {
  for (auto& subscriber : subscribers_) {
    subscriber->OnPreferredAppsChanged(changes->Clone());
  }
}

void AppServiceMojomImpl::OnPreferredAppSet(
    const std::string& app_id,
    apps::mojom::IntentFilterPtr intent_filter,
    apps::mojom::IntentPtr intent,
    apps::mojom::ReplacedAppPreferencesPtr replaced_app_preferences) {
  for (const auto& iter : publishers_) {
    iter.second->OnPreferredAppSet(app_id, intent_filter->Clone(),
                                   intent->Clone(),
                                   replaced_app_preferences->Clone());
  }
}

void AppServiceMojomImpl::OnSupportedLinksPreferenceChanged(
    const std::string& app_id,
    bool open_in_app) {
  for (const auto& iter : publishers_) {
    iter.second->OnSupportedLinksPreferenceChanged(app_id, open_in_app);
  }
}

apps::mojom::Publisher* AppServiceMojomImpl::GetPublisher(
    apps::mojom::AppType app_type) {
  auto iter = publishers_.find(app_type);
  if (iter == publishers_.end()) {
    return nullptr;
  }
  return iter->second.get();
}

PreferredAppsList& AppServiceMojomImpl::GetPreferredAppsListForTesting() {
  return preferred_apps_.preferred_apps_list_;
}

void AppServiceMojomImpl::OnPublisherDisconnected(
    apps::mojom::AppType app_type) {
  publishers_.erase(app_type);
}

}  // namespace apps
