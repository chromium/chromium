// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/policy_ui_handler.h"

#include <stddef.h>

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/files/file_util.h"
#include "base/json/json_writer.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/chrome_policy_conversions_client.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/schema_registry_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/browser/ui/webui/version_ui.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/channel_info.h"
#include "chrome/grit/chromium_strings.h"
#include "components/enterprise/browser/controller/browser_dm_token_storage.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/browser/cloud/message_util.h"
#include "components/policy/core/browser/configuration_policy_handler_list.h"
#include "components/policy/core/browser/policy_conversions.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_manager.h"
#include "components/policy/core/common/cloud/cloud_policy_refresh_scheduler.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/cloud_policy_util.h"
#include "components/policy/core/common/cloud/cloud_policy_validator.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_store.h"
#include "components/policy/core/common/policy_details.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_scheduler.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/remote_commands/remote_commands_service.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/core/common/schema_map.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/strings/grit/components_strings.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/web_contents.h"
#include "extensions/buildflags/buildflags.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/base/webui/web_ui_util.h"

#if defined(OS_ANDROID)
#include "chrome/browser/ui/android/android_about_app_info.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/policy/active_directory_policy_manager.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_store_chromeos.h"
#include "chrome/browser/chromeos/policy/device_local_account_policy_service.h"
#include "chrome/browser/chromeos/policy/off_hours/device_off_hours_controller.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chromeos/dbus/util/version_loader.h"
#include "components/user_manager/user_manager.h"
#else
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#endif

#if defined(OS_MAC)
#include "base/mac/mac_util.h"
#endif

#if defined(OS_WIN)
#include "chrome/browser/ui/webui/version_util_win.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#endif

#if defined(OS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include <DSRole.h>

#include "chrome/browser/google/google_update_policy_fetcher_win.h"
#include "chrome/install_static/install_util.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#endif  // defined(OS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)

namespace em = enterprise_management;

namespace {

// Formats the association state indicated by |data|. If |data| is NULL, the
// state is considered to be UNMANAGED.
base::string16 FormatAssociationState(const em::PolicyData* data) {
  if (data) {
    switch (data->state()) {
      case em::PolicyData::ACTIVE:
        return l10n_util::GetStringUTF16(IDS_POLICY_ASSOCIATION_STATE_ACTIVE);
      case em::PolicyData::UNMANAGED:
        return l10n_util::GetStringUTF16(
            IDS_POLICY_ASSOCIATION_STATE_UNMANAGED);
      case em::PolicyData::DEPROVISIONED:
        return l10n_util::GetStringUTF16(
            IDS_POLICY_ASSOCIATION_STATE_DEPROVISIONED);
    }
    NOTREACHED() << "Unknown state " << data->state();
  }

  // Default to UNMANAGED for the case of missing policy or bad state enum.
  return l10n_util::GetStringUTF16(IDS_POLICY_ASSOCIATION_STATE_UNMANAGED);
}

// CloudPolicyStore errors take precedence to show in the status message.
// Other errors (such as transient policy fetching problems) get displayed
// only if CloudPolicyStore is in STATUS_OK.
base::string16 GetPolicyStatusFromStore(
    const policy::CloudPolicyStore* store,
    const policy::CloudPolicyClient* client) {
  base::string16 status =
      policy::FormatStoreStatus(store->status(), store->validation_status());
  if (store->status() == policy::CloudPolicyStore::STATUS_OK) {
    if (client && client->status() != policy::DM_STATUS_SUCCESS)
      status = policy::FormatDeviceManagementStatus(client->status());
    else if (!store->is_managed())
      status = FormatAssociationState(store->policy());
  }
  return status;
}

base::string16 GetTimeSinceLastRefreshString(base::Time last_refresh_time) {
  if (last_refresh_time.is_null())
    return l10n_util::GetStringUTF16(IDS_POLICY_NEVER_FETCHED);
  base::Time now = base::Time::NowFromSystemTime();
  base::TimeDelta elapsed_time;
  if (now > last_refresh_time)
    elapsed_time = now - last_refresh_time;
  return ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_ELAPSED,
                                ui::TimeFormat::LENGTH_SHORT, elapsed_time);
}

void GetStatusFromCore(const policy::CloudPolicyCore* core,
                       base::DictionaryValue* dict) {
  const policy::CloudPolicyStore* store = core->store();
  const policy::CloudPolicyClient* client = core->client();
  const policy::CloudPolicyRefreshScheduler* refresh_scheduler =
      core->refresh_scheduler();

  const base::string16 status = GetPolicyStatusFromStore(store, client);

  const em::PolicyData* policy = store->policy();
  std::string client_id = policy ? policy->device_id() : std::string();
  std::string username = policy ? policy->username() : std::string();

  if (policy && policy->has_annotated_asset_id())
    dict->SetString("assetId", policy->annotated_asset_id());
  if (policy && policy->has_annotated_location())
    dict->SetString("location", policy->annotated_location());
  if (policy && policy->has_directory_api_id())
    dict->SetString("directoryApiId", policy->directory_api_id());
  if (policy && policy->has_gaia_id())
    dict->SetString("gaiaId", policy->gaia_id());

  base::TimeDelta refresh_interval = base::TimeDelta::FromMilliseconds(
      refresh_scheduler
          ? refresh_scheduler->GetActualRefreshDelay()
          : policy::CloudPolicyRefreshScheduler::kDefaultRefreshDelayMs);
  base::Time last_refresh_time =
      refresh_scheduler ? refresh_scheduler->last_refresh() : base::Time();

  bool no_error = store->status() == policy::CloudPolicyStore::STATUS_OK &&
                  client && client->status() == policy::DM_STATUS_SUCCESS;
  dict->SetBoolean("error", !no_error);
  dict->SetBoolean(
      "policiesPushAvailable",
      refresh_scheduler ? refresh_scheduler->invalidations_available() : false);
  dict->SetString("status", status);
  dict->SetString("clientId", client_id);
  dict->SetString("username", username);
  dict->SetString(
      "refreshInterval",
      ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_DURATION,
                             ui::TimeFormat::LENGTH_SHORT, refresh_interval));
  dict->SetString("timeSinceLastRefresh",
                  GetTimeSinceLastRefreshString(last_refresh_time));
}

