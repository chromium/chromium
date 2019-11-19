// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/test/fake_app_instance.h"

#include <stdint.h>

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "components/arc/app/arc_playstore_search_request_state.h"

namespace mojo {

template <>
struct TypeConverter<arc::mojom::AppInfoPtr, arc::mojom::AppInfo> {
  static arc::mojom::AppInfoPtr Convert(const arc::mojom::AppInfo& app_info) {
    return app_info.Clone();
  }
};

template <>
struct TypeConverter<arc::mojom::ArcPackageInfoPtr,
                     arc::mojom::ArcPackageInfo> {
  static arc::mojom::ArcPackageInfoPtr Convert(
      const arc::mojom::ArcPackageInfo& package_info) {
    return package_info.Clone();
  }
};

}  // namespace mojo

namespace arc {

FakeAppInstance::FakeAppInstance(mojom::AppHost* app_host)
    : app_host_(app_host) {}
FakeAppInstance::~FakeAppInstance() {}

void FakeAppInstance::InitDeprecated(mojom::AppHostPtr host_ptr) {
  Init(std::move(host_ptr), base::DoNothing());
}

void FakeAppInstance::Init(mojom::AppHostPtr host_ptr, InitCallback callback) {
  host_ = std::move(host_ptr);
  std::move(callback).Run();
}

void FakeAppInstance::LaunchAppDeprecated(
    const std::string& package_name,
    const std::string& activity,
    const base::Optional<gfx::Rect>& dimension) {
  LaunchApp(package_name, activity, 0);
}

void FakeAppInstance::LaunchApp(const std::string& package_name,
                                const std::string& activity,
                                int64_t display_id) {
  launch_requests_.push_back(std::make_unique<Request>(package_name, activity));
}

void FakeAppInstance::LaunchAppShortcutItem(const std::string& package_name,
                                            const std::string& shortcut_id,
                                            int64_t display_id) {
  ++launch_app_shortcut_item_count_;
}

void FakeAppInstance::RequestAppIcon(const std::string& package_name,
                                     const std::string& activity,
                                     int dimension,
                                     RequestAppIconCallback callback) {
  icon_requests_.push_back(
      std::make_unique<IconRequest>(package_name, activity, dimension));

  std::string png_data_as_string;
  if (GenerateIconResponse(dimension, true /* app_icon */,
                           &png_data_as_string)) {
    std::move(callback).Run(std::vector<uint8_t>(png_data_as_string.begin(),
                                                 png_data_as_string.end()));
  }
}

void FakeAppInstance::SendRefreshAppList(
    const std::vector<mojom::AppInfo>& apps) {
  std::vector<mojom::AppInfoPtr> v;
  for (const auto& app : apps)
    v.emplace_back(app.Clone());
  app_host_->OnAppListRefreshed(std::move(v));
}

void FakeAppInstance::SendPackageAppListRefreshed(
    const std::string& package_name,
    const std::vector<mojom::AppInfo>& apps) {
  std::vector<mojom::AppInfoPtr> v;
  for (const auto& app : apps)
    v.emplace_back(app.Clone());
  app_host_->OnPackageAppListRefreshed(package_name, std::move(v));
}

void FakeAppInstance::SendInstallShortcuts(
    const std::vector<mojom::ShortcutInfo>& shortcuts) {
  for (auto& shortcut : shortcuts)
    SendInstallShortcut(shortcut);
}

void FakeAppInstance::SendInstallShortcut(const mojom::ShortcutInfo& shortcut) {
  app_host_->OnInstallShortcut(shortcut.Clone());
}

void FakeAppInstance::SendUninstallShortcut(const std::string& package_name,
                                            const std::string& intent_uri) {
  app_host_->OnUninstallShortcut(package_name, intent_uri);
}

void FakeAppInstance::SendAppAdded(const mojom::AppInfo& app) {
  app_host_->OnAppAddedDeprecated(mojom::AppInfo::From(app));
}

void FakeAppInstance::SendTaskCreated(int32_t taskId,
                                      const mojom::AppInfo& app,
                                      const std::string& intent) {
  app_host_->OnTaskCreated(taskId, app.package_name, app.activity, app.name,
                           intent);
}

void FakeAppInstance::SendTaskDescription(
    int32_t taskId,
    const std::string& label,
    const std::string& icon_png_data_as_string) {
  app_host_->OnTaskDescriptionUpdated(
      taskId, label,
      std::vector<uint8_t>(icon_png_data_as_string.begin(),
                           icon_png_data_as_string.end()));
}

void FakeAppInstance::SendTaskDestroyed(int32_t taskId) {
  app_host_->OnTaskDestroyed(taskId);
}

bool FakeAppInstance::GetIconResponse(int dimension,
                                      std::string* png_data_as_string) {
  const auto previous_response = icon_responses_.find(dimension);
  if (previous_response == icon_responses_.end())
    return false;
  *png_data_as_string = previous_response->second;
  return true;
}

bool FakeAppInstance::GenerateIconResponse(int dimension,
                                           bool app_icon,
                                           std::string* png_data_as_string) {
  DCHECK(png_data_as_string != nullptr);

  auto previous_response = icon_responses_.find(dimension);
  if (previous_response != icon_responses_.end())
    icon_responses_.erase(previous_response);

  if (icon_response_type_ == IconResponseType::ICON_RESPONSE_SKIP)
    return false;

  if (icon_response_type_ == IconResponseType::ICON_RESPONSE_SEND_BAD) {
    *png_data_as_string = "BAD_ICON_CONTENT";
    icon_responses_[dimension] = *png_data_as_string;
    return true;
  }

  base::FilePath base_path;
  CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &base_path));
  base::FilePath icon_file_path =
      base_path.AppendASCII("components")
          .AppendASCII("test")
          .AppendASCII("data")
          .AppendASCII("arc")
          .AppendASCII(base::StringPrintf(
              "icon_%s_%d.png", app_icon ? "app" : "shortcut", dimension));
  {
    base::ScopedAllowBlockingForTesting allow_io;
    CHECK(base::PathExists(icon_file_path)) << icon_file_path.MaybeAsASCII();
    CHECK(base::ReadFileToString(icon_file_path, png_data_as_string));
  }
  icon_responses_[dimension] = *png_data_as_string;
  return true;
}

