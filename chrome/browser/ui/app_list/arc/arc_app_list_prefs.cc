// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"

#include <stddef.h>

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/containers/flat_set.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequenced_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/arc/policy/arc_policy_util.h"
#include "chrome/browser/chromeos/arc/session/arc_session_manager.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_session.h"
#include "chrome/browser/chromeos/login/session/user_session_manager.h"
#include "chrome/browser/image_decoder/image_decoder.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs_factory.h"
#include "chrome/browser/ui/app_list/arc/arc_app_scoped_pref_update.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/app_list/arc/arc_default_app_list.h"
#include "chrome/browser/ui/app_list/arc/arc_package_syncable_service.h"
#include "chrome/browser/ui/app_list/arc/arc_pai_starter.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/arc/arc_prefs.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/arc_util.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/arc/session/connection_holder.h"
#include "components/crx_file/id_util.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_thread.h"
#include "skia/ext/image_operations.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/layout.h"
#include "ui/gfx/codec/png_codec.h"

namespace {

constexpr char kActivity[] = "activity";
constexpr char kFrameworkPackageName[] = "android";
constexpr char kIconResourceId[] = "icon_resource_id";
constexpr char kIconVersion[] = "icon_version";
constexpr char kInstallTime[] = "install_time";
constexpr char kIntentUri[] = "intent_uri";
constexpr char kLastBackupAndroidId[] = "last_backup_android_id";
constexpr char kLastBackupTime[] = "last_backup_time";
constexpr char kLastLaunchTime[] = "lastlaunchtime";
constexpr char kLaunchable[] = "launchable";
constexpr char kName[] = "name";
constexpr char kNotificationsEnabled[] = "notifications_enabled";
constexpr char kPackageName[] = "package_name";
constexpr char kPackageVersion[] = "package_version";
constexpr char kPinIndex[] = "pin_index";
constexpr char kPermissionStates[] = "permission_states";
constexpr char kSticky[] = "sticky";
constexpr char kShortcut[] = "shortcut";
constexpr char kShouldSync[] = "should_sync";
constexpr char kSuspended[] = "suspended";
constexpr char kSystem[] = "system";
constexpr char kUninstalled[] = "uninstalled";
constexpr char kVPNProvider[] = "vpnprovider";
constexpr char kPermissionStateGranted[] = "granted";
constexpr char kPermissionStateManaged[] = "managed";

// Defines current version for app icons. This is used for invalidation icons in
// case we change how app icons are produced on Android side. Can be updated in
// unit tests.
int current_icons_version = 1;

// Set of default app icon dips that are required to support ARC icons in all
// usage cases.
constexpr int default_app_icon_dip_sizes[] = {16, 32, 48, 64};

constexpr base::TimeDelta kDetectDefaultAppAvailabilityTimeout =
    base::TimeDelta::FromMinutes(1);

// Accessor for deferred set notifications enabled requests in prefs.
class NotificationsEnabledDeferred {
 public:
  explicit NotificationsEnabledDeferred(PrefService* prefs) : prefs_(prefs) {}

  void Put(const std::string& app_id, bool enabled) {
    DictionaryPrefUpdate update(
        prefs_, arc::prefs::kArcSetNotificationsEnabledDeferred);
    base::DictionaryValue* const dict = update.Get();
    dict->SetKey(app_id, base::Value(enabled));
  }

  bool Get(const std::string& app_id) {
    const base::DictionaryValue* dict =
        prefs_->GetDictionary(arc::prefs::kArcSetNotificationsEnabledDeferred);
    return dict->FindBoolKey(app_id).value_or(false);
  }

  void Remove(const std::string& app_id) {
    DictionaryPrefUpdate update(
        prefs_, arc::prefs::kArcSetNotificationsEnabledDeferred);
    base::DictionaryValue* const dict = update.Get();
    dict->RemoveKey(app_id);
  }

 private:
  PrefService* const prefs_;
};

bool WriteIconFile(const base::FilePath& icon_path,
                   const std::vector<uint8_t>& icon_png_data) {
  if (icon_png_data.empty())
    return false;

  base::CreateDirectory(icon_path.DirName());

  int wrote = base::WriteFile(icon_path,
                              reinterpret_cast<const char*>(&icon_png_data[0]),
                              icon_png_data.size());
  if (wrote != static_cast<int>(icon_png_data.size())) {
    VLOG(2) << "Failed to write ARC icon file: " << icon_path.MaybeAsASCII()
            << ".";
    if (!base::DeleteFile(icon_path)) {
      VLOG(2) << "Couldn't delete broken icon file" << icon_path.MaybeAsASCII()
              << ".";
    }
    return false;
  }
  return true;
}

bool InstallIconFromFileThread(const base::FilePath& icon_path,
                               const base::FilePath& foreground_icon_path,
                               const base::FilePath& background_icon_path,
                               arc::mojom::RawIconPngDataPtr icon) {
  const std::vector<uint8_t>& icon_png_data = icon->icon_png_data.value();
  DCHECK(!icon_png_data.empty());

  if (!WriteIconFile(icon_path, icon_png_data))
    return false;

  if (!base::FeatureList::IsEnabled(features::kAppServiceAdaptiveIcon))
    return true;

  if (!icon->is_adaptive_icon) {
    // For non-adaptive icon, save the |icon_png_data| to the
    // |foreground_icon_path|, to identify the difference between migrating to
    // the adaptive icon feature enabled and the non-adaptive icon case. If
    // there is a |foreground_icon_path| file without a |background_icon_path|
    // file, that means the icon is a non-adaptive icon. Otherwise, if there is
    // no |foreground_icon_path| file, that means we haven't fetched the
    // adaptive icon yet, then we should request the icon.
    if (!WriteIconFile(foreground_icon_path, icon->icon_png_data.value())) {
      return false;
    }

    return true;
  }

  if (!WriteIconFile(foreground_icon_path,
                     icon->foreground_icon_png_data.value())) {
    return false;
  }

  if (!WriteIconFile(background_icon_path,
                     icon->background_icon_png_data.value())) {
    return false;
  }

  return true;
}

void DeleteAppFolderFromFileThread(const base::FilePath& path) {
  DCHECK(path.DirName().BaseName().MaybeAsASCII() == arc::prefs::kArcApps &&
         (!base::PathExists(path) || base::DirectoryExists(path)));
  const bool deleted = base::DeletePathRecursively(path);
  DCHECK(deleted);
}

// TODO(crbug.com/672829): Due to shutdown procedure dependency,
// ArcAppListPrefs may try to touch ArcSessionManager related stuff.
// Specifically, this returns false on shutdown phase.
// Remove this check after the shutdown behavior is fixed.
bool IsArcAlive() {
  const auto* arc_session_manager = arc::ArcSessionManager::Get();
  return arc_session_manager && arc_session_manager->IsAllowed();
}

// Returns true if ARC Android instance is supposed to be enabled for the
// profile.  This can happen for if the user has opted in for the given profile,
// or when ARC always starts after login.
bool IsArcAndroidEnabledForProfile(const Profile* profile) {
  return arc::ShouldArcAlwaysStart() ||
         arc::IsArcPlayStoreEnabledForProfile(profile);
}

bool GetInt64FromPref(const base::DictionaryValue* dict,
                      const std::string& key,
                      int64_t* value) {
  DCHECK(dict);
  std::string value_str;
  if (!dict->GetStringWithoutPathExpansion(key, &value_str)) {
    VLOG(2) << "Can't find key in local pref dictionary. Invalid key: " << key
            << ".";
    return false;
  }

  if (!base::StringToInt64(value_str, value)) {
    VLOG(2) << "Can't change string to int64_t. Invalid string value: "
            << value_str << ".";
    return false;
  }

  return true;
}

// Returns true if one of state of |info1| does not match the same state in
// |info2|.
bool AreAppStatesChanged(const ArcAppListPrefs::AppInfo& info1,
                         const ArcAppListPrefs::AppInfo& info2) {
  return info1.sticky != info2.sticky ||
         info1.notifications_enabled != info2.notifications_enabled ||
         info1.ready != info2.ready || info1.suspended != info2.suspended ||
         info1.show_in_launcher != info2.show_in_launcher ||
         info1.launchable != info2.launchable;
}

// We have only fixed icon dimensions for default apps, 32, 48 and 64. If
// requested dimension does not exist, use bigger one that can be downsized.
// In case requested dimension is bigger than 64, use largest possible size that
// can be upsized.
ArcAppIconDescriptor MapDefaultAppIconDescriptor(
    const ArcAppIconDescriptor& descriptor) {
  int default_app_dip_size;
  if (descriptor.dip_size <= 32)
    default_app_dip_size = 32;
  else if (descriptor.dip_size <= 48)
    default_app_dip_size = 48;
  else
    default_app_dip_size = 64;
  return ArcAppIconDescriptor(default_app_dip_size, descriptor.scale_factor);
}

// Whether skip install_time for comparing two |AppInfo|.
bool ignore_compare_app_info_install_time = false;

// Reason for installation enumeration; Used for UMA counter for reason for
// install.
enum class InstallationCounterReasonEnum {
  USER = 0,     // Application installed by user.
  DEFAULT = 1,  // Application part of the default set.
  OEM = 2,      // OEM application.
  POLICY = 3,   // Installed by policy.
  UNKNOWN = 4,
  kMaxValue = UNKNOWN,
};

// Reasons for uninstalls. Only one, USER, for now.
enum class UninstallCounterReasonEnum {
  USER = 0,  // Uninstall triggered by user.
  kMaxValue = USER
};

}  // namespace