#if defined(OS_CHROMEOS)
// Adds a new entry to |dict| with the affiliation status of the user associated
// with |profile|. Device scope policy status providers call this method with
// nullptr |profile|. In this case no entry is added as affiliation status only
// makes sense for user scope policy status providers.
void GetUserAffiliationStatus(base::DictionaryValue* dict, Profile* profile) {
  if (!profile)
    return;
  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
  if (!user)
    return;
  dict->SetBoolean("isAffiliated", user->IsAffiliated());
}

void GetOffHoursStatus(base::DictionaryValue* dict) {
  policy::off_hours::DeviceOffHoursController* off_hours_controller =
      chromeos::DeviceSettingsService::Get()->device_off_hours_controller();
  if (off_hours_controller) {
    dict->SetBoolean("isOffHoursActive",
                     off_hours_controller->is_off_hours_mode());
  }
}
#endif  // defined(OS_CHROMEOS)

void ExtractDomainFromUsername(base::DictionaryValue* dict) {
  std::string username;
  dict->GetString("username", &username);
  if (!username.empty())
    dict->SetString("domain", gaia::ExtractDomainName(username));
}

}  // namespace

// An interface for querying the status of a policy provider.  It surfaces
// things like last fetch time or status of the backing store, but not the
// actual policies themselves.
class PolicyStatusProvider {
 public:
  PolicyStatusProvider();
  virtual ~PolicyStatusProvider();

  // Sets a callback to invoke upon status changes.
  void SetStatusChangeCallback(const base::Closure& callback);

  virtual void GetStatus(base::DictionaryValue* dict);

 protected:
  void NotifyStatusChange();

 private:
  base::Closure callback_;

  DISALLOW_COPY_AND_ASSIGN(PolicyStatusProvider);
};

// Status provider implementation that pulls cloud policy status from a
// CloudPolicyCore instance provided at construction time. Also listens for
// changes on that CloudPolicyCore and reports them through the status change
// callback.
class CloudPolicyCoreStatusProvider
    : public PolicyStatusProvider,
      public policy::CloudPolicyStore::Observer {
 public:
  explicit CloudPolicyCoreStatusProvider(policy::CloudPolicyCore* core);
  ~CloudPolicyCoreStatusProvider() override;

  // policy::CloudPolicyStore::Observer implementation.
  void OnStoreLoaded(policy::CloudPolicyStore* store) override;
  void OnStoreError(policy::CloudPolicyStore* store) override;

 protected:
  // Policy status is read from the CloudPolicyClient, CloudPolicyStore and
  // CloudPolicyRefreshScheduler hosted by this |core_|.
  policy::CloudPolicyCore* core_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CloudPolicyCoreStatusProvider);
};

// A cloud policy status provider for user policy.
class UserCloudPolicyStatusProvider : public CloudPolicyCoreStatusProvider {
 public:
  explicit UserCloudPolicyStatusProvider(policy::CloudPolicyCore* core);
  ~UserCloudPolicyStatusProvider() override;

  // CloudPolicyCoreStatusProvider implementation.
  void GetStatus(base::DictionaryValue* dict) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(UserCloudPolicyStatusProvider);
};

#if defined(OS_CHROMEOS)
// A cloud policy status provider for user policy on Chrome OS.
class UserCloudPolicyStatusProviderChromeOS
    : public UserCloudPolicyStatusProvider {
 public:
  explicit UserCloudPolicyStatusProviderChromeOS(policy::CloudPolicyCore* core,
                                                 Profile* profile);
  ~UserCloudPolicyStatusProviderChromeOS() override;

  // CloudPolicyCoreStatusProvider implementation.
  void GetStatus(base::DictionaryValue* dict) override;

 private:
  Profile* profile_;
  DISALLOW_COPY_AND_ASSIGN(UserCloudPolicyStatusProviderChromeOS);
};
#endif  // defined(OS_CHROMEOS)

#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
class MachineLevelUserCloudPolicyStatusProvider
    : public PolicyStatusProvider,
      public policy::CloudPolicyStore::Observer {
 public:
  explicit MachineLevelUserCloudPolicyStatusProvider(
      policy::CloudPolicyCore* core);
  ~MachineLevelUserCloudPolicyStatusProvider() override;

  void GetStatus(base::DictionaryValue* dict) override;

  // policy::CloudPolicyStore::Observer implementation.
  void OnStoreLoaded(policy::CloudPolicyStore* store) override;
  void OnStoreError(policy::CloudPolicyStore* store) override;

 private:
  policy::CloudPolicyCore* core_;

  DISALLOW_COPY_AND_ASSIGN(MachineLevelUserCloudPolicyStatusProvider);
};
#endif  // !defined(OS_ANDROID) && !defined(OS_CHROMEOS)

#if defined(OS_CHROMEOS)
// A cloud policy status provider for device policy.
class DeviceCloudPolicyStatusProviderChromeOS
    : public CloudPolicyCoreStatusProvider {
 public:
  explicit DeviceCloudPolicyStatusProviderChromeOS(
      policy::BrowserPolicyConnectorChromeOS* connector);
  ~DeviceCloudPolicyStatusProviderChromeOS() override;

  // CloudPolicyCoreStatusProvider implementation.
  void GetStatus(base::DictionaryValue* dict) override;

 private:
  std::string enterprise_enrollment_domain_;
  std::string enterprise_display_domain_;

  DISALLOW_COPY_AND_ASSIGN(DeviceCloudPolicyStatusProviderChromeOS);
};

// A cloud policy status provider that reads policy status from the policy core
// associated with the device-local account specified by |user_id| at
// construction time. The indirection via user ID and
// DeviceLocalAccountPolicyService is necessary because the device-local account
// may go away any time behind the scenes, at which point the status message
// text will indicate CloudPolicyStore::STATUS_BAD_STATE.
class DeviceLocalAccountPolicyStatusProvider
    : public PolicyStatusProvider,
      public policy::DeviceLocalAccountPolicyService::Observer {
 public:
  DeviceLocalAccountPolicyStatusProvider(
      const std::string& user_id,
      policy::DeviceLocalAccountPolicyService* service);
  ~DeviceLocalAccountPolicyStatusProvider() override;

  // PolicyStatusProvider implementation.
  void GetStatus(base::DictionaryValue* dict) override;

  // policy::DeviceLocalAccountPolicyService::Observer implementation.
  void OnPolicyUpdated(const std::string& user_id) override;
  void OnDeviceLocalAccountsChanged() override;

 private:
  const std::string user_id_;
  policy::DeviceLocalAccountPolicyService* service_;

  DISALLOW_COPY_AND_ASSIGN(DeviceLocalAccountPolicyStatusProvider);
};

