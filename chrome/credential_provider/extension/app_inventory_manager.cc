// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/extension/app_inventory_manager.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "chrome/credential_provider/gaiacp/gcp_utils.h"
#include "chrome/credential_provider/gaiacp/gcpw_strings.h"
#include "chrome/credential_provider/gaiacp/logging.h"
#include "chrome/credential_provider/gaiacp/mdm_utils.h"
#include "chrome/credential_provider/gaiacp/os_user_manager.h"
#include "chrome/credential_provider/gaiacp/reg_utils.h"
#include "chrome/credential_provider/gaiacp/win_http_url_fetcher.h"

namespace credential_provider {

const base::TimeDelta kDefaultUploadAppInventoryRequestTimeout =
    base::Milliseconds(12000);

namespace {

// Constants used for contacting the gem service.
const char kGemServiceUploadAppInventoryPath[] = "/v1/uploadDeviceDetails";
const char kUploadAppInventoryRequestUserSidParameterName[] = "user_sid";
const char kUploadAppInventoryRequestDeviceResourceIdParameterName[] =
    "device_resource_id";
const char kUploadAppInventoryRequestWin32AppsParameterName[] =
    "windows_gpcw_app_info";
const char kDmToken[] = "dm_token";
const char kObfuscatedGaiaId[] = "obfuscated_gaia_id";
const char kAppDisplayName[] = "name";
const char kAppDisplayVersion[] = "version";
const char kAppPublisher[] = "publisher";
const char kAppType[] = "app_type";

const wchar_t kInstalledWin32AppsRegistryPath[] =
    L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall";
const wchar_t kInstalledWin32AppsRegistryPathWOW6432[] =
    L"SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall";
const wchar_t kDelimiter[] = L"\\";
const wchar_t kAppDisplayNameRegistryKey[] = L"DisplayName";
const wchar_t kAppDisplayVersionRegistryKey[] = L"DisplayVersion";
const wchar_t kAppPublisherRegistryKey[] = L"Publisher";

// Registry key to control whether upload app data from ESA feature is
// enabled.
const wchar_t kUploadAppInventoryFromEsaEnabledRegKey[] =
    L"upload_app_inventory_from_esa";

// The period of uploading app inventory to the backend.
const base::TimeDelta kUploadAppInventoryExecutionPeriod = base::Hours(3);

// True when upload device details from ESA feature is enabled.
bool g_upload_app_inventory_from_esa_enabled = false;

// Maximum number of retries if a HTTP call to the backend fails.
constexpr unsigned int kMaxNumHttpRetries = 3;

// Defines a task that is called by the ESA to upload app data.
class UploadAppInventoryTask : public extension::Task {
 public:
  static std::unique_ptr<extension::Task> Create() {
    std::unique_ptr<extension::Task> esa_task(new UploadAppInventoryTask());
    return esa_task;
  }

  // ESA calls this to retrieve a configuration for the task execution. Return
  // a default config for now.
  extension::Config GetConfig() final {
    extension::Config config;
    config.execution_period = kUploadAppInventoryExecutionPeriod;
    return config;
  }

  // ESA calls this to set all the user-device contexts for the execution of the
  // task.
  HRESULT SetContext(const std::vector<extension::UserDeviceContext>& c) final {
    context_ = c;
    return S_OK;
  }

  // ESA calls execute function to perform the actual task.
  HRESULT Execute() final {
    HRESULT task_status = S_OK;
    for (const auto& c : context_) {
      HRESULT hr = AppInventoryManager::Get()->UploadAppInventory(c);
      if (FAILED(hr)) {
        LOGFN(ERROR) << "Failed uploading device details for " << c.user_sid
                     << ". hr=" << putHR(hr);
        task_status = hr;
      }
    }
    return task_status;
  }