// static
ArcAppListPrefs* ArcAppListPrefs::Create(Profile* profile) {
  return new ArcAppListPrefs(profile, nullptr);
}

// static
ArcAppListPrefs* ArcAppListPrefs::Create(
    Profile* profile,
    arc::ConnectionHolder<arc::mojom::AppInstance, arc::mojom::AppHost>*
        app_connection_holder_for_testing) {
  DCHECK(app_connection_holder_for_testing);
  return new ArcAppListPrefs(profile, app_connection_holder_for_testing);
}

// static
void ArcAppListPrefs::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(arc::prefs::kArcApps);
  registry->RegisterDictionaryPref(arc::prefs::kArcPackages);
  registry->RegisterIntegerPref(arc::prefs::kArcFrameworkVersion,
                                -1 /* default_value */);
  registry->RegisterDictionaryPref(
      arc::prefs::kArcSetNotificationsEnabledDeferred);
  ArcDefaultAppList::RegisterProfilePrefs(registry);
}

// static
ArcAppListPrefs* ArcAppListPrefs::Get(content::BrowserContext* context) {
  return ArcAppListPrefsFactory::GetInstance()->GetForBrowserContext(context);
}

// static
std::string ArcAppListPrefs::GetAppId(const std::string& package_name,
                                      const std::string& activity) {
  if (package_name == arc::kPlayStorePackage &&
      activity == arc::kPlayStoreActivity) {
    return arc::kPlayStoreAppId;
  }
  const std::string input = package_name + "#" + activity;
  const std::string app_id = crx_file::id_util::GenerateId(input);
  return app_id;
}

// static
void ArcAppListPrefs::UprevCurrentIconsVersionForTesting() {
  ++current_icons_version;
}

std::string ArcAppListPrefs::GetAppIdByPackageName(
    const std::string& package_name) const {
  const base::DictionaryValue* apps =
      prefs_->GetDictionary(arc::prefs::kArcApps);
  if (!apps)
    return std::string();

  for (const auto& it : apps->DictItems()) {
    const base::Value& value = it.second;
    const base::Value* installed_package_name =
        value.FindKeyOfType(kPackageName, base::Value::Type::STRING);
    if (!installed_package_name ||
        installed_package_name->GetString() != package_name)
      continue;

    const base::Value* activity_name =
        value.FindKeyOfType(kActivity, base::Value::Type::STRING);
    return activity_name ? GetAppId(package_name, activity_name->GetString())
                         : std::string();
  }
  return std::string();
}

ArcAppListPrefs::ArcAppListPrefs(
    Profile* profile,
    arc::ConnectionHolder<arc::mojom::AppInstance, arc::mojom::AppHost>*
        app_connection_holder_for_testing)
    : profile_(profile),
      prefs_(profile->GetPrefs()),
      app_connection_holder_for_testing_(app_connection_holder_for_testing),
      file_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})) {
  VLOG(1) << "ARC app list prefs created";
  DCHECK(profile);
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  const base::FilePath& base_path = profile->GetPath();
  base_path_ = base_path.AppendASCII(arc::prefs::kArcApps);

  // Once default apps are ready OnDefaultAppsReady is called.
  default_apps_ = std::make_unique<ArcDefaultAppList>(
      profile, base::BindOnce(&ArcAppListPrefs::OnDefaultAppsReady,
                              base::Unretained(this)));

  arc::ArcSessionManager* arc_session_manager = arc::ArcSessionManager::Get();
  if (!arc_session_manager) {
    VLOG(1) << "ARC session manager is not available";
    return;
  }

  DCHECK(arc::IsArcAllowedForProfile(profile));

  const std::vector<std::string> existing_app_ids = GetAppIds();
  tracked_apps_.insert(existing_app_ids.begin(), existing_app_ids.end());

  // Not always set in unit_tests
  arc::ArcPolicyBridge* policy_bridge =
      arc::ArcPolicyBridge::GetForBrowserContext(profile_);
  if (policy_bridge)
    policy_bridge->AddObserver(this);
}

ArcAppListPrefs::~ArcAppListPrefs() {
  for (auto& observer : observer_list_)
    observer.OnArcAppListPrefsDestroyed();

  arc::ArcSessionManager* arc_session_manager = arc::ArcSessionManager::Get();
  if (!arc_session_manager)
    return;
  DCHECK(arc::ArcServiceManager::Get());
  arc_session_manager->RemoveObserver(this);
  app_connection_holder()->RemoveObserver(this);
}

void ArcAppListPrefs::StartPrefs() {
  // Don't tie ArcAppListPrefs created with sync test profile in sync
  // integration test to ArcSessionManager.
  if (!ArcAppListPrefsFactory::IsFactorySetForSyncTest()) {
    arc::ArcSessionManager* arc_session_manager = arc::ArcSessionManager::Get();
    CHECK(arc_session_manager);

    if (arc_session_manager->profile()) {
      // Note: If ArcSessionManager has profile, it should be as same as the one
      // this instance has, because ArcAppListPrefsFactory creates an instance
      // only if the given Profile meets ARC's requirement.
      // Anyway, just in case, check it here and log. Only some browser tests
      // will log the error. If you see the log outside browser_tests, something
      // unexpected may have happened.
      if (profile_ != arc_session_manager->profile()) {
        LOG(ERROR)
            << "This object's profile_ and ArcSessionManager's don't match.";
      }
      OnArcPlayStoreEnabledChanged(
          arc::IsArcPlayStoreEnabledForProfile(profile_));
    }
    arc_session_manager->AddObserver(this);
  }

  VLOG(1) << "Registering host...";

  app_connection_holder()->SetHost(this);
  app_connection_holder()->AddObserver(this);
  if (!app_connection_holder()->IsConnected())
    OnConnectionClosed();
}

base::FilePath ArcAppListPrefs::GetAppPath(const std::string& app_id) const {
  return base_path_.AppendASCII(app_id);
}

base::FilePath ArcAppListPrefs::MaybeGetIconPathForDefaultApp(
    const std::string& app_id,
    const ArcAppIconDescriptor& descriptor) const {
  const ArcDefaultAppList::AppInfo* default_app = default_apps_->GetApp(app_id);
  if (!default_app || default_app->app_path.empty())
    return base::FilePath();

  return default_app->app_path.AppendASCII(
      MapDefaultAppIconDescriptor(descriptor).GetName());
}

base::FilePath ArcAppListPrefs::MaybeGetForegroundIconPathForDefaultApp(
    const std::string& app_id,
    const ArcAppIconDescriptor& descriptor) const {
  const ArcDefaultAppList::AppInfo* default_app = default_apps_->GetApp(app_id);
  if (!default_app || default_app->app_path.empty())
    return base::FilePath();

  return default_app->app_path.AppendASCII(
      MapDefaultAppIconDescriptor(descriptor).GetForegroundIconName());
}

base::FilePath ArcAppListPrefs::MaybeGetBackgroundIconPathForDefaultApp(
    const std::string& app_id,
    const ArcAppIconDescriptor& descriptor) const {
  const ArcDefaultAppList::AppInfo* default_app = default_apps_->GetApp(app_id);
  if (!default_app || default_app->app_path.empty())
    return base::FilePath();

  return default_app->app_path.AppendASCII(
      MapDefaultAppIconDescriptor(descriptor).GetBackgroundIconName());
}

base::FilePath ArcAppListPrefs::GetIconPath(
    const std::string& app_id,
    const ArcAppIconDescriptor& descriptor) {
  // TODO(khmel): Add DCHECK(GetApp(app_id));
  active_icons_[app_id].insert(descriptor);
  return GetAppPath(app_id).AppendASCII(descriptor.GetName());
}

base::FilePath ArcAppListPrefs::GetForegroundIconPath(
    const std::string& app_id,
    const ArcAppIconDescriptor& descriptor) {
  active_icons_[app_id].insert(descriptor);
  return GetAppPath(app_id).AppendASCII(descriptor.GetForegroundIconName());
}

base::FilePath ArcAppListPrefs::GetBackgroundIconPath(
    const std::string& app_id,
    const ArcAppIconDescriptor& descriptor) {
  active_icons_[app_id].insert(descriptor);
  return GetAppPath(app_id).AppendASCII(descriptor.GetBackgroundIconName());
}

bool ArcAppListPrefs::IsIconRequestRecorded(
    const std::string& app_id,
    const ArcAppIconDescriptor& descriptor) const {
  const auto iter = request_icon_recorded_.find(app_id);
  if (iter == request_icon_recorded_.end())
    return false;
  return iter->second.count(descriptor);
}

void ArcAppListPrefs::MaybeRemoveIconRequestRecord(const std::string& app_id) {
  request_icon_recorded_.erase(app_id);
}

void ArcAppListPrefs::ClearIconRequestRecord() {
  request_icon_recorded_.clear();
}