bool FakeAppInstance::GetFakeIcon(mojom::ScaleFactor scale_factor,
                                  std::string* png_data_as_string) {
  CHECK(png_data_as_string != nullptr);
  std::string icon_file_name;
  switch (scale_factor) {
    case mojom::ScaleFactor::SCALE_FACTOR_100P:
      icon_file_name = "icon_100p.png";
      break;
    case mojom::ScaleFactor::SCALE_FACTOR_125P:
      icon_file_name = "icon_125p.png";
      break;
    case mojom::ScaleFactor::SCALE_FACTOR_133P:
      icon_file_name = "icon_133p.png";
      break;
    case mojom::ScaleFactor::SCALE_FACTOR_140P:
      icon_file_name = "icon_140p.png";
      break;
    case mojom::ScaleFactor::SCALE_FACTOR_150P:
      icon_file_name = "icon_150p.png";
      break;
    case mojom::ScaleFactor::SCALE_FACTOR_180P:
      icon_file_name = "icon_180p.png";
      break;
    case mojom::ScaleFactor::SCALE_FACTOR_200P:
      icon_file_name = "icon_200p.png";
      break;
    case mojom::ScaleFactor::SCALE_FACTOR_250P:
      icon_file_name = "icon_250p.png";
      break;
    case mojom::ScaleFactor::SCALE_FACTOR_300P:
      icon_file_name = "icon_300p.png";
      break;
    default:
      NOTREACHED();
      return false;
  }

  base::FilePath base_path;
  CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &base_path));
  base::FilePath icon_file_path = base_path.AppendASCII("components")
                                      .AppendASCII("test")
                                      .AppendASCII("data")
                                      .AppendASCII("arc")
                                      .AppendASCII(icon_file_name);
  CHECK(base::PathExists(icon_file_path));
  CHECK(base::ReadFileToString(icon_file_path, png_data_as_string));

  return true;
}

