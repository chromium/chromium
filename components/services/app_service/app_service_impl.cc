// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/app_service_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/debug/dump_without_crashing.h"
#include "base/files/file_util.h"
#include "base/json/json_string_value_serializer.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/token.h"
#include "components/services/app_service/public/cpp/preferred_apps_converter.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "content/public/browser/browser_thread.h"

namespace {

const base::FilePath::CharType kPreferredAppsDirname[] =
    FILE_PATH_LITERAL("PreferredApps");

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class PreferredAppsFileIOAction {
  kWriteSuccess = 0,
  kWriteFailed = 1,
  kReadSuccess = 2,
  kReadFailed = 3,
  kMaxValue = kReadFailed,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class PreferredAppsUpdateAction {
  kAdd = 0,
  kDeleteForFilter = 1,
  kDeleteForAppId = 2,
  kUpgraded = 3,
  kMaxValue = kUpgraded,
};

void Connect(apps::mojom::Publisher* publisher,
             apps::mojom::Subscriber* subscriber) {
  mojo::PendingRemote<apps::mojom::Subscriber> clone;
  subscriber->Clone(clone.InitWithNewPipeAndPassReceiver());
  // TODO: replace nullptr with a ConnectOptions.
  publisher->Connect(std::move(clone), nullptr);
}

void LogPreferredAppFileIOAction(PreferredAppsFileIOAction action) {
  UMA_HISTOGRAM_ENUMERATION("Apps.PreferredApps.FileIOAction", action);
}

void LogPreferredAppUpdateAction(PreferredAppsUpdateAction action) {
  UMA_HISTOGRAM_ENUMERATION("Apps.PreferredApps.UpdateAction", action);
}

void LogPreferredAppEntryCount(int entry_count) {
  base::UmaHistogramCounts10000("Apps.PreferredApps.EntryCount", entry_count);
}

// Performs blocking I/O. Called on another thread.
void WriteDataBlocking(const base::FilePath& preferred_apps_file,
                       const std::string& preferred_apps) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  bool write_success =
      base::WriteFile(preferred_apps_file, preferred_apps.c_str(),
                      preferred_apps.size()) != -1;
  if (write_success) {
    LogPreferredAppFileIOAction(PreferredAppsFileIOAction::kWriteSuccess);
  } else {
    DVLOG(0) << "Fail to write preferred apps to " << preferred_apps_file;
    LogPreferredAppFileIOAction(PreferredAppsFileIOAction::kWriteFailed);
  }
}

// Performs blocking I/O. Called on another thread.
std::string ReadDataBlocking(const base::FilePath& preferred_apps_file) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  std::string preferred_apps_string;
  bool read_success =
      base::ReadFileToString(preferred_apps_file, &preferred_apps_string);
  if (read_success) {
    LogPreferredAppFileIOAction(PreferredAppsFileIOAction::kReadSuccess);
  } else {
    LogPreferredAppFileIOAction(PreferredAppsFileIOAction::kReadFailed);
  }
  return preferred_apps_string;
}

}  // namespace

namespace apps {

AppServiceImpl::AppServiceImpl(const base::FilePath& profile_dir,
                               bool is_share_intents_supported,
                               base::OnceClosure read_completed_for_testing,
                               base::OnceClosure write_completed_for_testing)
    : profile_dir_(profile_dir),
      is_share_intents_supported_(is_share_intents_supported),
      should_write_preferred_apps_to_file_(false),
      writing_preferred_apps_(false),
      task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::ThreadPool(), base::MayBlock(),
           base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})),
      read_completed_for_testing_(std::move(read_completed_for_testing)),
      write_completed_for_testing_(std::move(write_completed_for_testing)) {
  InitializePreferredApps();
}

AppServiceImpl::~AppServiceImpl() = default;

void AppServiceImpl::BindReceiver(
    mojo::PendingReceiver<apps::mojom::AppService> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void AppServiceImpl::FlushMojoCallsForTesting() {
  subscribers_.FlushForTesting();
  receivers_.FlushForTesting();
}

void AppServiceImpl::RegisterPublisher(
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
      base::BindOnce(&AppServiceImpl::OnPublisherDisconnected,
                     base::Unretained(this), app_type));
  auto result = publishers_.emplace(app_type, std::move(publisher));
  CHECK(result.second);
}

void AppServiceImpl::RegisterSubscriber(
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
  if (preferred_apps_.IsInitialized()) {
    subscriber->InitializePreferredApps(preferred_apps_.GetValue());
  }

  // Add the new subscriber to the set.
  subscribers_.Add(std::move(subscriber));
}