void ArcAppListPrefs::RequestIcon(const std::string& app_id,
                                  const ArcAppIconDescriptor& descriptor) {
  DCHECK_NE(app_id, arc::kPlayStoreAppId);

  // ArcSessionManager can be terminated during test tear down, before callback
  // into this function.
  // TODO(victorhsieh): figure out the best way/place to handle this situation.
  if (arc::ArcSessionManager::Get() == nullptr)
    return;

  if (!IsRegistered(app_id)) {
    VLOG(2) << "Request to load icon for non-registered app: " << app_id << ".";
    return;
  }

  // In case app is not ready, recorded request will be send to ARC when app
  // becomes ready.
  // This record will prevent ArcAppIcon from resending request to ARC for app
  // icon when icon file decode failure is suffered in case app sends bad icon.
  request_icon_recorded_[app_id].insert(descriptor);

  if (!ready_apps_.count(app_id))
    return;

  if (!app_connection_holder()->IsConnected()) {
    // AppInstance should be ready since we have app_id in ready_apps_. This
    // can happen in browser_tests.
    return;
  }

  std::unique_ptr<AppInfo> app_info = GetApp(app_id);
  if (!app_info) {
    VLOG(2) << "Failed to get app info: " << app_id << ".";
    return;
  }

  SendIconRequest(app_id, *app_info, descriptor);
}

void ArcAppListPrefs::SendIconRequest(const std::string& app_id,
                                      const AppInfo& app_info,
                                      const ArcAppIconDescriptor& descriptor) {
  auto callback =
      base::BindOnce(&ArcAppListPrefs::OnIcon, weak_ptr_factory_.GetWeakPtr(),
                     app_id, descriptor);
  if (app_info.icon_resource_id.empty()) {
    auto* app_instance =
        ARC_GET_INSTANCE_FOR_METHOD(app_connection_holder(), GetAppIcon);
    if (!app_instance)
      return;  // Error is logged in macro.

    app_instance->GetAppIcon(app_info.package_name, app_info.activity,
                             descriptor.GetSizeInPixels(), std::move(callback));
  } else {
    auto* app_instance = ARC_GET_INSTANCE_FOR_METHOD(app_connection_holder(),
                                                     GetAppShortcutIcon);
    if (!app_instance)
      return;  // Error is logged in macro.

    app_instance->GetAppShortcutIcon(app_info.icon_resource_id,
                                     descriptor.GetSizeInPixels(),
                                     std::move(callback));
  }
}

void ArcAppListPrefs::MaybeRequestIcon(const std::string& app_id,
                                       const ArcAppIconDescriptor& descriptor) {
  if (!IsIconRequestRecorded(app_id, descriptor))
    RequestIcon(app_id, descriptor);
}

void ArcAppListPrefs::SetNotificationsEnabled(const std::string& app_id,
                                              bool enabled) {
  if (!IsRegistered(app_id)) {
    VLOG(2) << "Request to set notifications enabled flag for non-registered "
            << "app:" << app_id << ".";
    return;
  }

  std::unique_ptr<AppInfo> app_info = GetApp(app_id);
  if (!app_info) {
    VLOG(2) << "Failed to get app info: " << app_id << ".";
    return;
  }

  // In case app is not ready, defer this request.
  if (!ready_apps_.count(app_id)) {
    NotificationsEnabledDeferred(prefs_).Put(app_id, enabled);
    for (auto& observer : observer_list_)
      observer.OnNotificationsEnabledChanged(app_info->package_name, enabled);
    return;
  }

  auto* app_instance = ARC_GET_INSTANCE_FOR_METHOD(app_connection_holder(),
                                                   SetNotificationsEnabled);
  if (!app_instance)
    return;

  NotificationsEnabledDeferred(prefs_).Remove(app_id);
  app_instance->SetNotificationsEnabled(app_info->package_name, enabled);
}

void ArcAppListPrefs::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void ArcAppListPrefs::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

bool ArcAppListPrefs::HasObserver(Observer* observer) {
  return observer_list_.HasObserver(observer);
}

base::RepeatingCallback<std::string(const std::string&)>
ArcAppListPrefs::GetAppIdByPackageNameCallback() {
  return base::BindRepeating(
      [](base::WeakPtr<ArcAppListPrefs> self, const std::string& package_name) {
        if (!self)
          return std::string();
        return self->GetAppIdByPackageName(package_name);
      },
      weak_ptr_factory_.GetWeakPtr());
}

std::unique_ptr<ArcAppListPrefs::PackageInfo> ArcAppListPrefs::GetPackage(
    const std::string& package_name) const {
  if (!IsArcAlive() || !IsArcAndroidEnabledForProfile(profile_))
    return nullptr;

  const base::DictionaryValue* package = nullptr;
  const base::DictionaryValue* packages =
      prefs_->GetDictionary(arc::prefs::kArcPackages);
  if (!packages ||
      !packages->GetDictionaryWithoutPathExpansion(package_name, &package))
    return std::unique_ptr<PackageInfo>();

  if (package->FindBoolKey(kUninstalled).value_or(false))
    return nullptr;

  int64_t last_backup_android_id = 0;
  int64_t last_backup_time = 0;
  base::flat_map<arc::mojom::AppPermission, arc::mojom::PermissionStatePtr>
      permissions;

  GetInt64FromPref(package, kLastBackupAndroidId, &last_backup_android_id);
  GetInt64FromPref(package, kLastBackupTime, &last_backup_time);
  const base::Value* permission_val = package->FindKey(kPermissionStates);
  if (permission_val) {
    const base::DictionaryValue* permission_dict = nullptr;
    permission_val->GetAsDictionary(&permission_dict);
    DCHECK(permission_dict);

    for (base::DictionaryValue::Iterator iter(*permission_dict);
         !iter.IsAtEnd(); iter.Advance()) {
      int64_t permission_type = -1;
      base::StringToInt64(iter.key(), &permission_type);
      DCHECK_NE(-1, permission_type);

      const base::Value& permission_state = iter.value();

      const base::DictionaryValue* permission_state_dict;
      if (permission_state.GetAsDictionary(&permission_state_dict)) {
        bool granted =
            permission_state_dict->FindBoolKey(kPermissionStateGranted)
                .value_or(false);
        bool managed =
            permission_state_dict->FindBoolKey(kPermissionStateManaged)
                .value_or(false);
        arc::mojom::AppPermission permission =
            static_cast<arc::mojom::AppPermission>(permission_type);
        permissions.emplace(permission,
                            arc::mojom::PermissionState::New(granted, managed));
      } else {
        LOG(ERROR) << "Permission state was not a dictionary.";
      }
    }
  }

  return std::make_unique<PackageInfo>(
      package_name, package->FindIntKey(kPackageVersion).value_or(0),
      last_backup_android_id, last_backup_time,
      package->FindBoolKey(kShouldSync).value_or(false),
      package->FindBoolKey(kSystem).value_or(false),
      package->FindBoolKey(kVPNProvider).value_or(false),
      std::move(permissions));
}

std::vector<std::string> ArcAppListPrefs::GetAppIds() const {
  if (arc::ShouldArcAlwaysStart())
    return GetAppIdsNoArcEnabledCheck();

  if (!IsArcAlive() || !IsArcAndroidEnabledForProfile(profile_)) {
    // Default ARC apps available before OptIn.
    std::vector<std::string> ids;
    for (const auto& default_app : default_apps_->GetActiveApps()) {
      // Default apps are iteratively added to prefs. That generates
      // |OnAppRegistered| event per app. Consumer may use this event to request
      // list of all apps. Although this practice is discouraged due the
      // performance reason, let be safe and in order to prevent listing of not
      // yet registered apps, filter out default apps based of tracked state.
      if (tracked_apps_.count(default_app.first))
        ids.push_back(default_app.first);
    }
    return ids;
  }
  return GetAppIdsNoArcEnabledCheck();
}

std::vector<std::string> ArcAppListPrefs::GetAppIdsNoArcEnabledCheck() const {
  std::vector<std::string> ids;
  const base::DictionaryValue* apps =
      prefs_->GetDictionary(arc::prefs::kArcApps);
  DCHECK(apps);

  // crx_file::id_util is de-facto utility for id generation.
  for (base::DictionaryValue::Iterator app_id(*apps); !app_id.IsAtEnd();
       app_id.Advance()) {
    if (!crx_file::id_util::IdIsValid(app_id.key()))
      continue;

    ids.push_back(app_id.key());
  }

  return ids;
}

std::unique_ptr<ArcAppListPrefs::AppInfo> ArcAppListPrefs::GetApp(
    const std::string& app_id) const {
  // Information for default app is available before ARC enabled.
  if ((!IsArcAlive() || !IsArcAndroidEnabledForProfile(profile_)) &&
      !default_apps_->HasApp(app_id)) {
    return std::unique_ptr<AppInfo>();
  }

  return GetAppFromPrefs(app_id);
}

std::unique_ptr<ArcAppListPrefs::AppInfo> ArcAppListPrefs::GetAppFromPrefs(
    const std::string& app_id) const {
  const base::DictionaryValue* app = nullptr;
  const base::DictionaryValue* apps =
      prefs_->GetDictionary(arc::prefs::kArcApps);
  if (!apps || !apps->GetDictionaryWithoutPathExpansion(app_id, &app))
    return std::unique_ptr<AppInfo>();

  std::string name;
  std::string package_name;
  std::string activity;
  std::string intent_uri;
  std::string icon_resource_id;
  bool notifications_enabled =
      app->FindBoolKey(kNotificationsEnabled).value_or(true);
  const bool shortcut = app->FindBoolKey(kShortcut).value_or(false);
  const bool launchable = app->FindBoolKey(kLaunchable).value_or(true);

  app->GetString(kName, &name);
  app->GetString(kPackageName, &package_name);
  app->GetString(kActivity, &activity);
  app->GetString(kIntentUri, &intent_uri);
  app->GetString(kIconResourceId, &icon_resource_id);

  DCHECK(!name.empty());
  DCHECK(!shortcut || activity.empty());
  DCHECK(!shortcut || !intent_uri.empty());

  int64_t last_launch_time_internal = 0;
  base::Time last_launch_time;
  if (GetInt64FromPref(app, kLastLaunchTime, &last_launch_time_internal)) {
    last_launch_time = base::Time::FromInternalValue(last_launch_time_internal);
  }

  const bool deferred = NotificationsEnabledDeferred(prefs_).Get(app_id);
  if (deferred)
    notifications_enabled = deferred;

  return std::make_unique<AppInfo>(
      name, package_name, activity, intent_uri, icon_resource_id,
      last_launch_time, GetInstallTime(app_id),
      app->FindBoolKey(kSticky).value_or(false), notifications_enabled,
      ready_apps_.count(app_id) > 0 /* ready */,
      app->FindBoolKey(kSuspended).value_or(false),
      launchable && arc::ShouldShowInLauncher(app_id), shortcut, launchable);
}

