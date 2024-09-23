// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/web_app_info_recorder_utils.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/files/important_file_writer.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_string_value_serializer.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"

namespace chrome {
namespace startup {

namespace {

// This class is used to write open and installed web apps to the specified
// file.
class GetWebApps {
 public:
  GetWebApps(const GetWebApps&) = delete;
  GetWebApps& operator=(const GetWebApps&) = delete;

  ~GetWebApps() = default;

  static void Start(const base::FilePath& output_file,
                    const base::FilePath& profile_base_name);

 private:
  GetWebApps(const base::FilePath& output_file,
             const base::FilePath& profile_base_name)
      : output_file_(output_file),
        profile_base_name_(profile_base_name),
        keep_alive_(std::make_unique<ScopedKeepAlive>(
            KeepAliveOrigin::APP_GET_INFO,
            KeepAliveRestartOption::DISABLED)) {}

  // Serializes `output_info` to string and posts a task to the thread pool to
  // perform the write.
  void SerializeAndScheduleWrite(const base::Value& output_info);

  // Get installed web apps for all profiles if |profile_base_name_| is empty.
  // Otherwise, get installed web apps only for the given profile.
  base::Value GetInstalledWebApps();

  // Get open web apps for all profiles if |profile_base_name_| is empty.
  // Otherwise, get open web apps only for the given profile.
  base::Value GetOpenWebApps();

  void OnProfileLoaded(base::RepeatingClosure call_back, Profile* profile);

  void FetchWebAppsAndWriteToDisk();

  const base::FilePath output_file_;
  const base::FilePath profile_base_name_;
  std::unique_ptr<ScopedKeepAlive> keep_alive_;
  std::vector<raw_ptr<Profile, VectorExperimental>> profiles_;
  std::vector<std::unique_ptr<ScopedProfileKeepAlive>> profiles_keep_alive_;
};

// static
void GetWebApps::Start(const base::FilePath& output_file,
                       const base::FilePath& profile_base_name) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  std::vector<ProfileAttributesEntry*> profile_attributes_entry =
      profile_manager->GetProfileAttributesStorage().GetAllProfilesAttributes();
  size_t profile_count =
      profile_base_name.empty() ? profile_attributes_entry.size() : 1;
  // The instance is owned by the callback that invokes
  // FetchWebAppsAndWriteToDisk once all profiles have been loaded.
  std::unique_ptr<GetWebApps> get_web_apps(
      new GetWebApps(output_file, profile_base_name));
  GetWebApps* get_web_apps_ptr = get_web_apps.get();
  base::RepeatingClosure get_webapps_on_providers_ready = base::BarrierClosure(
      profile_count, base::BindOnce(&GetWebApps::FetchWebAppsAndWriteToDisk,
                                    std::move(get_web_apps)));
  if (profile_base_name.empty()) {
    for (auto* item : profile_attributes_entry) {
      profile_manager->LoadProfileByPath(
          item->GetPath(), /*incognito=*/false,
          base::BindOnce(&GetWebApps::OnProfileLoaded,
                         base::Unretained(get_web_apps_ptr),
                         get_webapps_on_providers_ready));
    }
  } else {
    profile_manager->LoadProfile(
        profile_base_name, /*incognito=*/false,
        base::BindOnce(&GetWebApps::OnProfileLoaded,
                       base::Unretained(get_web_apps_ptr),
                       get_webapps_on_providers_ready));
  }
}

void GetWebApps::SerializeAndScheduleWrite(const base::Value& output_info) {
  std::string output_info_str;
  JSONStringValueSerializer serializer(&output_info_str);
  serializer.set_pretty_print(true);
  if (serializer.Serialize(output_info)) {
    base::ThreadPool::PostTask(
        FROM_HERE,
        {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
         base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
        base::BindOnce(
            base::IgnoreResult(&base::ImportantFileWriter::WriteFileAtomically),
            output_file_, std::move(output_info_str), std::string_view()));
  }
}

base::Value GetWebApps::GetInstalledWebApps() {
  base::Value::List installed_apps_list;
  for (Profile* item : profiles_) {
    web_app::WebAppProvider* web_app_provider =
        web_app::WebAppProvider::GetForWebApps(item);
    base::Value::Dict item_info;
    item_info.Set("profile_id", item->GetBaseName().AsUTF8Unsafe());
    base::Value::List installed_apps_per_profile;
    for (const web_app::WebApp& web_app :
         web_app_provider->registrar_unsafe().GetApps()) {
      base::Value::Dict web_app_info;
      web_app_info.Set("name", web_app.untranslated_name());
      web_app_info.Set("id", web_app.app_id());
      installed_apps_per_profile.Append(std::move(web_app_info));
    }
    item_info.Set("web_apps", std::move(installed_apps_per_profile));
    installed_apps_list.Append(std::move(item_info));
  }
  return base::Value(std::move(installed_apps_list));
}

base::Value GetWebApps::GetOpenWebApps() {
  base::flat_map<std::string, base::Value::List> open_apps;
  for (Browser* browser : *BrowserList::GetInstance()) {
    if (browser->type() != Browser::Type::TYPE_APP)
      continue;
    std::string app_profile_base_name =
        browser->profile()->GetBaseName().AsUTF8Unsafe();
    if (!profile_base_name_.empty() &&
        profile_base_name_.AsUTF8Unsafe() != app_profile_base_name) {
      continue;
    }
    base::Value::Dict web_app_info;
    web_app_info.Set("id", browser->app_controller()->app_id());
    web_app_info.Set("name", base::UTF16ToUTF8(
                                 browser->app_controller()->GetAppShortName()));
    auto iter_and_inserted =
        open_apps.emplace(app_profile_base_name, base::Value::List());
    iter_and_inserted.first->second.Append(std::move(web_app_info));
  }
  base::Value::List open_apps_list;
  for (auto& item : open_apps) {
    base::Value::Dict item_info;
    item_info.Set("profile_id", item.first);
    item_info.Set("web_apps", std::move(item.second));
    open_apps_list.Append(std::move(item_info));
  }
  return base::Value(std::move(open_apps_list));
}

void GetWebApps::OnProfileLoaded(base::RepeatingClosure callback,
                                 Profile* profile) {
  if (!profile) {
    callback.Run();
    return;
  }
  profiles_.push_back(profile);
  profiles_keep_alive_.push_back(std::make_unique<ScopedProfileKeepAlive>(
      profile, ProfileKeepAliveOrigin::kGettingWebAppInfo));
  web_app::WebAppProvider* web_app_provider =
      web_app::WebAppProvider::GetForWebApps(profile);
  web_app_provider->on_registry_ready().Post(FROM_HERE, std::move(callback));
}

void GetWebApps::FetchWebAppsAndWriteToDisk() {
  base::Value::Dict apps_dict;
  apps_dict.Set("installed_web_apps", GetInstalledWebApps());
  apps_dict.Set("open_web_apps", GetOpenWebApps());
  SerializeAndScheduleWrite(base::Value(std::move(apps_dict)));
  // `this` is owned by the callback that calls this function, so it will be
  // destroyed automatically after being run.
}

}  // namespace

void WriteWebAppsToFile(const base::FilePath& output_file,
                        const base::FilePath& profile_base_name) {
  GetWebApps::Start(output_file, profile_base_name);
}

}  // namespace startup
}  // namespace chrome