void AppServiceImpl::LoadIcon(apps::mojom::AppType app_type,
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

void AppServiceImpl::Launch(apps::mojom::AppType app_type,
                            const std::string& app_id,
                            int32_t event_flags,
                            apps::mojom::LaunchSource launch_source,
                            int64_t display_id) {
  auto iter = publishers_.find(app_type);
  if (iter == publishers_.end()) {
    return;
  }
  iter->second->Launch(app_id, event_flags, launch_source, display_id);
}
void AppServiceImpl::LaunchAppWithFiles(apps::mojom::AppType app_type,
                                        const std::string& app_id,
                                        apps::mojom::LaunchContainer container,
                                        int32_t event_flags,
                                        apps::mojom::LaunchSource launch_source,
                                        apps::mojom::FilePathsPtr file_paths) {
  auto iter = publishers_.find(app_type);
  if (iter == publishers_.end()) {
    return;
  }
  iter->second->LaunchAppWithFiles(app_id, container, event_flags,
                                   launch_source, std::move(file_paths));
}

void AppServiceImpl::LaunchAppWithIntent(
    apps::mojom::AppType app_type,
    const std::string& app_id,
    int32_t event_flags,
    apps::mojom::IntentPtr intent,
    apps::mojom::LaunchSource launch_source,
    int64_t display_id) {
  auto iter = publishers_.find(app_type);
  if (iter == publishers_.end()) {
    return;
  }
  iter->second->LaunchAppWithIntent(app_id, event_flags, std::move(intent),
                                    launch_source, display_id);
}

void AppServiceImpl::SetPermission(apps::mojom::AppType app_type,
                                   const std::string& app_id,
                                   apps::mojom::PermissionPtr permission) {
  auto iter = publishers_.find(app_type);
  if (iter == publishers_.end()) {
    return;
  }
  iter->second->SetPermission(app_id, std::move(permission));
}

void AppServiceImpl::Uninstall(apps::mojom::AppType app_type,
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

void AppServiceImpl::PauseApp(apps::mojom::AppType app_type,
                              const std::string& app_id) {
  auto iter = publishers_.find(app_type);
  if (iter == publishers_.end()) {
    return;
  }
  iter->second->PauseApp(app_id);
}

void AppServiceImpl::UnpauseApps(apps::mojom::AppType app_type,
                                 const std::string& app_id) {
  auto iter = publishers_.find(app_type);
  if (iter == publishers_.end()) {
    return;
  }
  iter->second->UnpauseApps(app_id);
}

void AppServiceImpl::StopApp(apps::mojom::AppType app_type,
                             const std::string& app_id) {
  auto iter = publishers_.find(app_type);
  if (iter == publishers_.end()) {
    return;
  }
  iter->second->StopApp(app_id);
}

void AppServiceImpl::GetMenuModel(apps::mojom::AppType app_type,
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

void AppServiceImpl::OpenNativeSettings(apps::mojom::AppType app_type,
                                        const std::string& app_id) {
  auto iter = publishers_.find(app_type);
  if (iter == publishers_.end()) {
    return;
  }
  iter->second->OpenNativeSettings(app_id);
}

void AppServiceImpl::AddPreferredApp(apps::mojom::AppType app_type,
                                     const std::string& app_id,
                                     apps::mojom::IntentFilterPtr intent_filter,
                                     apps::mojom::IntentPtr intent,
                                     bool from_publisher) {
  // TODO(crbug.com/853604): Make sure the ARC preference init happens after
  // this. Might need to change the interface to call that after read completed.
  // Might also need to record the change before data read and make the update
  // after initialization in the future.
  if (!preferred_apps_.IsInitialized()) {
    DVLOG(0) << "Preferred apps not initialised when try to add.";
    return;
  }

  // TODO(https://crbug.com/853604): Remove this and convert to a DCHECK
  // after finding out the root cause.
  if (app_id.empty()) {
    base::debug::DumpWithoutCrashing();
    return;
  }

  apps::mojom::ReplacedAppPreferencesPtr replaced_app_preferences =
      preferred_apps_.AddPreferredApp(app_id, intent_filter);

  LogPreferredAppUpdateAction(PreferredAppsUpdateAction::kAdd);

  WriteToJSON(profile_dir_, preferred_apps_);

  for (auto& subscriber : subscribers_) {
    subscriber->OnPreferredAppSet(app_id, intent_filter->Clone());
  }

  if (from_publisher || !intent) {
    return;
  }

  // Sync the change to publishers. Because |replaced_app_preference| can
  // be any app type, we should run this for all publishers. Currently
  // only implemented in ARC publisher.
  // TODO(crbug.com/853604): The |replaced_app_preference| can be really big,
  // update this logic to only call the relevant publisher for each app after
  // updating the storage structure.
  for (const auto& iter : publishers_) {
    iter.second->OnPreferredAppSet(app_id, intent_filter->Clone(),
                                   intent->Clone(),
                                   replaced_app_preferences->Clone());
  }
}

void AppServiceImpl::RemovePreferredApp(apps::mojom::AppType app_type,
                                        const std::string& app_id) {
  // TODO(crbug.com/853604): Make sure the ARC preference init happens after
  // this. Might need to change the interface to call that after read completed.
  // Might also need to record the change before data read and make the update
  // after initialization in the future.
  if (!preferred_apps_.IsInitialized()) {
    DVLOG(0) << "Preferred apps not initialised when try to remove an app id.";
    return;
  }

  preferred_apps_.DeleteAppId(app_id);

  LogPreferredAppUpdateAction(PreferredAppsUpdateAction::kDeleteForAppId);

  WriteToJSON(profile_dir_, preferred_apps_);
}

void AppServiceImpl::RemovePreferredAppForFilter(
    apps::mojom::AppType app_type,
    const std::string& app_id,
    apps::mojom::IntentFilterPtr intent_filter) {
  // TODO(crbug.com/853604): Make sure the ARC preference init happens after
  // this. Might need to change the interface to call that after read completed.
  // Might also need to record the change before data read and make the update
  // after initialization in the future.
  if (!preferred_apps_.IsInitialized()) {
    DVLOG(0) << "Preferred apps not initialised when try to remove a filter.";
    return;
  }

  preferred_apps_.DeletePreferredApp(app_id, intent_filter);

  WriteToJSON(profile_dir_, preferred_apps_);

  for (auto& subscriber : subscribers_) {
    subscriber->OnPreferredAppRemoved(app_id, intent_filter->Clone());
  }

  LogPreferredAppUpdateAction(PreferredAppsUpdateAction::kDeleteForFilter);
}

PreferredAppsList& AppServiceImpl::GetPreferredAppsForTesting() {
  return preferred_apps_;
}

void AppServiceImpl::OnPublisherDisconnected(apps::mojom::AppType app_type) {
  publishers_.erase(app_type);
}

void AppServiceImpl::InitializePreferredApps() {
  ReadFromJSON(profile_dir_);
}

void AppServiceImpl::WriteToJSON(
    const base::FilePath& profile_dir,
    const apps::PreferredAppsList& preferred_apps) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // If currently is writing preferred apps to file, set a flag to write after
  // the current write completed.
  if (writing_preferred_apps_) {
    should_write_preferred_apps_to_file_ = true;
    return;
  }

  writing_preferred_apps_ = true;

  auto preferred_apps_value = apps::ConvertPreferredAppsToValue(
      preferred_apps.GetReference(), is_share_intents_supported_);

  std::string json_string;
  JSONStringValueSerializer serializer(&json_string);
  serializer.Serialize(preferred_apps_value);

  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&WriteDataBlocking,
                     profile_dir.Append(kPreferredAppsDirname), json_string),
      base::BindOnce(&AppServiceImpl::WriteCompleted,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AppServiceImpl::WriteCompleted() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  writing_preferred_apps_ = false;
  if (!should_write_preferred_apps_to_file_) {
    // Call the testing callback if it is set.
    if (write_completed_for_testing_) {
      std::move(write_completed_for_testing_).Run();
    }
    return;
  }
  // If need to perform another write, write the most up to date preferred apps
  // from memory to file.
  should_write_preferred_apps_to_file_ = false;
  WriteToJSON(profile_dir_, preferred_apps_);
}

void AppServiceImpl::ReadFromJSON(const base::FilePath& profile_dir) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&ReadDataBlocking,
                     profile_dir.Append(kPreferredAppsDirname)),
      base::BindOnce(&AppServiceImpl::ReadCompleted,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AppServiceImpl::ReadCompleted(std::string preferred_apps_string) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  bool preferred_apps_upgraded = false;
  if (preferred_apps_string.empty()) {
    preferred_apps_.Init();
  } else {
    std::string json_string;
    JSONStringValueDeserializer deserializer(preferred_apps_string);
    int error_code;
    std::string error_message;
    auto preferred_apps_value =
        deserializer.Deserialize(&error_code, &error_message);

    if (!preferred_apps_value) {
      DVLOG(0) << "Fail to deserialize json value from string with error code: "
               << error_code << " and error message: " << error_message;
      preferred_apps_.Init();
    } else {
      preferred_apps_upgraded = IsUpgradedForSharing(*preferred_apps_value);
      auto preferred_apps =
          apps::ParseValueToPreferredApps(*preferred_apps_value);
      if (is_share_intents_supported_ && !preferred_apps_upgraded) {
        UpgradePreferredApps(preferred_apps);
      }
      preferred_apps_.Init(preferred_apps);
    }
  }
  if (is_share_intents_supported_ && !preferred_apps_upgraded) {
    LogPreferredAppUpdateAction(PreferredAppsUpdateAction::kUpgraded);
    WriteToJSON(profile_dir_, preferred_apps_);
  }

  for (auto& subscriber : subscribers_) {
    subscriber->InitializePreferredApps(preferred_apps_.GetValue());
  }
  if (read_completed_for_testing_) {
    std::move(read_completed_for_testing_).Run();
  }

  LogPreferredAppEntryCount(preferred_apps_.GetEntrySize());
}

}  // namespace apps