bool ArcAppListPrefs::IsRegistered(const std::string& app_id) const {
  if ((!IsArcAlive() || !IsArcAndroidEnabledForProfile(profile_)) &&
      !default_apps_->HasApp(app_id))
    return false;

  const base::DictionaryValue* app = nullptr;
  const base::DictionaryValue* apps =
      prefs_->GetDictionary(arc::prefs::kArcApps);
  return apps && apps->GetDictionaryWithoutPathExpansion(app_id, &app);
}

bool ArcAppListPrefs::IsDefault(const std::string& app_id) const {
  return default_apps_->HasApp(app_id);
}

bool ArcAppListPrefs::IsOem(const std::string& app_id) const {
  const ArcDefaultAppList::AppInfo* app_info = default_apps_->GetApp(app_id);
  return app_info && app_info->oem;
}

bool ArcAppListPrefs::IsShortcut(const std::string& app_id) const {
  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = GetApp(app_id);
  return app_info && app_info->shortcut;
}

bool ArcAppListPrefs::IsControlledByPolicy(
    const std::string& package_name) const {
  return packages_by_policy_.count(package_name);
}

void ArcAppListPrefs::SetLastLaunchTime(const std::string& app_id) {
  if (!IsRegistered(app_id)) {
    NOTREACHED();
    return;
  }

  // Usage time on hidden should not be tracked.
  if (!arc::ShouldShowInLauncher(app_id))
    return;

  const base::Time time = base::Time::Now();
  arc::ArcAppScopedPrefUpdate update(prefs_, app_id, arc::prefs::kArcApps);
  base::DictionaryValue* app_dict = update.Get();
  const std::string string_value = base::NumberToString(time.ToInternalValue());
  app_dict->SetString(kLastLaunchTime, string_value);

  for (auto& observer : observer_list_)
    observer.OnAppLastLaunchTimeUpdated(app_id);

  if (first_launch_app_request_) {
    first_launch_app_request_ = false;
    // UI Shown time may not be set in unit tests.
    const user_manager::UserManager* user_manager =
        user_manager::UserManager::Get();
    if (arc::ArcSessionManager::Get()->is_directly_started() &&
        !user_manager->IsLoggedInAsKioskApp() &&
        !user_manager->IsLoggedInAsArcKioskApp() &&
        !chromeos::UserSessionManager::GetInstance()
             ->ui_shown_time()
             .is_null()) {
      UMA_HISTOGRAM_CUSTOM_TIMES(
          "Arc.FirstAppLaunchRequest.TimeDelta",
          time - chromeos::UserSessionManager::GetInstance()->ui_shown_time(),
          base::TimeDelta::FromSeconds(1), base::TimeDelta::FromMinutes(2), 20);
    }
  }
}

void ArcAppListPrefs::DisableAllApps() {
  std::unordered_set<std::string> old_ready_apps;
  old_ready_apps.swap(ready_apps_);
  for (auto& app_id : old_ready_apps)
    NotifyAppStatesChanged(app_id);
}

void ArcAppListPrefs::NotifyRegisteredApps() {
  if (apps_restored_)
    return;

  DCHECK(ready_apps_.empty());
  std::vector<std::string> app_ids = GetAppIdsNoArcEnabledCheck();
  for (const auto& app_id : app_ids) {
    std::unique_ptr<AppInfo> app_info = GetApp(app_id);
    if (!app_info) {
      NOTREACHED();
      continue;
    }

    // Default apps are reported earlier.
    if (tracked_apps_.insert(app_id).second) {
      for (auto& observer : observer_list_)
        observer.OnAppRegistered(app_id, *app_info);
    }
  }

  apps_restored_ = true;
}

void ArcAppListPrefs::RemoveAllAppsAndPackages() {
  std::vector<std::string> app_ids = GetAppIdsNoArcEnabledCheck();
  for (const auto& app_id : app_ids) {
    if (!default_apps_->HasApp(app_id)) {
      RemoveApp(app_id);
    } else {
      if (ready_apps_.count(app_id)) {
        ready_apps_.erase(app_id);
        NotifyAppStatesChanged(app_id);
      }
    }
  }
  DCHECK(ready_apps_.empty());

  const std::vector<std::string> package_names_to_remove =
      GetPackagesFromPrefs(false /* check_arc_alive */, true /* installed */);
  for (const auto& package_name : package_names_to_remove) {
    RemovePackageFromPrefs(package_name);
    for (auto& observer : observer_list_)
      observer.OnPackageRemoved(package_name, false);
  }
}

void ArcAppListPrefs::OnArcPlayStoreEnabledChanged(bool enabled) {
  SetDefaultAppsFilterLevel();

  // TODO(victorhsieh): Implement opt-in and opt-out.
  if (arc::ShouldArcAlwaysStart())
    return;

  if (enabled)
    NotifyRegisteredApps();
  else
    RemoveAllAppsAndPackages();
}

void ArcAppListPrefs::SetDefaultAppsFilterLevel() {
  // There is no a blocklisting mechanism for Android apps. Until there is
  // one, we have no option but to ban all pre-installed apps on Android side.
  // Match this requirement and don't show pre-installed apps for managed users
  // in app list.
  if (arc::policy_util::IsAccountManaged(profile_)) {
    if (profile_->IsChild() || chromeos::switches::IsTabletFormFactor()) {
      // For child accounts, filter only optional apps.
      // For tablet form factor devices, filter only optional apps.
      default_apps_->set_filter_level(
          ArcDefaultAppList::FilterLevel::OPTIONAL_APPS);
    } else {
      default_apps_->set_filter_level(
          arc::IsArcPlayStoreEnabledForProfile(profile_)
              ? ArcDefaultAppList::FilterLevel::OPTIONAL_APPS
              : ArcDefaultAppList::FilterLevel::ALL);
    }
  } else {
    default_apps_->set_filter_level(ArcDefaultAppList::FilterLevel::NOTHING);
  }

  // Register default apps if it was not registered before.
  RegisterDefaultApps();
}

void ArcAppListPrefs::OnDefaultAppsReady() {
  VLOG(1) << "Default apps ready";

  // Deprecated. Convert uninstalled packages info to hidden default apps and
  // erase pending perf entry afterward.
  // TODO (khmel): Remove in M73
  const std::vector<std::string> uninstalled_package_names =
      GetPackagesFromPrefs(false /* check_arc_alive */, false /* installed */);
  for (const auto& uninstalled_package_name : uninstalled_package_names) {
    default_apps_->SetAppsHiddenForPackage(uninstalled_package_name);
    RemovePackageFromPrefs(uninstalled_package_name);
  }

  SetDefaultAppsFilterLevel();
  default_apps_ready_ = true;
  if (!default_apps_ready_callback_.is_null())
    std::move(default_apps_ready_callback_).Run();

  StartPrefs();
}

void ArcAppListPrefs::OnPolicySent(const std::string& policy) {
  // Update set of packages installed by policy.
  packages_by_policy_ =
      arc::policy_util::GetRequestedPackagesFromArcPolicy(policy);
}

void ArcAppListPrefs::Shutdown() {
  arc::ArcPolicyBridge* policy_bridge =
      arc::ArcPolicyBridge::GetForBrowserContext(profile_);
  if (policy_bridge)
    policy_bridge->RemoveObserver(this);
}

void ArcAppListPrefs::RegisterDefaultApps() {
  // Report default apps first, note, app_map includes uninstalled and filtered
  // out apps as well.
  for (const auto& default_app : default_apps_->GetActiveApps()) {
    const std::string& app_id = default_app.first;
    DCHECK(default_apps_->HasApp(app_id));
    // Skip already tracked app.
    if (tracked_apps_.count(app_id)) {
      // Notify that icon is ready for default app.
      for (auto& observer : observer_list_) {
        for (const auto& descriptor : active_icons_[app_id])
          observer.OnAppIconUpdated(app_id, descriptor);
      }
      continue;
    }

    const ArcDefaultAppList::AppInfo& app_info = *default_app.second;
    AddAppAndShortcut(app_info.name, app_info.package_name, app_info.activity,
                      std::string() /* intent_uri */,
                      std::string() /* icon_resource_id */, false /* sticky */,
                      false /* notifications_enabled */, false /* app_ready */,
                      false /* suspended */, false /* shortcut */,
                      true /* launchable */);
  }
}

base::Value* ArcAppListPrefs::GetPackagePrefs(const std::string& package_name,
                                              const std::string& key) {
  if (!GetPackage(package_name)) {
    LOG(ERROR) << package_name << " can not be found.";
    return nullptr;
  }
  arc::ArcAppScopedPrefUpdate update(prefs_, package_name,
                                     arc::prefs::kArcPackages);
  return update.Get()->FindKey(key);
}