// Provides status for Active Directory user policy.
class UserActiveDirectoryPolicyStatusProvider
    : public PolicyStatusProvider,
      public policy::CloudPolicyStore::Observer {
 public:
  explicit UserActiveDirectoryPolicyStatusProvider(
      policy::ActiveDirectoryPolicyManager* policy_manager,
      Profile* profile);

  ~UserActiveDirectoryPolicyStatusProvider() override;

  // PolicyStatusProvider implementation.
  void GetStatus(base::DictionaryValue* dict) override;

  // policy::CloudPolicyStore::Observer implementation.
  void OnStoreLoaded(policy::CloudPolicyStore* store) override;
  void OnStoreError(policy::CloudPolicyStore* store) override;

 private:
  policy::ActiveDirectoryPolicyManager* const policy_manager_;  // not owned.
  Profile* profile_;
  DISALLOW_COPY_AND_ASSIGN(UserActiveDirectoryPolicyStatusProvider);
};

// Provides status for Device Active Directory policy.
class DeviceActiveDirectoryPolicyStatusProvider
    : public UserActiveDirectoryPolicyStatusProvider {
 public:
  DeviceActiveDirectoryPolicyStatusProvider(
      policy::ActiveDirectoryPolicyManager* policy_manager,
      const std::string& enterprise_realm,
      const std::string& enterprise_display_domain);

  ~DeviceActiveDirectoryPolicyStatusProvider() override = default;

  // PolicyStatusProvider implementation.
  void GetStatus(base::DictionaryValue* dict) override;

 private:
  std::string enterprise_realm_;
  std::string enterprise_display_domain_;

  DISALLOW_COPY_AND_ASSIGN(DeviceActiveDirectoryPolicyStatusProvider);
};
#endif

#if defined(OS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
class UpdaterStatusProvider : public PolicyStatusProvider {
 public:
  UpdaterStatusProvider();
  ~UpdaterStatusProvider() override = default;
  void SetUpdaterStatus(std::unique_ptr<GoogleUpdateState> status);
  void GetStatus(base::DictionaryValue* dict) override;

 private:
  static std::string FetchActiveDirectoryDomain();
  void OnDomainReceived(std::string domain);

  std::unique_ptr<GoogleUpdateState> updater_status_;
  std::string domain_;
  base::WeakPtrFactory<UpdaterStatusProvider> weak_factory_{this};
};
#endif

PolicyStatusProvider::PolicyStatusProvider() {}

PolicyStatusProvider::~PolicyStatusProvider() {}

void PolicyStatusProvider::SetStatusChangeCallback(
    const base::Closure& callback) {
  callback_ = callback;
}

void PolicyStatusProvider::GetStatus(base::DictionaryValue* dict) {}

void PolicyStatusProvider::NotifyStatusChange() {
  if (!callback_.is_null())
    callback_.Run();
}

CloudPolicyCoreStatusProvider::CloudPolicyCoreStatusProvider(
    policy::CloudPolicyCore* core)
    : core_(core) {
  core_->store()->AddObserver(this);
  // TODO(bartfab): Add an observer that watches for client errors. Observing
  // core_->client() directly is not safe as the client may be destroyed and
  // (re-)created anytime if the user signs in or out on desktop platforms.
}

CloudPolicyCoreStatusProvider::~CloudPolicyCoreStatusProvider() {
  core_->store()->RemoveObserver(this);
}

void CloudPolicyCoreStatusProvider::OnStoreLoaded(
    policy::CloudPolicyStore* store) {
  NotifyStatusChange();
}

void CloudPolicyCoreStatusProvider::OnStoreError(
    policy::CloudPolicyStore* store) {
  NotifyStatusChange();
}

UserCloudPolicyStatusProvider::UserCloudPolicyStatusProvider(
    policy::CloudPolicyCore* core)
    : CloudPolicyCoreStatusProvider(core) {}

UserCloudPolicyStatusProvider::~UserCloudPolicyStatusProvider() {}

void UserCloudPolicyStatusProvider::GetStatus(base::DictionaryValue* dict) {
  if (!core_->store()->is_managed())
    return;
  GetStatusFromCore(core_, dict);
  ExtractDomainFromUsername(dict);
}

#if defined(OS_CHROMEOS)
UserCloudPolicyStatusProviderChromeOS::UserCloudPolicyStatusProviderChromeOS(
    policy::CloudPolicyCore* core,
    Profile* profile)
    : UserCloudPolicyStatusProvider(core) {
  profile_ = profile;
}

UserCloudPolicyStatusProviderChromeOS::
    ~UserCloudPolicyStatusProviderChromeOS() {}

void UserCloudPolicyStatusProviderChromeOS::GetStatus(
    base::DictionaryValue* dict) {
  if (!core_->store()->is_managed())
    return;
  UserCloudPolicyStatusProvider::GetStatus(dict);
  GetUserAffiliationStatus(dict, profile_);
}
#endif  // defined(OS_CHROMEOS)

#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)

MachineLevelUserCloudPolicyStatusProvider::
    MachineLevelUserCloudPolicyStatusProvider(policy::CloudPolicyCore* core)
    : core_(core) {
  if (core_->store())
    core_->store()->AddObserver(this);
}

MachineLevelUserCloudPolicyStatusProvider::
    ~MachineLevelUserCloudPolicyStatusProvider() {
  if (core_->store())
    core_->store()->RemoveObserver(this);
}

