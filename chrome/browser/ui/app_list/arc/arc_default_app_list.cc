// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/arc/arc_default_app_list.h"

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/path_service.h"
#include "base/task/post_task.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/arc/arc_app_scoped_pref_update.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/common/chrome_paths.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_registry.h"

namespace {

constexpr char kActivity[] = "activity";
constexpr char kAppPath[] = "app_path";
constexpr char kName[] = "name";
constexpr char kOem[] = "oem";
constexpr char kPackageName[] = "package_name";
constexpr char kSystem[] = "system";

constexpr char kDefaultApps[] = "arc.apps.default";
constexpr char kHidden[] = "hidden";

// Sub-directory wher ARC apps forward declarations are stored.
const base::FilePath::CharType kArcDirectory[] = FILE_PATH_LITERAL("arc");
const base::FilePath::CharType kArcTestDirectory[] =
    FILE_PATH_LITERAL("arc_default_apps");
const base::FilePath::CharType kArcTestBoardDirectory[] =
    FILE_PATH_LITERAL("arc_board_default_apps");
const base::FilePath::CharType kBoardDirectory[] =
    FILE_PATH_LITERAL("/var/cache/arc_default_apps");

bool use_test_apps_directory = false;

std::unique_ptr<ArcDefaultAppList::AppInfoMap> ReadAppsFromFileThread(
    const base::FilePath& base_path) {
  base::FilePath root_dir;
  // FileEnumerator does not work with a symbolic link dir. So map link
  // to real folder in case |base_path| specifies a symbolic link.
  if (!base::ReadSymbolicLink(base_path, &root_dir))
    root_dir = base_path;

  std::unique_ptr<ArcDefaultAppList::AppInfoMap> apps =
      std::make_unique<ArcDefaultAppList::AppInfoMap>();

  base::FilePath::StringType extension(".json");
  base::FileEnumerator json_files(root_dir,
                                  false,  // Recursive.
                                  base::FileEnumerator::FILES);

  for (base::FilePath file = json_files.Next(); !file.empty();
      file = json_files.Next()) {
    if (!file.MatchesExtension(extension)) {
      DVLOG(1) << "Not considering: " << file.LossyDisplayName()
               << " (does not have a .json extension)";
      continue;
    }

    JSONFileValueDeserializer deserializer(file);
    std::string error_msg;
    std::unique_ptr<base::Value> app_info =
        deserializer.Deserialize(nullptr, &error_msg);
    if (!app_info) {
      VLOG(2) << "Unable to deserialize json data: " << error_msg << " in file "
              << file.value() << ".";
      continue;
    }

    std::unique_ptr<base::DictionaryValue> app_info_dictionary =
        base::DictionaryValue::From(std::move(app_info));
    CHECK(app_info_dictionary);

    std::string name;
    std::string package_name;
    std::string activity;
    std::string app_path;
    bool oem = false;
    bool system = false;

    app_info_dictionary->GetString(kName, &name);
    app_info_dictionary->GetString(kPackageName, &package_name);
    app_info_dictionary->GetString(kActivity, &activity);
    app_info_dictionary->GetString(kAppPath, &app_path);
    app_info_dictionary->GetBoolean(kOem, &oem);
    app_info_dictionary->GetBoolean(kSystem, &system);

    if (name.empty() || package_name.empty() || activity.empty() ||
        app_path.empty()) {
      VLOG(2) << "ARC app declaration is incomplete in file " << file.value()
              << ".";
      continue;
    }

    const std::string app_id =
        ArcAppListPrefs::GetAppId(package_name, activity);
    std::unique_ptr<ArcDefaultAppList::AppInfo> app =
        std::make_unique<ArcDefaultAppList::AppInfo>(name, package_name,
                                                     activity, oem, system,
                                                     root_dir.Append(app_path));
    apps.get()->insert(
        std::pair<std::string, std::unique_ptr<ArcDefaultAppList::AppInfo>>(
            app_id, std::move(app)));
  }

  return apps;
}

// Returns true if default app |app_id| is marked as hidden in the prefs.
bool IsAppHidden(const PrefService* prefs, const std::string& app_id) {
  const base::DictionaryValue* apps_dict = prefs->GetDictionary(kDefaultApps);
  const base::DictionaryValue* app_dict;
  if (!apps_dict || !apps_dict->GetDictionary(app_id, &app_dict))
    return false;
  bool hidden = false;
  return app_dict->GetBoolean(kHidden, &hidden) && hidden;
}

}  // namespace

// static
void ArcDefaultAppList::UseTestAppsDirectory() {
  use_test_apps_directory = true;
}

// static
void ArcDefaultAppList::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(kDefaultApps);
}