void ArcAppListPrefs::SetPackagePrefs(const std::string& package_name,
                                      const std::string& key,
                                      base::Value value) {
  if (!GetPackage(package_name)) {
    LOG(ERROR) << package_name << " can not be found.";
    return;
  }
  arc::ArcAppScopedPrefUpdate update(prefs_, package_name,
                                     arc::prefs::kArcPackages);
  update.Get()->SetKey(key, std::move(value));
}

void ArcAppListPrefs::SetDefaultAppsReadyCallback(base::OnceClosure callback) {
  DCHECK(!callback.is_null());
  DCHECK(default_apps_ready_callback_.is_null());
  default_apps_ready_callback_ = std::move(callback);
  if (default_apps_ready_)
    std::move(default_apps_ready_callback_).Run();
}

void ArcAppListPrefs::SimulateDefaultAppAvailabilityTimeoutForTesting() {
  if (!detect_default_app_availability_timeout_.IsRunning())
    return;
  detect_default_app_availability_timeout_.Stop();
  DetectDefaultAppAvailability();
}

void ArcAppListPrefs::OnConnectionReady() {
  VLOG(1) << "App instance connection is ready.";
  // Note, sync_service_ may be nullptr in testing.
  sync_service_ = arc::ArcPackageSyncableService::Get(profile_);
  is_initialized_ = false;

  if (!app_list_refreshed_callback_.is_null())
    std::move(app_list_refreshed_callback_).Run();
}

void ArcAppListPrefs::OnConnectionClosed() {
  VLOG(1) << "App instance connection is closed.";
  DisableAllApps();
  installing_packages_count_ = 0;
  apps_installations_.clear();
  detect_default_app_availability_timeout_.Stop();
  ClearIconRequestRecord();

  if (sync_service_) {
    sync_service_->StopSyncing(syncer::ARC_PACKAGE);
    sync_service_ = nullptr;
  }

  is_initialized_ = false;
  package_list_initial_refreshed_ = false;
  app_list_refreshed_callback_.Reset();
}

void ArcAppListPrefs::HandleTaskCreated(const base::Optional<std::string>& name,
                                        const std::string& package_name,
                                        const std::string& activity) {
  DCHECK(IsArcAndroidEnabledForProfile(profile_));
  const std::string app_id = GetAppId(package_name, activity);
  if (IsRegistered(app_id)) {
    SetLastLaunchTime(app_id);
  } else {
    // Create runtime app entry that is valid for the current user session. This
    // entry is not shown in App Launcher and only required for shelf
    // integration.
    AddAppAndShortcut(name.value_or(std::string()), package_name, activity,
                      std::string() /* intent_uri */,
                      std::string() /* icon_resource_id */, false /* sticky */,
                      false /* notifications_enabled */, true /* app_ready */,
                      false /* suspended */, false /* shortcut */,
                      false /* launchable */);
  }
}

void ArcAppListPrefs::AddAppAndShortcut(const std::string& name,
                                        const std::string& package_name,
                                        const std::string& activity,
                                        const std::string& intent_uri,
                                        const std::string& icon_resource_id,
                                        const bool sticky,
                                        const bool notifications_enabled,
                                        const bool app_ready,
                                        const bool suspended,
                                        const bool shortcut,
                                        const bool launchable) {
  const std::string app_id = shortcut ? GetAppId(package_name, intent_uri)
                                      : GetAppId(package_name, activity);

  // Do not add Play Store in certain conditions.
  if (app_id == arc::kPlayStoreAppId) {
    // TODO(khmel): Use show_in_launcher flag to hide the Play Store app.
    // Display Play Store if we are in Demo Mode.
    // TODO(b/154290639): Remove check for |IsDemoModeOfflineEnrolled| when
    //                    fixed in Play Store.
    if (arc::IsRobotOrOfflineDemoAccountMode() &&
        !(chromeos::DemoSession::IsDeviceInDemoMode() &&
          chromeos::features::ShouldShowPlayStoreInDemoMode() &&
          !chromeos::DemoSession::IsDemoModeOfflineEnrolled())) {
      return;
    }
  }

  std::string updated_name = name;
  // Add "(beta)" string to Play Store. See crbug.com/644576 for details.
  if (app_id == arc::kPlayStoreAppId)
    updated_name = l10n_util::GetStringUTF8(IDS_ARC_PLAYSTORE_ICON_TITLE_BETA);

  base::Time last_launch_time;
  const bool was_tracked = tracked_apps_.count(app_id);
  std::unique_ptr<ArcAppListPrefs::AppInfo> app_old_info;
  if (was_tracked) {
    app_old_info = GetApp(app_id);
    DCHECK(app_old_info);
    DCHECK(launchable);
    last_launch_time = app_old_info->last_launch_time;
    if (updated_name != app_old_info->name) {
      for (auto& observer : observer_list_)
        observer.OnAppNameUpdated(app_id, updated_name);
    }
  }

  arc::ArcAppScopedPrefUpdate update(prefs_, app_id, arc::prefs::kArcApps);
  base::DictionaryValue* app_dict = update.Get();
  app_dict->SetString(kName, updated_name);
  app_dict->SetString(kPackageName, package_name);
  app_dict->SetString(kActivity, activity);
  app_dict->SetString(kIntentUri, intent_uri);
  app_dict->SetString(kIconResourceId, icon_resource_id);
  app_dict->SetBoolean(kSuspended, suspended);
  app_dict->SetBoolean(kSticky, sticky);
  app_dict->SetBoolean(kNotificationsEnabled, notifications_enabled);
  app_dict->SetBoolean(kShortcut, shortcut);
  app_dict->SetBoolean(kLaunchable, launchable);

  // Note the install time is the first time the Chrome OS sees the app, not the
  // actual install time in Android side.
  if (GetInstallTime(app_id).is_null()) {
    std::string install_time_str =
        base::NumberToString(base::Time::Now().ToInternalValue());
    app_dict->SetString(kInstallTime, install_time_str);
  }

  const bool was_disabled = ready_apps_.count(app_id) == 0;
  DCHECK(!(!was_disabled && !app_ready));
  if (was_disabled && app_ready)
    ready_apps_.insert(app_id);

  AppInfo app_info(updated_name, package_name, activity, intent_uri,
                   icon_resource_id, last_launch_time, GetInstallTime(app_id),
                   sticky, notifications_enabled, app_ready, suspended,
                   launchable && arc::ShouldShowInLauncher(app_id), shortcut,
                   launchable);

  if (was_tracked) {
    if (AreAppStatesChanged(*app_old_info, app_info)) {
      for (auto& observer : observer_list_)
        observer.OnAppStatesChanged(app_id, app_info);
    }
  } else {
    for (auto& observer : observer_list_)
      observer.OnAppRegistered(app_id, app_info);
    default_apps_->SetAppHidden(app_id, false);
    tracked_apps_.insert(app_id);
  }

  // Send pending requests in case app becomes visible.
  if (!app_old_info || !app_old_info->ready) {
    for (const auto& descriptor : request_icon_recorded_[app_id])
      RequestIcon(app_id, descriptor);
  }

  if (app_ready) {
    const bool deferred_notifications_enabled =
        NotificationsEnabledDeferred(prefs_).Get(app_id);
    if (deferred_notifications_enabled)
      SetNotificationsEnabled(app_id, deferred_notifications_enabled);

    // Invalidate app icons in case it was already registered, becomes ready and
    // icon version is updated. This allows to use previous icons until new
    // icons are been prepared.
    const base::Value* existing_version = app_dict->FindKey(kIconVersion);
    if (was_tracked && (!existing_version ||
                        existing_version->GetInt() != current_icons_version)) {
      VLOG(1) << "Invalidate icons for " << app_id << " from "
              << (existing_version ? existing_version->GetInt() : -1) << " to "
              << current_icons_version;
      InvalidateAppIcons(app_id);
    }

    app_dict->SetKey(kIconVersion, base::Value(current_icons_version));

    if (arc::IsArcForceCacheAppIcon()) {
      // Request full set of app icons.
      VLOG(1) << "Requested full set of app icons " << app_id;
      for (auto scale_factor : ui::GetSupportedScaleFactors()) {
        for (int dip_size : default_app_icon_dip_sizes) {
          MaybeRequestIcon(app_id,
                           ArcAppIconDescriptor(dip_size, scale_factor));
        }
      }
    }
  }
}

void ArcAppListPrefs::RemoveApp(const std::string& app_id) {
  // Delete cached icon if there is any.
  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = GetApp(app_id);
  if (app_info && !app_info->icon_resource_id.empty())
    arc::RemoveCachedIcon(app_info->icon_resource_id);

  MaybeRemoveIconRequestRecord(app_id);

  // From now, app is not available.
  ready_apps_.erase(app_id);
  active_icons_.erase(app_id);

  // In case default app, mark it as hidden.
  default_apps_->SetAppHidden(app_id, true);

  // Remove asyncronously local data on file system.
  ScheduleAppFolderDeletion(app_id);

  // Remove from prefs.
  DictionaryPrefUpdate update(prefs_, arc::prefs::kArcApps);
  base::DictionaryValue* apps = update.Get();
  const bool removed = apps->Remove(app_id, nullptr);
  DCHECK(removed);

  // |tracked_apps_| contains apps that are reported externally as available.
  // However, in case ARC++ appears as disbled on next start and had some apps
  // left in prefs from the previous session, app clean up is performed on very
  // early stage. Don't report |OnAppRemoved| in this case once the app was not
  // reported as available for the current session.
  if (!tracked_apps_.count(app_id))
    return;

  for (auto& observer : observer_list_)
    observer.OnAppRemoved(app_id);
  tracked_apps_.erase(app_id);
}