void MachineLevelUserCloudPolicyStatusProvider::GetStatus(
    base::DictionaryValue* dict) {
  policy::CloudPolicyStore* store = core_->store();
  policy::CloudPolicyClient* client = core_->client();
  policy::CloudPolicyRefreshScheduler* refresh_scheduler =
      core_->refresh_scheduler();

  policy::BrowserDMTokenStorage* dmTokenStorage =
      policy::BrowserDMTokenStorage::Get();

  dict->SetString(
      "refreshInterval",
      ui::TimeFormat::Simple(
          ui::TimeFormat::FORMAT_DURATION, ui::TimeFormat::LENGTH_SHORT,
          base::TimeDelta::FromMilliseconds(
              refresh_scheduler ? refresh_scheduler->GetActualRefreshDelay()
                                : policy::CloudPolicyRefreshScheduler::
                                      kDefaultRefreshDelayMs)));
  dict->SetBoolean(
      "policiesPushAvailable",
      refresh_scheduler ? refresh_scheduler->invalidations_available() : false);

  if (dmTokenStorage) {
    dict->SetString("enrollmentToken",
                    dmTokenStorage->RetrieveEnrollmentToken());

    dict->SetString("deviceId", dmTokenStorage->RetrieveClientId());
  }
  if (store) {
    base::string16 status = GetPolicyStatusFromStore(store, client);

    dict->SetString("status", status);
    const em::PolicyData* policy = store->policy();
    if (policy) {
      dict->SetString("timeSinceLastRefresh",
                      GetTimeSinceLastRefreshString(
                          refresh_scheduler ? refresh_scheduler->last_refresh()
                                            : base::Time()));
      std::string username = policy->username();
      dict->SetString("domain", gaia::ExtractDomainName(username));
    }
  }
  dict->SetString("machine", policy::GetMachineName());
}

void MachineLevelUserCloudPolicyStatusProvider::OnStoreLoaded(
    policy::CloudPolicyStore* store) {
  NotifyStatusChange();
}

void MachineLevelUserCloudPolicyStatusProvider::OnStoreError(
    policy::CloudPolicyStore* store) {
  NotifyStatusChange();
}

#endif  // !defined(OS_ANDROID) && !defined(OS_CHROMEOS)

#if defined(OS_CHROMEOS)
DeviceCloudPolicyStatusProviderChromeOS::
    DeviceCloudPolicyStatusProviderChromeOS(
        policy::BrowserPolicyConnectorChromeOS* connector)
    : CloudPolicyCoreStatusProvider(
          connector->GetDeviceCloudPolicyManager()->core()) {
  enterprise_enrollment_domain_ = connector->GetEnterpriseEnrollmentDomain();
  enterprise_display_domain_ = connector->GetEnterpriseDisplayDomain();
}

DeviceCloudPolicyStatusProviderChromeOS::
    ~DeviceCloudPolicyStatusProviderChromeOS() = default;

void DeviceCloudPolicyStatusProviderChromeOS::GetStatus(
    base::DictionaryValue* dict) {
  GetStatusFromCore(core_, dict);
  dict->SetString("enterpriseEnrollmentDomain", enterprise_enrollment_domain_);
  dict->SetString("enterpriseDisplayDomain", enterprise_display_domain_);
  GetOffHoursStatus(dict);
}

DeviceLocalAccountPolicyStatusProvider::DeviceLocalAccountPolicyStatusProvider(
    const std::string& user_id,
    policy::DeviceLocalAccountPolicyService* service)
    : user_id_(user_id), service_(service) {
  service_->AddObserver(this);
}

DeviceLocalAccountPolicyStatusProvider::
    ~DeviceLocalAccountPolicyStatusProvider() {
  service_->RemoveObserver(this);
}

void DeviceLocalAccountPolicyStatusProvider::GetStatus(
    base::DictionaryValue* dict) {
  const policy::DeviceLocalAccountPolicyBroker* broker =
      service_->GetBrokerForUser(user_id_);
  if (broker) {
    GetStatusFromCore(broker->core(), dict);
  } else {
    dict->SetBoolean("error", true);
    dict->SetString("status",
                    policy::FormatStoreStatus(
                        policy::CloudPolicyStore::STATUS_BAD_STATE,
                        policy::CloudPolicyValidatorBase::VALIDATION_OK));
    dict->SetString("username", std::string());
  }
  ExtractDomainFromUsername(dict);
  dict->SetBoolean("publicAccount", true);
}

void DeviceLocalAccountPolicyStatusProvider::OnPolicyUpdated(
    const std::string& user_id) {
  if (user_id == user_id_)
    NotifyStatusChange();
}

void DeviceLocalAccountPolicyStatusProvider::OnDeviceLocalAccountsChanged() {
  NotifyStatusChange();
}

UserActiveDirectoryPolicyStatusProvider::
    UserActiveDirectoryPolicyStatusProvider(
        policy::ActiveDirectoryPolicyManager* policy_manager,
        Profile* profile)
    : policy_manager_(policy_manager) {
  policy_manager_->store()->AddObserver(this);
  profile_ = profile;
}

UserActiveDirectoryPolicyStatusProvider::
    ~UserActiveDirectoryPolicyStatusProvider() {
  policy_manager_->store()->RemoveObserver(this);
}

void UserActiveDirectoryPolicyStatusProvider::GetStatus(
    base::DictionaryValue* dict) {
  const em::PolicyData* policy = policy_manager_->store()->policy();
  const std::string client_id = policy ? policy->device_id() : std::string();
  const std::string username = policy ? policy->username() : std::string();
  const base::Time last_refresh_time =
      (policy && policy->has_timestamp())
          ? base::Time::FromJavaTime(policy->timestamp())
          : base::Time();
  const base::string16 status =
      policy::FormatStoreStatus(policy_manager_->store()->status(),
                                policy_manager_->store()->validation_status());
  dict->SetString("status", status);
  dict->SetString("username", username);
  dict->SetString("clientId", client_id);

  const base::TimeDelta refresh_interval =
      policy_manager_->scheduler()->interval();
  dict->SetString(
      "refreshInterval",
      ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_DURATION,
                             ui::TimeFormat::LENGTH_SHORT, refresh_interval));

  dict->SetString("timeSinceLastRefresh",
                  GetTimeSinceLastRefreshString(last_refresh_time));
  GetUserAffiliationStatus(dict, profile_);
}

void UserActiveDirectoryPolicyStatusProvider::OnStoreLoaded(
    policy::CloudPolicyStore* store) {
  NotifyStatusChange();
}