ArcDefaultAppList::ArcDefaultAppList(Profile* profile,
                                     base::OnceClosure ready_callback)
    : profile_(profile), ready_callback_(std::move(ready_callback)) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  // Load default apps from two sources.
  // /usr/share/google-chrome/extensions/arc - contains default apps for all
  //     boards that share the same image.
  // /var/cache/arc_default_apps that is link to
  //     /usr/share/google-chrome/extensions/arc/BOARD_NAME - contains default
  //     apps for particular current board.
  //
  std::vector<base::FilePath> sources;

  base::FilePath base_path;
  if (!use_test_apps_directory) {
    const bool valid_path = base::PathService::Get(
        chrome::DIR_STANDALONE_EXTERNAL_EXTENSIONS, &base_path);
    DCHECK(valid_path);
    sources.push_back(base_path.Append(kArcDirectory));
    sources.push_back(base::FilePath(kBoardDirectory));
  } else {
    const bool valid_path =
        base::PathService::Get(chrome::DIR_TEST_DATA, &base_path);
    DCHECK(valid_path);
    sources.push_back(base_path.Append(kArcTestDirectory));
    sources.push_back(base_path.Append(kArcTestBoardDirectory));
  }

  // Using base::Unretained(this) here is safe since we own barrier_closure_.
  barrier_closure_ = base::BarrierClosure(
      sources.size(),
      base::BindOnce(&ArcDefaultAppList::OnAppsReady, base::Unretained(this)));

  // Once ready OnAppsReady is called.
  for (const auto& source : sources) {
    base::PostTaskAndReplyWithResult(
        FROM_HERE,
        {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::BindOnce(&ReadAppsFromFileThread, source),
        base::BindOnce(&ArcDefaultAppList::OnAppsRead,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

ArcDefaultAppList::~ArcDefaultAppList() = default;

void ArcDefaultAppList::OnAppsRead(std::unique_ptr<AppInfoMap> apps) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  const PrefService* const prefs = profile_->GetPrefs();
  for (auto& entry : *apps.get()) {
    AppInfoMap& app_map =
        IsAppHidden(prefs, entry.first) ? hidden_apps_ : visible_apps_;
    app_map[entry.first] = std::move(entry.second);
  }

  barrier_closure_.Run();
}

void ArcDefaultAppList::OnAppsReady() {
  const PrefService* const prefs = profile_->GetPrefs();
  // Register Play Store as default app. Some services and ArcSupportHost may
  // not be available in tests.
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile_);
  const extensions::Extension* arc_host =
      registry->GetInstalledExtension(arc::kPlayStoreAppId);
  if (arc_host && arc::IsPlayStoreAvailable()) {
    std::unique_ptr<ArcDefaultAppList::AppInfo> play_store_app =
        std::make_unique<ArcDefaultAppList::AppInfo>(
            arc_host->name(), arc::kPlayStorePackage, arc::kPlayStoreActivity,
            false /* oem */, true /* system */,
            base::FilePath() /* app_path */);
    AppInfoMap& app_map =
        IsAppHidden(prefs, arc::kPlayStoreAppId) ? hidden_apps_ : visible_apps_;
    app_map.insert(
        std::pair<std::string, std::unique_ptr<ArcDefaultAppList::AppInfo>>(
            arc::kPlayStoreAppId, std::move(play_store_app)));
  }

  std::move(ready_callback_).Run();
}

const ArcDefaultAppList::AppInfo* ArcDefaultAppList::GetApp(
    const std::string& app_id) const {
  if (filter_level_ == FilterLevel::ALL)
    return nullptr;

  const auto it = visible_apps_.find(app_id);
  if (it == visible_apps_.end())
    return nullptr;

  if (filter_level_ == FilterLevel::OPTIONAL_APPS && !it->second->system)
    return nullptr;

  return it->second.get();
}

bool ArcDefaultAppList::HasApp(const std::string& app_id) const {
  return GetApp(app_id) != nullptr;
}

bool ArcDefaultAppList::HasPackage(const std::string& package_name) const {
  for (const auto& it : visible_apps_) {
    if (it.second->package_name == package_name)
      return true;
  }
  return false;
}

bool ArcDefaultAppList::HasHiddenPackage(
    const std::string& package_name) const {
  for (const auto& it : hidden_apps_) {
    if (it.second->package_name == package_name)
      return true;
  }
  return false;
}

void ArcDefaultAppList::SetAppHidden(const std::string& app_id, bool hidden) {
  AppInfoMap& active_map = hidden ? visible_apps_ : hidden_apps_;
  AppInfoMap& inactive_map = hidden ? hidden_apps_ : visible_apps_;

  auto it = active_map.find(app_id);
  if (it == active_map.end())
    return;
  inactive_map[app_id] = std::move(it->second);
  active_map.erase(it);

  // Store hidden flag.
  arc::ArcAppScopedPrefUpdate(profile_->GetPrefs(), app_id, kDefaultApps)
      .Get()
      ->SetBoolean(kHidden, hidden);
}

void ArcDefaultAppList::SetAppsHiddenForPackage(
    const std::string& package_name) {
  std::unordered_set<std::string> apps_to_hide;
  for (const auto& app : visible_apps_) {
    if (app.second->package_name == package_name)
      apps_to_hide.insert(app.first);
  }
  for (const auto& app : apps_to_hide)
    SetAppHidden(app, true);
}

std::map<std::string, const ArcDefaultAppList::AppInfo*>
ArcDefaultAppList::GetActiveApps() const {
  std::map<std::string, const AppInfo*> result;
  for (const auto& it : visible_apps_) {
    if (HasApp(it.first))
      result[it.first] = it.second.get();
  }
  return result;
}

std::unordered_set<std::string> ArcDefaultAppList::GetActivePackages() const {
  std::unordered_set<std::string> result;
  for (const auto& it : GetActiveApps())
    result.insert(it.second->package_name);
  return result;
}

ArcDefaultAppList::AppInfo::AppInfo(const std::string& name,
                                    const std::string& package_name,
                                    const std::string& activity,
                                    bool oem,
                                    bool system,
                                    const base::FilePath app_path)
    : name(name),
      package_name(package_name),
      activity(activity),
      oem(oem),
      system(system),
      app_path(app_path) {}

ArcDefaultAppList::AppInfo::~AppInfo() {}