arc::ConnectionHolder<arc::mojom::AppInstance, arc::mojom::AppHost>*
ArcAppListPrefs::app_connection_holder() {
  // Some tests set their own holder. If it's set, return the holder.
  if (app_connection_holder_for_testing_)
    return app_connection_holder_for_testing_;
  auto* arc_service_manager = arc::ArcServiceManager::Get();
  // The null check is for unit tests. On production, |arc_service_manager| is
  // always non-null.
  if (!arc_service_manager)
    return nullptr;
  return arc_service_manager->arc_bridge_service()->app();
}

void ArcAppListPrefs::AddOrUpdatePackagePrefs(
    const arc::mojom::ArcPackageInfo& package) {
  DCHECK(IsArcAndroidEnabledForProfile(profile_));
  const std::string& package_name = package.package_name;

  if (package_name.empty()) {
    VLOG(2) << "Package name cannot be empty.";
    return;
  }

  arc::ArcAppScopedPrefUpdate update(prefs_, package_name,
                                     arc::prefs::kArcPackages);
  base::DictionaryValue* package_dict = update.Get();
  const std::string id_str =
      base::NumberToString(package.last_backup_android_id);
  const std::string time_str = base::NumberToString(package.last_backup_time);

  int old_package_version =
      package_dict->FindIntKey(kPackageVersion).value_or(-1);
  package_dict->SetBoolean(kShouldSync, package.sync);
  package_dict->SetInteger(kPackageVersion, package.package_version);
  package_dict->SetString(kLastBackupAndroidId, id_str);
  package_dict->SetString(kLastBackupTime, time_str);
  package_dict->SetBoolean(kSystem, package.system);
  package_dict->SetBoolean(kUninstalled, false);
  package_dict->SetBoolean(kVPNProvider, package.vpn_provider);

  base::DictionaryValue permissions_dict;
  if (package.permission_states.has_value()) {
    // Support new format
    for (const auto& permission : package.permission_states.value()) {
      base::DictionaryValue permission_state_dict;
      permission_state_dict.SetBoolKey(kPermissionStateGranted,
                                       permission.second->granted);
      permission_state_dict.SetBoolKey(kPermissionStateManaged,
                                       permission.second->managed);
      permissions_dict.SetKey(
          base::NumberToString(static_cast<int64_t>(permission.first)),
          std::move(permission_state_dict));
    }
    package_dict->SetKey(kPermissionStates, std::move(permissions_dict));
  } else if (package.permissions.has_value()) {
    // Support deprecated format
    for (const auto& permission : package.permissions.value()) {
      base::DictionaryValue permission_state_dict;
      permission_state_dict.SetBoolKey(kPermissionStateGranted,
                                       permission.second);
      // Assume deprecated format is not managed.
      permission_state_dict.SetBoolKey(kPermissionStateManaged, false);
      permissions_dict.SetKey(
          base::NumberToString(static_cast<int64_t>(permission.first)),
          std::move(permission_state_dict));
    }
    package_dict->SetKey(kPermissionStates, std::move(permissions_dict));
  } else {
    // Remove kPermissionStates from dict if there are no permissions.
    package_dict->RemoveKey(kPermissionStates);
  }

  // TODO (crbug.com/xxxxx): Remove in M78. This is required to force updating
  // icons for all packages in case framework version is changed. Prior to this
  // change |InvalidatePackageIcons| for framework did not refresh all packages.
  if (package_name == kFrameworkPackageName) {
    const int last_framework_version =
        profile_->GetPrefs()->GetInteger(arc::prefs::kArcFrameworkVersion);
    if (last_framework_version != package.package_version) {
      InvalidatePackageIcons(package_name);
      profile_->GetPrefs()->SetInteger(arc::prefs::kArcFrameworkVersion,
                                       package.package_version);
    }
    return;
  }

  if (old_package_version == -1 ||
      old_package_version == package.package_version) {
    return;
  }

  InvalidatePackageIcons(package_name);
}

void ArcAppListPrefs::RemovePackageFromPrefs(const std::string& package_name) {
  DictionaryPrefUpdate(prefs_, arc::prefs::kArcPackages)
      .Get()
      ->RemoveKey(package_name);
}

void ArcAppListPrefs::OnAppListRefreshed(
    std::vector<arc::mojom::AppInfoPtr> apps) {
  DCHECK(app_list_refreshed_callback_.is_null());
  if (!app_connection_holder()->IsConnected()) {
    LOG(ERROR) << "App instance is not connected. Delaying app list refresh. "
               << "See b/70566216.";
    app_list_refreshed_callback_ =
        base::BindOnce(&ArcAppListPrefs::OnAppListRefreshed,
                       weak_ptr_factory_.GetWeakPtr(), std::move(apps));
    return;
  }

  DCHECK(IsArcAndroidEnabledForProfile(profile_));
  std::vector<std::string> old_apps = GetAppIds();

  ready_apps_.clear();
  for (const auto& app : apps) {
    AddAppAndShortcut(
        app->name, app->package_name, app->activity,
        std::string() /* intent_uri */, std::string() /* icon_resource_id */,
        app->sticky, app->notifications_enabled, true /* app_ready */,
        app->suspended, false /* shortcut */, true /* launchable */);
  }

  // Detect removed ARC apps after current refresh.
  for (const auto& app_id : old_apps) {
    if (ready_apps_.count(app_id))
      continue;

    if (IsShortcut(app_id)) {
      // If this is a shortcut, we just mark it as ready.
      ready_apps_.insert(app_id);
      NotifyAppStatesChanged(app_id);
    } else {
      // Default apps may not be installed yet at this moment.
      if (!default_apps_->HasApp(app_id))
        RemoveApp(app_id);
    }
  }

  if (!is_initialized_) {
    is_initialized_ = true;

    UMA_HISTOGRAM_COUNTS_1000("Arc.AppsInstalledAtStartup", ready_apps_.size());

    arc::ArcPaiStarter* pai_starter =
        arc::ArcSessionManager::Get()->pai_starter();

    if (pai_starter) {
      pai_starter->AddOnStartCallback(
          base::BindOnce(&ArcAppListPrefs::MaybeSetDefaultAppLoadingTimeout,
                         weak_ptr_factory_.GetWeakPtr()));
    } else {
      MaybeSetDefaultAppLoadingTimeout();
    }
  }
}

void ArcAppListPrefs::DetectDefaultAppAvailability() {
  for (const auto& package : default_apps_->GetActivePackages()) {
    // Check if already installed or installation in progress.
    if (!GetPackage(package) && !apps_installations_.count(package))
      HandlePackageRemoved(package);
  }
}

void ArcAppListPrefs::MaybeSetDefaultAppLoadingTimeout() {
  // Find at least one not installed default app package.
  for (const auto& package : default_apps_->GetActivePackages()) {
    if (!GetPackage(package)) {
      detect_default_app_availability_timeout_.Start(FROM_HERE,
          kDetectDefaultAppAvailabilityTimeout, this,
          &ArcAppListPrefs::DetectDefaultAppAvailability);
      break;
    }
  }
}

void ArcAppListPrefs::AddApp(const arc::mojom::AppInfo& app_info) {
  if ((app_info.name.empty() || app_info.package_name.empty() ||
       app_info.activity.empty())) {
    VLOG(2) << "App Name, package name, and activity cannot be empty.";
    return;
  }

  AddAppAndShortcut(
      app_info.name, app_info.package_name, app_info.activity,
      std::string() /* intent_uri */, std::string() /* icon_resource_id */,
      app_info.sticky, app_info.notifications_enabled, true /* app_ready */,
      app_info.suspended, false /* shortcut */, true /* launchable */);
}

void ArcAppListPrefs::OnAppAddedDeprecated(arc::mojom::AppInfoPtr app) {
  AddApp(*app);
}

void ArcAppListPrefs::InvalidateAppIcons(const std::string& app_id) {
  // Ignore Play Store app since we provide its icon in Chrome resources.
  if (app_id == arc::kPlayStoreAppId)
    return;

  // Clean up previous icon records. They may refer to outdated icons.
  MaybeRemoveIconRequestRecord(app_id);

  // Clear icon cache that contains outdated icons.
  ScheduleAppFolderDeletion(app_id);

  // Re-request active icons.
  for (const auto& descriptor : active_icons_[app_id])
    MaybeRequestIcon(app_id, descriptor);
}

void ArcAppListPrefs::InvalidatePackageIcons(const std::string& package_name) {
  if (package_name == kFrameworkPackageName) {
    VLOG(1)
        << "Android framework was changed, refreshing icons for all packages";
    for (const auto& package_name_to_invalidate : GetPackagesFromPrefs()) {
      if (package_name_to_invalidate != kFrameworkPackageName)
        InvalidatePackageIcons(package_name_to_invalidate);
    }
  }
  for (const std::string& app_id : GetAppsForPackage(package_name))
    InvalidateAppIcons(app_id);
}

void ArcAppListPrefs::ScheduleAppFolderDeletion(const std::string& app_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  file_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DeleteAppFolderFromFileThread, GetAppPath(app_id)));
}