void FakeAppInstance::SetTaskInfo(int32_t task_id,
                                  const std::string& package_name,
                                  const std::string& activity) {
  task_id_to_info_[task_id].reset(new Request(package_name, activity));
}

void FakeAppInstance::SendRefreshPackageList(
    std::vector<mojom::ArcPackageInfoPtr> packages) {
  app_host_->OnPackageListRefreshed(std::move(packages));
}

void FakeAppInstance::SendPackageAdded(mojom::ArcPackageInfoPtr package) {
  app_host_->OnPackageAdded(std::move(package));
}

void FakeAppInstance::SendPackageModified(mojom::ArcPackageInfoPtr package) {
  app_host_->OnPackageModified(std::move(package));
}

void FakeAppInstance::SendPackageUninstalled(const std::string& package_name) {
  app_host_->OnPackageRemoved(package_name);
}

void FakeAppInstance::SendInstallationStarted(const std::string& package_name) {
  app_host_->OnInstallationStarted(package_name);
}

void FakeAppInstance::SendInstallationFinished(const std::string& package_name,
                                               bool success) {
  mojom::InstallationResult result;
  result.package_name = package_name;
  result.success = success;
  app_host_->OnInstallationFinished(
      mojom::InstallationResultPtr(result.Clone()));
}

void FakeAppInstance::CanHandleResolutionDeprecated(
    const std::string& package_name,
    const std::string& activity,
    const gfx::Rect& dimension,
    CanHandleResolutionDeprecatedCallback callback) {
  std::move(callback).Run(true);
}

void FakeAppInstance::UninstallPackage(const std::string& package_name) {
  app_host_->OnPackageRemoved(package_name);
}

void FakeAppInstance::GetTaskInfo(int32_t task_id,
                                  GetTaskInfoCallback callback) {
  TaskIdToInfo::const_iterator it = task_id_to_info_.find(task_id);
  if (it == task_id_to_info_.end()) {
    std::move(callback).Run(std::string(), std::string());
    return;
  }
  std::move(callback).Run(it->second->package_name(), it->second->activity());
}

void FakeAppInstance::SetTaskActive(int32_t task_id) {}

void FakeAppInstance::CloseTask(int32_t task_id) {}

void FakeAppInstance::ShowPackageInfoDeprecated(
    const std::string& package_name,
    const gfx::Rect& dimension_on_screen) {}

void FakeAppInstance::ShowPackageInfoOnPageDeprecated(
    const std::string& package_name,
    mojom::ShowPackageInfoPage page,
    const gfx::Rect& dimension_on_screen) {}

void FakeAppInstance::ShowPackageInfoOnPage(const std::string& package_name,
                                            mojom::ShowPackageInfoPage page,
                                            int64_t display_id) {}

void FakeAppInstance::SetNotificationsEnabled(const std::string& package_name,
                                              bool enabled) {}

void FakeAppInstance::InstallPackage(mojom::ArcPackageInfoPtr arcPackageInfo) {
  app_host_->OnPackageAdded(std::move(arcPackageInfo));
}

void FakeAppInstance::GetAndroidId(GetAndroidIdCallback callback) {
  std::move(callback).Run(android_id_);
}