void UserActiveDirectoryPolicyStatusProvider::OnStoreError(
    policy::CloudPolicyStore* store) {
  NotifyStatusChange();
}

DeviceActiveDirectoryPolicyStatusProvider::
    DeviceActiveDirectoryPolicyStatusProvider(
        policy::ActiveDirectoryPolicyManager* policy_manager,
        const std::string& enterprise_realm,
        const std::string& enterprise_display_domain)
    : UserActiveDirectoryPolicyStatusProvider(policy_manager, nullptr),
      enterprise_realm_(enterprise_realm),
      enterprise_display_domain_(enterprise_display_domain) {}

void DeviceActiveDirectoryPolicyStatusProvider::GetStatus(
    base::DictionaryValue* dict) {
  UserActiveDirectoryPolicyStatusProvider::GetStatus(dict);
  dict->SetString("enterpriseEnrollmentDomain", enterprise_realm_);
  dict->SetString("enterpriseDisplayDomain", enterprise_display_domain_);
}

#endif  // defined(OS_CHROMEOS)

#if defined(OS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
UpdaterStatusProvider::UpdaterStatusProvider() {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(),
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&UpdaterStatusProvider::FetchActiveDirectoryDomain),
      base::BindOnce(&UpdaterStatusProvider::OnDomainReceived,
                     weak_factory_.GetWeakPtr()));
}

void UpdaterStatusProvider::SetUpdaterStatus(
    std::unique_ptr<GoogleUpdateState> status) {
  updater_status_ = std::move(status);
  NotifyStatusChange();
}

void UpdaterStatusProvider::GetStatus(base::DictionaryValue* dict) {
  if (!domain_.empty())
    dict->SetStringKey("domain", domain_);
  if (!updater_status_)
    return;
  if (!updater_status_->version.empty())
    dict->SetStringKey("version", updater_status_->version);
  if (!updater_status_->last_checked_time.is_null()) {
    dict->SetStringKey(
        "timeSinceLastRefresh",
        GetTimeSinceLastRefreshString(updater_status_->last_checked_time));
  }
}

// static
std::string UpdaterStatusProvider::FetchActiveDirectoryDomain() {
  std::string domain;
  ::DSROLE_PRIMARY_DOMAIN_INFO_BASIC* info = nullptr;
  if (::DsRoleGetPrimaryDomainInformation(nullptr,
                                          ::DsRolePrimaryDomainInfoBasic,
                                          (PBYTE*)&info) != ERROR_SUCCESS) {
    return domain;
  }
  if (info->DomainNameDns)
    domain = base::WideToUTF8(info->DomainNameDns);
  ::DsRoleFreeMemory(info);
  return domain;
}

void UpdaterStatusProvider::OnDomainReceived(std::string domain) {
  domain_ = std::move(domain);
  NotifyStatusChange();
}

#endif  // defined(OS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)

PolicyUIHandler::PolicyUIHandler() {}

PolicyUIHandler::~PolicyUIHandler() {
  GetPolicyService()->RemoveObserver(policy::POLICY_DOMAIN_CHROME, this);
  GetPolicyService()->RemoveObserver(policy::POLICY_DOMAIN_EXTENSIONS, this);
  policy::SchemaRegistry* registry = Profile::FromWebUI(web_ui())
                                         ->GetOriginalProfile()
                                         ->GetPolicySchemaRegistryService()
                                         ->registry();
  registry->RemoveObserver(this);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::ExtensionRegistry::Get(Profile::FromWebUI(web_ui()))
      ->RemoveObserver(this);
#endif
}

void PolicyUIHandler::AddCommonLocalizedStringsToSource(
    content::WebUIDataSource* source) {
  AddLocalizedStringsBulk(source, policy::kPolicySources);

  static constexpr webui::LocalizedString kStrings[] = {
      {"conflict", IDS_POLICY_LABEL_CONFLICT},
      {"headerLevel", IDS_POLICY_HEADER_LEVEL},
      {"headerName", IDS_POLICY_HEADER_NAME},
      {"headerScope", IDS_POLICY_HEADER_SCOPE},
      {"headerSource", IDS_POLICY_HEADER_SOURCE},
      {"headerStatus", IDS_POLICY_HEADER_STATUS},
      {"headerValue", IDS_POLICY_HEADER_VALUE},
      {"warning", IDS_POLICY_HEADER_WARNING},
      {"levelMandatory", IDS_POLICY_LEVEL_MANDATORY},
      {"levelRecommended", IDS_POLICY_LEVEL_RECOMMENDED},
      {"error", IDS_POLICY_LABEL_ERROR},
      {"deprecated", IDS_POLICY_LABEL_DEPRECATED},
      {"future", IDS_POLICY_LABEL_FUTURE},
      {"ignored", IDS_POLICY_LABEL_IGNORED},
      {"notSpecified", IDS_POLICY_NOT_SPECIFIED},
      {"ok", IDS_POLICY_OK},
      {"scopeDevice", IDS_POLICY_SCOPE_DEVICE},
      {"scopeUser", IDS_POLICY_SCOPE_USER},
      {"title", IDS_POLICY_TITLE},
      {"unknown", IDS_POLICY_UNKNOWN},
      {"unset", IDS_POLICY_UNSET},
      {"value", IDS_POLICY_LABEL_VALUE},
      {"sourceDefault", IDS_POLICY_SOURCE_DEFAULT},
  };
  AddLocalizedStringsBulk(source, kStrings);

  source->UseStringsJs();
}