void ArcAppListPrefs::OnPackageAppListRefreshed(
    const std::string& package_name,
    std::vector<arc::mojom::AppInfoPtr> apps) {
  if (package_name.empty()) {
    VLOG(2) << "Package name cannot be empty.";
    return;
  }

  std::unordered_set<std::string> apps_to_remove =
      GetAppsAndShortcutsForPackage(package_name,
                                    true, /* include_only_launchable_apps */
                                    false /* include_shortcuts */);

  for (const auto& app : apps) {
    const std::string app_id = GetAppId(app->package_name, app->activity);
    apps_to_remove.erase(app_id);

    AddApp(*app);
  }

  arc::ArcAppScopedPrefUpdate update(prefs_, package_name,
                                     arc::prefs::kArcPackages);
  base::DictionaryValue* package_dict = update.Get();
  if (!apps_to_remove.empty()) {
    auto* launcher_controller = ChromeLauncherController::instance();
    if (launcher_controller) {
      int pin_index =
          launcher_controller->PinnedItemIndexByAppID(*apps_to_remove.begin());
      package_dict->SetInteger(kPinIndex, pin_index);
    }
  }

  for (const auto& app_id : apps_to_remove)
    RemoveApp(app_id);
}

void ArcAppListPrefs::OnInstallShortcut(arc::mojom::ShortcutInfoPtr shortcut) {
  if ((shortcut->name.empty() || shortcut->intent_uri.empty())) {
    VLOG(2) << "Shortcut Name, and intent_uri cannot be empty.";
    return;
  }

  AddAppAndShortcut(
      shortcut->name, shortcut->package_name, std::string() /* activity */,
      shortcut->intent_uri, shortcut->icon_resource_id, false /* sticky */,
      false /* notifications_enabled */, true /* app_ready */,
      false /* suspended */, true /* shortcut */, true /* launchable */);
}

void ArcAppListPrefs::OnUninstallShortcut(const std::string& package_name,
                                          const std::string& intent_uri) {
  std::vector<std::string> shortcuts_to_remove;
  const base::DictionaryValue* apps =
      prefs_->GetDictionary(arc::prefs::kArcApps);
  for (base::DictionaryValue::Iterator app_it(*apps); !app_it.IsAtEnd();
       app_it.Advance()) {
    const base::Value* value = &app_it.value();
    const base::DictionaryValue* app;
    std::string installed_package_name;
    std::string installed_intent_uri;
    if (!value->GetAsDictionary(&app) ||
        !app->GetString(kPackageName, &installed_package_name) ||
        !app->GetString(kIntentUri, &installed_intent_uri)) {
      VLOG(2) << "Failed to extract information for " << app_it.key() << ".";
      continue;
    }
    const bool shortcut = app->FindBoolKey(kShortcut).value_or(false);
    if (!shortcut || installed_package_name != package_name ||
        installed_intent_uri != intent_uri) {
      continue;
    }

    shortcuts_to_remove.push_back(app_it.key());
  }

  for (const auto& shortcut_id : shortcuts_to_remove)
    RemoveApp(shortcut_id);
}

std::unordered_set<std::string> ArcAppListPrefs::GetAppsForPackage(
    const std::string& package_name) const {
  return GetAppsAndShortcutsForPackage(package_name,
                                       false, /* include_only_launchable_apps */
                                       false /* include_shortcuts */);
}

std::unordered_set<std::string> ArcAppListPrefs::GetAppsAndShortcutsForPackage(
    const std::string& package_name,
    bool include_only_launchable_apps,
    bool include_shortcuts) const {
  std::unordered_set<std::string> app_set;
  const base::DictionaryValue* apps =
      prefs_->GetDictionary(arc::prefs::kArcApps);
  for (base::DictionaryValue::Iterator app_it(*apps); !app_it.IsAtEnd();
       app_it.Advance()) {
    if (!crx_file::id_util::IdIsValid(app_it.key()))
      continue;

    const base::Value* value = &app_it.value();
    const base::DictionaryValue* app;
    if (!value->GetAsDictionary(&app)) {
      NOTREACHED();
      continue;
    }

    std::string app_package;
    if (!app->GetString(kPackageName, &app_package)) {
      LOG(ERROR) << "App is malformed: " << app_it.key();
      continue;
    }

    if (package_name != app_package)
      continue;

    if (!include_shortcuts) {
      if (app->FindBoolKey(kShortcut).value_or(false))
        continue;
    }

    if (include_only_launchable_apps) {
      // Filter out non-lauchable apps.
      if (!app->FindBoolKey(kLaunchable).value_or(false))
        continue;
    }

    app_set.insert(app_it.key());
  }

  return app_set;
}

void ArcAppListPrefs::HandlePackageRemoved(const std::string& package_name) {
  DCHECK(IsArcAndroidEnabledForProfile(profile_));
  const std::unordered_set<std::string> apps_to_remove =
      GetAppsAndShortcutsForPackage(package_name,
                                    false /* include_only_launchable_apps */,
                                    true /* include_shortcuts */);
  for (const auto& app_id : apps_to_remove)
    RemoveApp(app_id);

  RemovePackageFromPrefs(package_name);
}

void ArcAppListPrefs::OnPackageRemoved(const std::string& package_name) {
  UMA_HISTOGRAM_ENUMERATION("Arc.AppUninstallReason",
                            UninstallCounterReasonEnum::USER);
  HandlePackageRemoved(package_name);

  for (auto& observer : observer_list_)
    observer.OnPackageRemoved(package_name, true);
}

void ArcAppListPrefs::OnIcon(const std::string& app_id,
                             const ArcAppIconDescriptor& descriptor,
                             arc::mojom::RawIconPngDataPtr icon) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!icon || !icon->icon_png_data.has_value() ||
      icon->icon_png_data->empty()) {
    LOG(WARNING) << "Cannot fetch icon for " << app_id;
    return;
  }

  if (!IsRegistered(app_id)) {
    VLOG(2) << "Request to update icon for non-registered app: " << app_id;
    return;
  }

  InstallIcon(app_id, descriptor, std::move(icon));
}

void ArcAppListPrefs::OnIconLoaded(const std::string& app_id,
                                   const ArcAppIconDescriptor& descriptor,
                                   arc::mojom::RawIconPngDataPtr icon) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (icon->icon_png_data->empty()) {
    LOG(WARNING) << "Cannot fetch icon for " << app_id;
    return;
  }

  if (!IsRegistered(app_id)) {
    VLOG(2) << "Request to update icon for non-registered app: " << app_id;
    return;
  }

  InstallIcon(app_id, descriptor, std::move(icon));
}

void ArcAppListPrefs::OnTaskCreated(int32_t task_id,
                                    const std::string& package_name,
                                    const std::string& activity,
                                    const base::Optional<std::string>& name,
                                    const base::Optional<std::string>& intent) {
  HandleTaskCreated(name, package_name, activity);
  for (auto& observer : observer_list_) {
    observer.OnTaskCreated(task_id,
                           package_name,
                           activity,
                           intent.value_or(std::string()));
  }
}

void ArcAppListPrefs::OnTaskDescriptionUpdated(
    int32_t task_id,
    const std::string& label,
    const std::vector<uint8_t>& icon_png_data) {
  arc::mojom::RawIconPngDataPtr icon = arc::mojom::RawIconPngData::New();
  icon->is_adaptive_icon = false;
  icon->icon_png_data =
      std::vector<uint8_t>(icon_png_data.begin(), icon_png_data.end());
  for (auto& observer : observer_list_)
    observer.OnTaskDescriptionChanged(task_id, label, *icon);
}

void ArcAppListPrefs::OnTaskDescriptionChanged(
    int32_t task_id,
    const std::string& label,
    arc::mojom::RawIconPngDataPtr icon) {
  for (auto& observer : observer_list_)
    observer.OnTaskDescriptionChanged(task_id, label, *icon);
}

void ArcAppListPrefs::OnTaskDestroyed(int32_t task_id) {
  for (auto& observer : observer_list_)
    observer.OnTaskDestroyed(task_id);
}

void ArcAppListPrefs::OnTaskSetActive(int32_t task_id) {
  for (auto& observer : observer_list_)
    observer.OnTaskSetActive(task_id);
}

void ArcAppListPrefs::OnNotificationsEnabledChanged(
    const std::string& package_name,
    bool enabled) {
  const base::DictionaryValue* apps =
      prefs_->GetDictionary(arc::prefs::kArcApps);
  for (base::DictionaryValue::Iterator app(*apps); !app.IsAtEnd();
       app.Advance()) {
    const base::DictionaryValue* app_dict;
    std::string app_package_name;
    if (!app.value().GetAsDictionary(&app_dict) ||
        !app_dict->GetString(kPackageName, &app_package_name)) {
      NOTREACHED();
      continue;
    }
    if (app_package_name != package_name) {
      continue;
    }
    arc::ArcAppScopedPrefUpdate update(prefs_, app.key(), arc::prefs::kArcApps);
    base::DictionaryValue* updateing_app_dict = update.Get();
    updateing_app_dict->SetBoolean(kNotificationsEnabled, enabled);
  }
  for (auto& observer : observer_list_)
    observer.OnNotificationsEnabledChanged(package_name, enabled);
}

bool ArcAppListPrefs::IsUnknownPackage(const std::string& package_name) const {
  if (GetPackage(package_name))
    return false;
  if (sync_service_ && sync_service_->IsPackageSyncing(package_name))
    return false;
  if (default_apps_->HasPackage(package_name))
    return false;
  if (apps_installations_.count(package_name))
    return false;
  return true;
}