void FakeAppInstance::GetRecentAndSuggestedAppsFromPlayStore(
    const std::string& query,
    int32_t max_results,
    GetRecentAndSuggestedAppsFromPlayStoreCallback callback) {
  // Fake Play Store app info
  std::vector<arc::mojom::AppDiscoveryResultPtr> fake_apps;

  // Check if we're fabricating failed query.
  const std::string kFailedQueryPrefix("FailedQueryWithCode-");
  ArcPlayStoreSearchRequestState state_code =
      ArcPlayStoreSearchRequestState::SUCCESS;
  if (!query.compare(0, kFailedQueryPrefix.size(), kFailedQueryPrefix)) {
    state_code = static_cast<ArcPlayStoreSearchRequestState>(
        stoi(query.substr(kFailedQueryPrefix.size())));
    std::move(callback).Run(state_code, std::move(fake_apps));
    return;
  }

  // Fake icon data.
  std::string png_data_as_string;
  GetFakeIcon(mojom::ScaleFactor::SCALE_FACTOR_100P, &png_data_as_string);
  std::vector<uint8_t> fake_icon_png_data(png_data_as_string.begin(),
                                          png_data_as_string.end());

  fake_apps.push_back(mojom::AppDiscoveryResult::New(
      std::string("LauncherIntentUri"),        // launch_intent_uri
      std::string("InstallIntentUri"),         // install_intent_uri
      std::string(query),                      // label
      false,                                   // is_instant_app
      false,                                   // is_recent
      std::string("Publisher"),                // publisher_name
      std::string("$7.22"),                    // formatted_price
      5,                                       // review_score
      fake_icon_png_data,                      // icon_png_data
      std::string("com.google.android.gm")));  // package_name

  for (int i = 0; i < max_results - 1; ++i) {
    fake_apps.push_back(mojom::AppDiscoveryResult::New(
        base::StringPrintf("LauncherIntentUri %d", i),  // launch_intent_uri
        base::StringPrintf("InstallIntentUri %d", i),   // install_intent_uri
        base::StringPrintf("%s %d", query.c_str(), i),  // label
        i % 2 == 0,                                     // is_instant_app
        i % 4 == 0,                                     // is_recent
        base::StringPrintf("Publisher %d", i),          // publisher_name
        base::StringPrintf("$%d.22", i),                // formatted_price
        i,                                              // review_score
        fake_icon_png_data,                             // icon_png_data
        base::StringPrintf("test.package.%d", i)));     // package_name
  }

  std::move(callback).Run(state_code, std::move(fake_apps));
}

void FakeAppInstance::GetIcingGlobalQueryResults(
    const std::string& query,
    int32_t max_results,
    GetIcingGlobalQueryResultsCallback callback) {
  // Fake successful app data search results.
  std::vector<arc::mojom::AppDataResultPtr> fake_app_data_results;

  // Fake icon data.
  std::string png_data_as_string;
  GetFakeIcon(mojom::ScaleFactor::SCALE_FACTOR_100P, &png_data_as_string);
  std::vector<uint8_t> fake_icon_png_data(png_data_as_string.begin(),
                                          png_data_as_string.end());

  int i = 0;
  fake_app_data_results.push_back(mojom::AppDataResult::New(
      base::StringPrintf("LauncherIntentUri %d", i),
      base::StringPrintf("Label %s %d", query.c_str(), i),
      base::StringPrintf("Text %s %d", query.c_str(), i), fake_icon_png_data,
      mojom::AppDataResultType::PERSON));
  ++i;
  fake_app_data_results.push_back(mojom::AppDataResult::New(
      base::StringPrintf("LauncherIntentUri %d", i),
      base::StringPrintf("Label %s %d", query.c_str(), i),
      base::StringPrintf("Text %s %d", query.c_str(), i), fake_icon_png_data,
      mojom::AppDataResultType::NOTE_DOCUMENT));

  std::move(callback).Run(arc::mojom::AppDataRequestState::REQUEST_SUCCESS,
                          std::move(fake_app_data_results));
}

void FakeAppInstance::GetAppShortcutGlobalQueryItems(
    const std::string& query,
    int32_t max_results,
    GetAppShortcutGlobalQueryItemsCallback callback) {
  // Fake app shortcut items results.
  std::vector<mojom::AppShortcutItemPtr> fake_app_shortcut_items;

  // Fake icon data.
  std::string png_data_as_string;
  GetFakeIcon(mojom::ScaleFactor::SCALE_FACTOR_100P, &png_data_as_string);
  std::vector<uint8_t> fake_icon_png_data(png_data_as_string.begin(),
                                          png_data_as_string.end());

  for (int i = 0; i < max_results; ++i) {
    fake_app_shortcut_items.emplace_back(mojom::AppShortcutItem::New(
        base::StringPrintf("ShortcutId %d", i),
        base::StringPrintf("ShortLabel %d", i), fake_icon_png_data,
        "FakeAppPackageName", mojom::AppShortcutItemType::kStatic, i));
  }

  std::move(callback).Run(std::move(fake_app_shortcut_items));
}