void PolicyUIHandler::RegisterMessages() {
  Profile* profile = Profile::FromWebUI(web_ui());
#if defined(OS_CHROMEOS)
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  if (connector->IsEnterpriseManaged()) {
    if (connector->GetDeviceActiveDirectoryPolicyManager()) {
      device_status_provider_ =
          std::make_unique<DeviceActiveDirectoryPolicyStatusProvider>(
              connector->GetDeviceActiveDirectoryPolicyManager(),
              connector->GetRealm(), connector->GetEnterpriseDisplayDomain());
    } else {
      device_status_provider_ =
          std::make_unique<DeviceCloudPolicyStatusProviderChromeOS>(connector);
    }
  }

  const user_manager::UserManager* user_manager =
      user_manager::UserManager::Get();
  policy::DeviceLocalAccountPolicyService* local_account_service =
      user_manager->IsLoggedInAsPublicAccount()
          ? connector->GetDeviceLocalAccountPolicyService()
          : nullptr;
  policy::UserCloudPolicyManagerChromeOS* user_cloud_policy =
      profile->GetUserCloudPolicyManagerChromeOS();
  policy::ActiveDirectoryPolicyManager* active_directory_policy =
      profile->GetActiveDirectoryPolicyManager();
  if (local_account_service) {
    user_status_provider_ =
        std::make_unique<DeviceLocalAccountPolicyStatusProvider>(
            user_manager->GetActiveUser()->GetAccountId().GetUserEmail(),
            local_account_service);
  } else if (user_cloud_policy) {
    user_status_provider_ =
        std::make_unique<UserCloudPolicyStatusProviderChromeOS>(
            user_cloud_policy->core(), profile);
  } else if (active_directory_policy) {
    user_status_provider_ =
        std::make_unique<UserActiveDirectoryPolicyStatusProvider>(
            active_directory_policy, profile);
  }
#else
  policy::UserCloudPolicyManager* user_cloud_policy_manager =
      profile->GetUserCloudPolicyManager();
  if (user_cloud_policy_manager) {
    user_status_provider_ = std::make_unique<UserCloudPolicyStatusProvider>(
        user_cloud_policy_manager->core());
  }

#if !defined(OS_ANDROID)
  policy::MachineLevelUserCloudPolicyManager* manager =
      g_browser_process->browser_policy_connector()
          ->machine_level_user_cloud_policy_manager();

  if (manager) {
    machine_status_provider_ =
        std::make_unique<MachineLevelUserCloudPolicyStatusProvider>(
            manager->core());
  }
#endif  // !defined(OS_ANDROID)
#endif  // defined(OS_CHROMEOS)

#if defined(OS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  ReloadUpdaterPoliciesAndState();
#endif  // defined(OS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)

  if (!user_status_provider_.get())
    user_status_provider_ = std::make_unique<PolicyStatusProvider>();
  if (!device_status_provider_.get())
    device_status_provider_ = std::make_unique<PolicyStatusProvider>();
  if (!machine_status_provider_.get())
    machine_status_provider_ = std::make_unique<PolicyStatusProvider>();
  if (!updater_status_provider_.get())
    updater_status_provider_ = std::make_unique<PolicyStatusProvider>();

  auto update_callback(base::BindRepeating(&PolicyUIHandler::SendStatus,
                                           base::Unretained(this)));
  user_status_provider_->SetStatusChangeCallback(update_callback);
  device_status_provider_->SetStatusChangeCallback(update_callback);
  machine_status_provider_->SetStatusChangeCallback(update_callback);
  updater_status_provider_->SetStatusChangeCallback(update_callback);
  GetPolicyService()->AddObserver(policy::POLICY_DOMAIN_CHROME, this);
  GetPolicyService()->AddObserver(policy::POLICY_DOMAIN_EXTENSIONS, this);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::ExtensionRegistry::Get(Profile::FromWebUI(web_ui()))
      ->AddObserver(this);
#endif
  policy::SchemaRegistry* registry = Profile::FromWebUI(web_ui())
                                         ->GetOriginalProfile()
                                         ->GetPolicySchemaRegistryService()
                                         ->registry();
  registry->AddObserver(this);

  web_ui()->RegisterMessageCallback(
      "exportPoliciesJSON",
      base::BindRepeating(&PolicyUIHandler::HandleExportPoliciesJson,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "listenPoliciesUpdates",
      base::BindRepeating(&PolicyUIHandler::HandleListenPoliciesUpdates,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "reloadPolicies",
      base::BindRepeating(&PolicyUIHandler::HandleReloadPolicies,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "copyPoliciesJSON",
      base::BindRepeating(&PolicyUIHandler::HandleCopyPoliciesJson,
                          base::Unretained(this)));
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
void PolicyUIHandler::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension) {
  SendPolicies();
}

void PolicyUIHandler::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionReason reason) {
  SendPolicies();
}
#endif

void PolicyUIHandler::OnSchemaRegistryUpdated(bool has_new_schemas) {
  // Update UI when new schema is added.
  if (has_new_schemas) {
    SendPolicies();
  }
}

void PolicyUIHandler::OnPolicyUpdated(const policy::PolicyNamespace& ns,
                                      const policy::PolicyMap& previous,
                                      const policy::PolicyMap& current) {
  SendPolicies();
}

base::Value PolicyUIHandler::GetPolicyNames() const {
  base::Value names(base::Value::Type::DICTIONARY);
  Profile* profile = Profile::FromWebUI(web_ui());
  policy::SchemaRegistry* registry = profile->GetOriginalProfile()
                                         ->GetPolicySchemaRegistryService()
                                         ->registry();
  scoped_refptr<policy::SchemaMap> schema_map = registry->schema_map();

  // Add Chrome policy names.
  base::Value chrome_policy_names(base::Value::Type::LIST);
  policy::PolicyNamespace chrome_ns(policy::POLICY_DOMAIN_CHROME, "");
  const policy::Schema* chrome_schema = schema_map->GetSchema(chrome_ns);
  for (auto it = chrome_schema->GetPropertiesIterator(); !it.IsAtEnd();
       it.Advance()) {
    chrome_policy_names.Append(base::Value(it.key()));
  }
  base::Value chrome_values(base::Value::Type::DICTIONARY);
  chrome_values.SetStringKey("name", "Chrome Policies");
  chrome_values.SetKey("policyNames", std::move(chrome_policy_names));
  names.SetKey("chrome", std::move(chrome_values));

#if defined(OS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (updater_policies_) {
    base::Value updater_policies(base::Value::Type::DICTIONARY);
    updater_policies.SetStringKey("name", "Google Update Policies");
    updater_policies.SetKey("policyNames", GetGoogleUpdatePolicyNames());
    names.SetKey("updater", std::move(updater_policies));
  }
#endif  // defined(OS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Add extension policy names.
  AddExtensionPolicyNames(&names, policy::POLICY_DOMAIN_EXTENSIONS);

#if defined(OS_CHROMEOS)
  AddExtensionPolicyNames(&names, policy::POLICY_DOMAIN_SIGNIN_EXTENSIONS);
#endif  // defined(OS_CHROMEOS)

#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

  return names;
}

base::Value PolicyUIHandler::GetPolicyValues() const {
  auto client = std::make_unique<policy::ChromePolicyConversionsClient>(
      web_ui()->GetWebContents()->GetBrowserContext());

#if defined(OS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (updater_policies_) {
    return policy::ArrayPolicyConversions(std::move(client))
        .EnableConvertValues(true)
        .WithUpdaterPolicies(updater_policies_->DeepCopy())
        .WithUpdaterPolicySchemas(GetGoogleUpdatePolicySchemas())
        .ToValue();
  }
#endif  // defined(OS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)

  return policy::ArrayPolicyConversions(std::move(client))
      .EnableConvertValues(true)
      .ToValue();
}