 private:
  std::vector<extension::UserDeviceContext> context_;
};
}  // namespace

// static
AppInventoryManager* AppInventoryManager::Get() {
  return *GetInstanceStorage();
}

// static
AppInventoryManager** AppInventoryManager::GetInstanceStorage() {
  static AppInventoryManager instance(kDefaultUploadAppInventoryRequestTimeout);
  static AppInventoryManager* instance_storage = &instance;
  return &instance_storage;
}

// static
extension::TaskCreator AppInventoryManager::UploadAppInventoryTaskCreator() {
  return base::BindRepeating(&UploadAppInventoryTask::Create);
}

AppInventoryManager::AppInventoryManager(
    base::TimeDelta upload_app_inventory_request_timeout)
    : upload_app_inventory_request_timeout_(
          upload_app_inventory_request_timeout) {
  g_upload_app_inventory_from_esa_enabled =
      GetGlobalFlagOrDefault(kUploadAppInventoryFromEsaEnabledRegKey, 1) == 1;
}

AppInventoryManager::~AppInventoryManager() = default;

GURL AppInventoryManager::GetGemServiceUploadAppInventoryUrl() {
  GURL gem_service_url = GetGcpwServiceUrl();
  return gem_service_url.Resolve(kGemServiceUploadAppInventoryPath);
}

bool AppInventoryManager::UploadAppInventoryFromEsaFeatureEnabled() const {
  return g_upload_app_inventory_from_esa_enabled;
}

// Uploads the app data into GEM database using |dm_token|
// for authentication and authorization. The GEM service would use
// |resource_id| for identifying the device entry in GEM database.
HRESULT AppInventoryManager::UploadAppInventory(
    const extension::UserDeviceContext& context) {
  std::wstring obfuscated_user_id;
  HRESULT status = GetIdFromSid(context.user_sid.c_str(), &obfuscated_user_id);
  if (FAILED(status)) {
    LOGFN(ERROR) << "Could not get user id from sid " << context.user_sid;
    return status;
  }

  if (obfuscated_user_id.empty()) {
    LOGFN(ERROR) << "Got empty user id from sid " << context.user_sid;
    return E_FAIL;
  }

  std::wstring dm_token_value = context.dm_token;
  HRESULT hr;
  if (dm_token_value.empty()) {
    hr = GetGCPWDmToken(context.user_sid, &dm_token_value);
    if (FAILED(hr)) {
      LOGFN(WARNING) << "Failed to fetch DmToken hr=" << putHR(hr);
      return hr;
    }
  }

  request_dict_ = std::make_unique<base::Value::Dict>();
  request_dict_->Set(kUploadAppInventoryRequestUserSidParameterName,
                     base::WideToUTF8(context.user_sid));
  request_dict_->Set(kDmToken, base::WideToUTF8(dm_token_value));
  request_dict_->Set(kObfuscatedGaiaId, base::WideToUTF8(obfuscated_user_id));
  std::wstring known_resource_id =
      context.device_resource_id.empty()
          ? GetUserDeviceResourceId(context.user_sid)
          : context.device_resource_id;
  // ResourceId cannot be empty while uploading app data. App data is updated
  // only for devices with existing device record.
  if (known_resource_id.empty()) {
    LOGFN(ERROR) << "Could not find valid resourceId for sid:"
                 << context.user_sid;
    return E_FAIL;
  }
  request_dict_->Set(kUploadAppInventoryRequestDeviceResourceIdParameterName,
                     base::WideToUTF8(known_resource_id));

  request_dict_->Set(kUploadAppInventoryRequestWin32AppsParameterName,
                     GetInstalledWin32Apps());

  std::optional<base::Value> request_result;
  hr = WinHttpUrlFetcher::BuildRequestAndFetchResultFromHttpService(
      AppInventoryManager::Get()->GetGemServiceUploadAppInventoryUrl(),
      /* access_token= */ std::string(), {}, *request_dict_,
      upload_app_inventory_request_timeout_, kMaxNumHttpRetries,
      &request_result);

  if (FAILED(hr)) {
    LOGFN(ERROR) << "BuildRequestAndFetchResultFromHttpService hr="
                 << putHR(hr);
    return E_FAIL;
  }
  return hr;
}

base::Value AppInventoryManager::GetInstalledWin32Apps() {
  std::vector<std::wstring> app_name_list;
  std::vector<std::wstring> app_path_list;

  GetChildrenAtPath(kInstalledWin32AppsRegistryPath, app_name_list);
  for (std::wstring a : app_name_list) {
    app_path_list.push_back(std::wstring(kInstalledWin32AppsRegistryPath)
                                .append(std::wstring(kDelimiter))
                                .append(a));
  }
  app_name_list.clear();
  GetChildrenAtPath(kInstalledWin32AppsRegistryPathWOW6432, app_name_list);
  for (std::wstring a : app_name_list) {
    app_path_list.push_back(std::wstring(kInstalledWin32AppsRegistryPathWOW6432)
                                .append(std::wstring(kDelimiter))
                                .append(a));
  }

  base::Value::List app_info_value_list;
  for (std::wstring regPath : app_path_list) {
    base::Value::Dict request_dict;

    wchar_t display_name[256];
    ULONG display_length = std::size(display_name);
    HRESULT hr =
        GetMachineRegString(regPath, std::wstring(kAppDisplayNameRegistryKey),
                            display_name, &display_length);
    if (hr == S_OK) {
      request_dict.Set(kAppDisplayName, base::WideToUTF8(display_name));

      wchar_t display_version[256];
      ULONG version_length = std::size(display_version);
      hr = GetMachineRegString(regPath,
                               std::wstring(kAppDisplayVersionRegistryKey),
                               display_version, &version_length);
      if (hr == S_OK) {
        request_dict.Set(kAppDisplayVersion, base::WideToUTF8(display_version));
      }

      wchar_t publisher[256];
      ULONG publisher_length = std::size(publisher);
      hr = GetMachineRegString(regPath, std::wstring(kAppPublisherRegistryKey),
                               publisher, &publisher_length);
      if (hr == S_OK) {
        request_dict.Set(kAppPublisher, base::WideToUTF8(publisher));
      }

      // App_type value 1 refers to WIN_32 applications.
      request_dict.Set(kAppType, 1);

      app_info_value_list.Append(std::move(request_dict));
    }
  }

  return base::Value(std::move(app_info_value_list));
}

void AppInventoryManager::SetUploadAppInventoryFromEsaFeatureEnabledForTesting(
    bool value) {
  g_upload_app_inventory_from_esa_enabled = value;
}

void AppInventoryManager::SetFakesForTesting(FakesForTesting* fakes) {
  DCHECK(fakes);

  WinHttpUrlFetcher::SetCreatorForTesting(
      fakes->fake_win_http_url_fetcher_creator);
  if (fakes->os_user_manager_for_testing) {
    OSUserManager::SetInstanceForTesting(fakes->os_user_manager_for_testing);
  }
}

}  // namespace credential_provider