void FakeAppInstance::GetAppShortcutItems(
    const std::string& package_name,
    GetAppShortcutItemsCallback callback) {
  // Fake app shortcut items results.
  std::vector<mojom::AppShortcutItemPtr> fake_app_shortcut_items;

  // Fake icon data.
  std::string png_data_as_string;
  GetFakeIcon(mojom::ScaleFactor::SCALE_FACTOR_100P, &png_data_as_string);
  std::vector<uint8_t> fake_icon_png_data(png_data_as_string.begin(),
                                          png_data_as_string.end());

  for (int i = 0; i < 3; ++i) {
    fake_app_shortcut_items.push_back(mojom::AppShortcutItem::New(
        base::StringPrintf("ShortcutId %d", i),
        base::StringPrintf("ShortLabel %d", i), fake_icon_png_data,
        package_name, mojom::AppShortcutItemType::kStatic, i));
  }

  std::move(callback).Run(std::move(fake_app_shortcut_items));
}

void FakeAppInstance::StartPaiFlowDeprecated() {
  NOTREACHED();
}

void FakeAppInstance::StartPaiFlow(StartPaiFlowCallback callback) {
  ++start_pai_request_count_;
  std::move(callback).Run(pai_state_response_);
}

void FakeAppInstance::GetAppReinstallCandidates(
    GetAppReinstallCandidatesCallback callback) {
  ++get_app_reinstall_callback_count_;
  std::vector<arc::mojom::AppReinstallCandidatePtr> candidates;
  for (const auto& candidate : app_reinstall_candidates_)
    candidates.emplace_back(candidate.Clone());

  std::move(callback).Run(arc::mojom::AppReinstallState::REQUEST_SUCCESS,
                          std::move(candidates));
}

void FakeAppInstance::StartFastAppReinstallFlow(
    const std::vector<std::string>& package_names) {
  ++start_fast_app_reinstall_request_count_;
}

void FakeAppInstance::RequestAssistStructure(
    RequestAssistStructureCallback callback) {
  std::move(callback).Run(nullptr, nullptr);
}

void FakeAppInstance::IsInstallable(const std::string& package_name,
                                    IsInstallableCallback callback) {
  std::move(callback).Run(is_installable_);
}

void FakeAppInstance::LaunchIntentDeprecated(
    const std::string& intent_uri,
    const base::Optional<gfx::Rect>& dimension_on_screen) {
  LaunchIntent(intent_uri, 0);
}

void FakeAppInstance::LaunchIntent(const std::string& intent_uri,
                                   int64_t display_id) {
  launch_intents_.push_back(intent_uri);
}

void FakeAppInstance::RequestShortcutIcon(
    const std::string& icon_resource_id,
    int dimension,
    RequestShortcutIconCallback callback) {
  shortcut_icon_requests_.push_back(
      std::make_unique<ShortcutIconRequest>(icon_resource_id, dimension));

  std::string png_data_as_string;
  if (GenerateIconResponse(dimension, false /* app_icon */,
                           &png_data_as_string)) {
    std::move(callback).Run(std::vector<uint8_t>(png_data_as_string.begin(),
                                                 png_data_as_string.end()));
  }
}

void FakeAppInstance::RequestPackageIcon(const std::string& package_name,
                                         int dimension,
                                         bool normalize,
                                         RequestPackageIconCallback callback) {
  base::ScopedAllowBlockingForTesting allow_io;
  std::string png_data_as_string;
  GetFakeIcon(mojom::ScaleFactor::SCALE_FACTOR_100P, &png_data_as_string);
  std::move(callback).Run(std::vector<uint8_t>(png_data_as_string.begin(),
                                               png_data_as_string.end()));
}

void FakeAppInstance::RemoveCachedIcon(const std::string& icon_resource_id) {}

void FakeAppInstance::SetAppReinstallCandidates(
    const std::vector<arc::mojom::AppReinstallCandidatePtr>& candidates) {
  app_reinstall_candidates_.clear();
  for (const auto& candidate : candidates)
    app_reinstall_candidates_.emplace_back(candidate.Clone());
}

}  // namespace arc