void PolicyUIHandler::AddExtensionPolicyNames(
    base::Value* names,
    policy::PolicyDomain policy_domain) const {
  DCHECK(names->is_dict());
#if BUILDFLAG(ENABLE_EXTENSIONS)

#if defined(OS_CHROMEOS)
  Profile* extension_profile =
      policy_domain == policy::POLICY_DOMAIN_SIGNIN_EXTENSIONS
          ? chromeos::ProfileHelper::GetSigninProfile()
          : Profile::FromWebUI(web_ui());
#else   // defined(OS_CHROMEOS)
  Profile* extension_profile = Profile::FromWebUI(web_ui());
#endif  // defined(OS_CHROMEOS)

  scoped_refptr<policy::SchemaMap> schema_map =
      extension_profile->GetOriginalProfile()
          ->GetPolicySchemaRegistryService()
          ->registry()
          ->schema_map();

  const extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(extension_profile);
  std::unique_ptr<extensions::ExtensionSet> extension_set =
      registry->GenerateInstalledExtensionsSet();

  for (const scoped_refptr<const extensions::Extension>& extension :
       *extension_set) {
    // Skip this extension if it's not an enterprise extension.
    if (!extension->manifest()->HasPath(
            extensions::manifest_keys::kStorageManagedSchema)) {
      continue;
    }
    base::Value extension_value(base::Value::Type::DICTIONARY);
    extension_value.SetStringKey("name", extension->name());
    const policy::Schema* schema = schema_map->GetSchema(
        policy::PolicyNamespace(policy_domain, extension->id()));
    base::Value policy_names(base::Value::Type::LIST);
    if (schema && schema->valid()) {
      // Get policy names from the extension's policy schema.
      // Store in a map, not an array, for faster lookup on JS side.
      for (auto prop = schema->GetPropertiesIterator(); !prop.IsAtEnd();
           prop.Advance()) {
        policy_names.Append(base::Value(prop.key()));
      }
    }
    extension_value.SetKey("policyNames", std::move(policy_names));
    names->SetKey(extension->id(), std::move(extension_value));
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
}

void PolicyUIHandler::SendStatus() {
  if (!IsJavascriptAllowed())
    return;
  std::unique_ptr<base::DictionaryValue> device_status(
      new base::DictionaryValue);
  device_status_provider_->GetStatus(device_status.get());
  if (!device_domain_.empty())
    device_status->SetString("domain", device_domain_);
  std::string domain = device_domain_;
  std::unique_ptr<base::DictionaryValue> user_status(new base::DictionaryValue);
  user_status_provider_->GetStatus(user_status.get());
  std::string username;
  user_status->GetString("username", &username);
  if (!username.empty())
    user_status->SetString("domain", gaia::ExtractDomainName(username));

  std::unique_ptr<base::DictionaryValue> machine_status(
      new base::DictionaryValue);
  machine_status_provider_->GetStatus(machine_status.get());

  std::unique_ptr<base::DictionaryValue> updater_status(
      new base::DictionaryValue);
  updater_status_provider_->GetStatus(updater_status.get());

  base::DictionaryValue status;
  if (!device_status->empty())
    status.Set("device", std::move(device_status));
  if (!machine_status->empty())
    status.Set("machine", std::move(machine_status));
  if (!user_status->empty())
    status.Set("user", std::move(user_status));
  if (!updater_status->empty())
    status.Set("updater", std::move(updater_status));

  FireWebUIListener("status-updated", status);
}

void PolicyUIHandler::HandleExportPoliciesJson(const base::ListValue* args) {
  // If the "select file" dialog window is already opened, we don't want to open
  // it again.
  if (export_policies_select_file_dialog_)
    return;

  content::WebContents* webcontents = web_ui()->GetWebContents();

  // Building initial path based on download preferences.
  base::FilePath initial_dir =
      DownloadPrefs::FromBrowserContext(webcontents->GetBrowserContext())
          ->DownloadPath();
  base::FilePath initial_path =
      initial_dir.Append(FILE_PATH_LITERAL("policies.json"));

  export_policies_select_file_dialog_ = ui::SelectFileDialog::Create(
      this,
      std::make_unique<ChromeSelectFilePolicy>(web_ui()->GetWebContents()));
  ui::SelectFileDialog::FileTypeInfo file_type_info;
  file_type_info.extensions = {{FILE_PATH_LITERAL("json")}};
  gfx::NativeWindow owning_window = webcontents->GetTopLevelNativeWindow();
  export_policies_select_file_dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_SAVEAS_FILE, base::string16(), initial_path,
      &file_type_info, 0, base::FilePath::StringType(), owning_window, nullptr);
}

void PolicyUIHandler::HandleListenPoliciesUpdates(const base::ListValue* args) {
  AllowJavascript();
  OnRefreshPoliciesDone();
}