bool ArcAppListPrefs::IsDefaultPackage(const std::string& package_name) const {
  DCHECK(default_apps_ready_);
  return default_apps_->HasPackage(package_name) ||
         default_apps_->HasHiddenPackage(package_name);
}

void ArcAppListPrefs::OnPackageAdded(
    arc::mojom::ArcPackageInfoPtr package_info) {
  DCHECK(IsArcAndroidEnabledForProfile(profile_));

  AddOrUpdatePackagePrefs(*package_info);
  for (auto& observer : observer_list_)
    observer.OnPackageInstalled(*package_info);
}

void ArcAppListPrefs::OnPackageModified(
    arc::mojom::ArcPackageInfoPtr package_info) {
  DCHECK(IsArcAndroidEnabledForProfile(profile_));
  AddOrUpdatePackagePrefs(*package_info);
  for (auto& observer : observer_list_)
    observer.OnPackageModified(*package_info);
}

void ArcAppListPrefs::OnPackageListRefreshed(
    std::vector<arc::mojom::ArcPackageInfoPtr> packages) {
  DCHECK(IsArcAndroidEnabledForProfile(profile_));

  const base::flat_set<std::string> old_packages(GetPackagesFromPrefs());
  std::set<std::string> current_packages;

  for (const auto& package : packages) {
    AddOrUpdatePackagePrefs(*package);
    if (!base::Contains(old_packages, package->package_name)) {
      for (auto& observer : observer_list_)
        observer.OnPackageInstalled(*package);
    }
    current_packages.insert(package->package_name);
  }

  for (const auto& package_name : old_packages) {
    if (!base::Contains(current_packages, package_name)) {
      RemovePackageFromPrefs(package_name);
      for (auto& observer : observer_list_)
        observer.OnPackageRemoved(package_name, false);
    }
  }

  package_list_initial_refreshed_ = true;
  for (auto& observer : observer_list_)
    observer.OnPackageListInitialRefreshed();
}

std::vector<std::string> ArcAppListPrefs::GetPackagesFromPrefs() const {
  return GetPackagesFromPrefs(true /* check_arc_alive */, true /* installed */);
}

std::vector<std::string> ArcAppListPrefs::GetPackagesFromPrefs(
    bool check_arc_alive,
    bool installed) const {
  std::vector<std::string> packages;
  if (check_arc_alive &&
      (!IsArcAlive() || !IsArcAndroidEnabledForProfile(profile_))) {
    return packages;
  }

  const base::DictionaryValue* package_prefs =
      prefs_->GetDictionary(arc::prefs::kArcPackages);
  for (base::DictionaryValue::Iterator package(*package_prefs);
       !package.IsAtEnd(); package.Advance()) {
    const base::DictionaryValue* package_info;
    if (!package.value().GetAsDictionary(&package_info)) {
      NOTREACHED();
      continue;
    }

    const bool uninstalled =
        package_info->FindBoolKey(kUninstalled).value_or(false);
    if (installed != !uninstalled)
      continue;

    packages.push_back(package.key());
  }

  return packages;
}

base::Time ArcAppListPrefs::GetInstallTime(const std::string& app_id) const {
  const base::DictionaryValue* app = nullptr;
  const base::DictionaryValue* apps =
      prefs_->GetDictionary(arc::prefs::kArcApps);
  if (!apps || !apps->GetDictionaryWithoutPathExpansion(app_id, &app))
    return base::Time();

  std::string install_time_str;
  if (!app->GetString(kInstallTime, &install_time_str))
    return base::Time();

  int64_t install_time_i64;
  if (!base::StringToInt64(install_time_str, &install_time_i64))
    return base::Time();
  return base::Time::FromInternalValue(install_time_i64);
}

void ArcAppListPrefs::InstallIcon(const std::string& app_id,
                                  const ArcAppIconDescriptor& descriptor,
                                  arc::mojom::RawIconPngDataPtr icon) {
  const base::FilePath icon_path = GetIconPath(app_id, descriptor);
  const base::FilePath foreground_icon_path =
      GetForegroundIconPath(app_id, descriptor);
  const base::FilePath background_icon_path =
      GetBackgroundIconPath(app_id, descriptor);
  base::PostTaskAndReplyWithResult(
      file_task_runner_.get(), FROM_HERE,
      base::BindOnce(&InstallIconFromFileThread, icon_path,
                     foreground_icon_path, background_icon_path,
                     std::move(icon)),
      base::BindOnce(&ArcAppListPrefs::OnIconInstalled,
                     weak_ptr_factory_.GetWeakPtr(), app_id, descriptor));
}

void ArcAppListPrefs::OnIconInstalled(const std::string& app_id,
                                      const ArcAppIconDescriptor& descriptor,
                                      bool install_succeed) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!install_succeed)
    return;

  for (auto& observer : observer_list_)
    observer.OnAppIconUpdated(app_id, descriptor);
}

void ArcAppListPrefs::OnInstallationStarted(
    const base::Optional<std::string>& package_name) {
  ++installing_packages_count_;

  if (!package_name.has_value())
    return;

  apps_installations_.insert(*package_name);

  for (auto& observer : observer_list_)
    observer.OnInstallationStarted(*package_name);
}

void ArcAppListPrefs::OnInstallationFinished(
    arc::mojom::InstallationResultPtr result) {
  if (result) {
    apps_installations_.erase(result->package_name);
    if (default_apps_->HasPackage(result->package_name) && !result->success &&
        !GetPackage(result->package_name)) {
      HandlePackageRemoved(result->package_name);
    }
    for (auto& observer : observer_list_)
      observer.OnInstallationFinished(result->package_name, result->success);
    if (result->success) {
      InstallationCounterReasonEnum reason =
          InstallationCounterReasonEnum::USER;
      if (IsDefault(result->package_name)) {
        reason = InstallationCounterReasonEnum::DEFAULT;
      } else if (IsOem(result->package_name)) {
        reason = InstallationCounterReasonEnum::OEM;
      } else if (IsControlledByPolicy(result->package_name)) {
        reason = InstallationCounterReasonEnum::POLICY;
      }
      UMA_HISTOGRAM_ENUMERATION("Arc.AppInstalledReason", reason);
    }
  }

  if (!installing_packages_count_) {
    VLOG(2) << "Received unexpected installation finished event";
    return;
  }
  --installing_packages_count_;
}

void ArcAppListPrefs::NotifyAppStatesChanged(const std::string& app_id) {
  std::unique_ptr<AppInfo> app_info = GetAppFromPrefs(app_id);
  CHECK(app_info);
  for (auto& observer : observer_list_)
    observer.OnAppStatesChanged(app_id, *app_info);
}

// static
void ArcAppListPrefs::AppInfo::SetIgnoreCompareInstallTimeForTesting(
    bool ignore) {
  ignore_compare_app_info_install_time = ignore;
}

ArcAppListPrefs::AppInfo::AppInfo(const std::string& name,
                                  const std::string& package_name,
                                  const std::string& activity,
                                  const std::string& intent_uri,
                                  const std::string& icon_resource_id,
                                  const base::Time& last_launch_time,
                                  const base::Time& install_time,
                                  bool sticky,
                                  bool notifications_enabled,
                                  bool ready,
                                  bool suspended,
                                  bool show_in_launcher,
                                  bool shortcut,
                                  bool launchable)
    : name(name),
      package_name(package_name),
      activity(activity),
      intent_uri(intent_uri),
      icon_resource_id(icon_resource_id),
      last_launch_time(last_launch_time),
      install_time(install_time),
      sticky(sticky),
      notifications_enabled(notifications_enabled),
      ready(ready),
      suspended(suspended),
      show_in_launcher(show_in_launcher),
      shortcut(shortcut),
      launchable(launchable) {
  // If app is not launchable it also does not show in launcher.
  DCHECK(launchable || !show_in_launcher);
}

ArcAppListPrefs::AppInfo::AppInfo(const AppInfo& other) = default;

// Need to add explicit destructor for chromium style checker error:
// Complex class/struct needs an explicit out-of-line destructor
ArcAppListPrefs::AppInfo::~AppInfo() = default;

bool ArcAppListPrefs::AppInfo::operator==(const AppInfo& other) const {
  return name == other.name && package_name == other.package_name &&
         activity == other.activity && intent_uri == other.intent_uri &&
         icon_resource_id == other.icon_resource_id &&
         last_launch_time == other.last_launch_time &&
         (ignore_compare_app_info_install_time ||
          install_time == other.install_time) &&
         sticky == other.sticky &&
         notifications_enabled == other.notifications_enabled &&
         ready == other.ready && suspended == other.suspended &&
         show_in_launcher == other.show_in_launcher &&
         shortcut == other.shortcut && launchable == other.launchable;
}

ArcAppListPrefs::PackageInfo::PackageInfo(
    const std::string& package_name,
    int32_t package_version,
    int64_t last_backup_android_id,
    int64_t last_backup_time,
    bool should_sync,
    bool system,
    bool vpn_provider,
    base::flat_map<arc::mojom::AppPermission, arc::mojom::PermissionStatePtr>
        permissions)
    : package_name(package_name),
      package_version(package_version),
      last_backup_android_id(last_backup_android_id),
      last_backup_time(last_backup_time),
      should_sync(should_sync),
      system(system),
      vpn_provider(vpn_provider),
      permissions(std::move(permissions)) {}

// Need to add explicit destructor for chromium style checker error:
// Complex class/struct needs an explicit out-of-line destructor
ArcAppListPrefs::PackageInfo::~PackageInfo() = default;