void PolicyUIHandler::HandleReloadPolicies(const base::ListValue* args) {
#if defined(OS_CHROMEOS)
  // Allow user to manually fetch remote commands. Useful for testing or when
  // the invalidation service is not working properly.
  policy::CloudPolicyManager* const device_manager =
      g_browser_process->platform_part()
          ->browser_policy_connector_chromeos()
          ->GetDeviceCloudPolicyManager();
  Profile* const profile = Profile::FromWebUI(web_ui());
  policy::CloudPolicyManager* const user_manager =
      profile->GetUserCloudPolicyManagerChromeOS();

  // Fetch both device and user remote commands.
  for (policy::CloudPolicyManager* manager : {device_manager, user_manager}) {
    // Active Directory management has no CloudPolicyManager.
    if (manager) {
      policy::RemoteCommandsService* const remote_commands_service =
          manager->core()->remote_commands_service();
      if (remote_commands_service)
        remote_commands_service->FetchRemoteCommands();
    }
  }
#endif

#if defined(OS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  ReloadUpdaterPoliciesAndState();
#endif  // defined(OS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)

  GetPolicyService()->RefreshPolicies(base::BindOnce(
      &PolicyUIHandler::OnRefreshPoliciesDone, weak_factory_.GetWeakPtr()));
}

void PolicyUIHandler::HandleCopyPoliciesJson(const base::ListValue* args) {
  std::string policies_json = GetPoliciesAsJson();
  ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
  scw.WriteText(base::UTF8ToUTF16(policies_json));
}

std::string PolicyUIHandler::GetPoliciesAsJson() const {
  auto client = std::make_unique<policy::ChromePolicyConversionsClient>(
      web_ui()->GetWebContents()->GetBrowserContext());
  base::Value dict =
      policy::DictionaryPolicyConversions(std::move(client)).ToValue();

  base::Value chrome_metadata(base::Value::Type::DICTIONARY);

  chrome_metadata.SetKey(
      "application", base::Value(l10n_util::GetStringUTF8(IDS_PRODUCT_NAME)));
  std::string cohort_name;
#if defined(OS_WIN)
  base::string16 cohort_version_info =
      version_utils::win::GetCohortVersionInfo();
  if (!cohort_version_info.empty()) {
    cohort_name = base::StringPrintf(
        " %s", base::UTF16ToUTF8(cohort_version_info).c_str());
  }
#endif
  std::string channel_name = chrome::GetChannelName();
  std::string version = base::StringPrintf(
      "%s (%s)%s %s%s", version_info::GetVersionNumber().c_str(),
      l10n_util::GetStringUTF8(version_info::IsOfficialBuild()
                                   ? IDS_VERSION_UI_OFFICIAL
                                   : IDS_VERSION_UI_UNOFFICIAL)
          .c_str(),
      (channel_name.empty() ? "" : " " + channel_name).c_str(),
      l10n_util::GetStringUTF8(VersionUI::VersionProcessorVariation()).c_str(),
      cohort_name.c_str());
  chrome_metadata.SetKey("version", base::Value(version));

#if defined(OS_CHROMEOS)
  chrome_metadata.SetKey("platform",
                         base::Value(chromeos::version_loader::GetVersion(
                             chromeos::version_loader::VERSION_FULL)));
#elif defined(OS_MAC)
  chrome_metadata.SetKey("OS", base::Value(base::mac::GetOSDisplayName()));
#else
  std::string os = version_info::GetOSType();
#if defined(OS_WIN)
  os += " " + version_utils::win::GetFullWindowsVersion();
#elif defined(OS_ANDROID)
  os += " " + AndroidAboutAppInfo::GetOsInfo();
#endif
  chrome_metadata.SetKey("OS", base::Value(os));
#endif
  chrome_metadata.SetKey("revision",
                         base::Value(version_info::GetLastChange()));

  dict.SetKey("chromeMetadata", std::move(chrome_metadata));

  std::string json_policies;
  base::JSONWriter::WriteWithOptions(
      dict, base::JSONWriter::OPTIONS_PRETTY_PRINT, &json_policies);

  return json_policies;
}

void DoWritePoliciesToJSONFile(const base::FilePath& path,
                               const std::string& data) {
  base::WriteFile(path, data.c_str(), data.size());
}

void PolicyUIHandler::WritePoliciesToJSONFile(
    const base::FilePath& path) const {
  std::string json_policies = GetPoliciesAsJson();
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
      base::BindOnce(&DoWritePoliciesToJSONFile, path, json_policies));
}

void PolicyUIHandler::FileSelected(const base::FilePath& path,
                                   int index,
                                   void* params) {
  DCHECK(export_policies_select_file_dialog_);

  WritePoliciesToJSONFile(path);

  export_policies_select_file_dialog_ = nullptr;
}

void PolicyUIHandler::FileSelectionCanceled(void* params) {
  DCHECK(export_policies_select_file_dialog_);
  export_policies_select_file_dialog_ = nullptr;
}

void PolicyUIHandler::SendPolicies() {
  if (IsJavascriptAllowed())
    FireWebUIListener("policies-updated", GetPolicyNames(), GetPolicyValues());
}

#if defined(OS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
void PolicyUIHandler::SetUpdaterPoliciesAndState(
    std::unique_ptr<GoogleUpdatePoliciesAndState> updater_policies_and_state) {
  updater_policies_ = std::move(updater_policies_and_state->policies);
  static_cast<UpdaterStatusProvider*>(updater_status_provider_.get())
      ->SetUpdaterStatus(std::move(updater_policies_and_state->state));
  if (updater_policies_)
    SendPolicies();
}

void PolicyUIHandler::ReloadUpdaterPoliciesAndState() {
  if (!updater_status_provider_)
    updater_status_provider_ = std::make_unique<UpdaterStatusProvider>();
  base::PostTaskAndReplyWithResult(
      base::ThreadPool::CreateCOMSTATaskRunner(
          {base::TaskPriority::USER_BLOCKING,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN, base::MayBlock()})
          .get(),
      FROM_HERE, base::BindOnce(&GetGoogleUpdatePoliciesAndState),
      base::BindOnce(&PolicyUIHandler::SetUpdaterPoliciesAndState,
                     weak_factory_.GetWeakPtr()));
}

#endif  // defined(OS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)

void PolicyUIHandler::OnRefreshPoliciesDone() {
  SendPolicies();
  SendStatus();
}

policy::PolicyService* PolicyUIHandler::GetPolicyService() const {
  Profile* profile = Profile::FromBrowserContext(
      web_ui()->GetWebContents()->GetBrowserContext());
  return profile->GetProfilePolicyConnector()->policy_service();
}
