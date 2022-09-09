// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/policy/policy_ui_handler.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/files/file_util.h"
#include "base/i18n/time_formatting.h"
#include "base/json/json_writer.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/enterprise/util/affiliation.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/chrome_policy_conversions_client.h"
#include "chrome/browser/policy/policy_ui_utils.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/schema_registry_service.h"
#include "chrome/browser/policy/status_provider/cloud_policy_core_status_provider.h"
#include "chrome/browser/policy/status_provider/status_provider_util.h"
#include "chrome/browser/policy/status_provider/user_cloud_policy_status_provider.h"
#include "chrome/browser/policy/value_provider/chrome_policies_value_provider.h"
#include "chrome/browser/policy/value_provider/value_provider_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/browser/ui/managed_ui.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/grit/chromium_strings.h"
#include "components/enterprise/browser/controller/browser_dm_token_storage.h"
#include "components/enterprise/browser/reporting/common_pref_names.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/browser/cloud/message_util.h"
#include "components/policy/core/browser/configuration_policy_handler_list.h"
#include "components/policy/core/browser/policy_conversions.h"
#include "components/policy/core/browser/webui/json_generation.h"
#include "components/policy/core/browser/webui/machine_level_user_cloud_policy_status_provider.h"
#include "components/policy/core/browser/webui/policy_status_provider.h"
#include "components/policy/core/browser/webui/policy_webui_constants.h"
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
#include "components/policy/core/common/policy_scheduler.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/remote_commands/remote_commands_service.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/core/common/schema_map.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "extensions/buildflags/buildflags.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/base/webui/web_ui_util.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/policy/active_directory/active_directory_policy_manager.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/device_cloud_policy_store_ash.h"
#include "chrome/browser/ash/policy/core/device_local_account_policy_service.h"
#include "chrome/browser/ash/policy/core/user_cloud_policy_manager_ash.h"
#include "chrome/browser/ash/policy/off_hours/device_off_hours_controller.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/policy/status_provider/device_active_directory_policy_status_provider.h"
#include "chrome/browser/policy/status_provider/device_cloud_policy_status_provider_chromeos.h"
#include "chrome/browser/policy/status_provider/device_local_account_policy_status_provider.h"
#include "chrome/browser/policy/status_provider/user_active_directory_policy_status_provider.h"
#include "chrome/browser/policy/status_provider/user_cloud_policy_status_provider_chromeos.h"
#include "components/user_manager/user_manager.h"
#else
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/policy/status_provider/ash_lacros_policy_stack_bridge.h"
#include "chrome/browser/policy/status_provider/user_policy_status_provider_lacros.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/policy/value_provider/extension_policies_value_provider.h"
#endif

#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/browser/policy/status_provider/updater_status_and_value_provider.h"
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)

namespace {
void AddPolicyIdsForDisplay(const base::Value::Dict& policy_values,
                            base::Value::List& policy_ids) {
  for (const auto id_policy_pair : policy_values) {
    policy_ids.Append(id_policy_pair.first);
  }
}

// Appends the ID of `policy_values` to `policy_ids` and merges it to
// `out_policy_values`.
void MergePolicyValuesAndIds(base::Value::Dict policy_values,
                             base::Value::Dict& out_policy_values,
                             base::Value::List& out_policy_ids) {
  AddPolicyIdsForDisplay(policy_values, out_policy_ids);
  out_policy_values.Merge(std::move(policy_values));
}

// Puts `status` in `out_status` dictionary with `scope` key of `status` is not
// empty.
void SetStatus(std::string scope,
               base::Value::Dict status,
               base::Value::Dict& out_status) {
  if (!status.empty())
    out_status.Set(scope, std::move(status));
}
}  // namespace

PolicyUIHandler::PolicyUIHandler() = default;

PolicyUIHandler::~PolicyUIHandler() {
  if (export_policies_select_file_dialog_) {
    export_policies_select_file_dialog_->ListenerDestroyed();
  }
}

void PolicyUIHandler::AddCommonLocalizedStringsToSource(
    content::WebUIDataSource* source) {
  source->AddLocalizedStrings(policy::kPolicySources);

  static constexpr webui::LocalizedString kStrings[] = {
      {"conflict", IDS_POLICY_LABEL_CONFLICT},
      {"superseding", IDS_POLICY_LABEL_SUPERSEDING},
      {"conflictValue", IDS_POLICY_LABEL_CONFLICT_VALUE},
      {"supersededValue", IDS_POLICY_LABEL_SUPERSEDED_VALUE},
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
      {"info", IDS_POLICY_LABEL_INFO},
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
      {"loadPoliciesDone", IDS_POLICY_LOAD_POLICIES_DONE},
      {"loadingPolicies", IDS_POLICY_LOADING_POLICIES},
  };
  source->AddLocalizedStrings(kStrings);

  source->UseStringsJs();
}

void PolicyUIHandler::RegisterMessages() {
  Profile* profile = Profile::FromWebUI(web_ui());
#if BUILDFLAG(IS_CHROMEOS_ASH)
  policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  if (connector->IsDeviceEnterpriseManaged()) {
    if (connector->GetDeviceActiveDirectoryPolicyManager()) {
      device_status_provider_ =
          std::make_unique<DeviceActiveDirectoryPolicyStatusProvider>(
              connector->GetDeviceActiveDirectoryPolicyManager(),
              connector->GetEnterpriseDomainManager());
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
  policy::UserCloudPolicyManagerAsh* user_cloud_policy =
      profile->GetUserCloudPolicyManagerAsh();
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
        user_cloud_policy_manager->core(), profile);
  } else {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    if (profile->IsMainProfile()) {
      user_status_provider_ = std::make_unique<UserPolicyStatusProviderLacros>(
          g_browser_process->browser_policy_connector()
              ->device_account_policy_loader(),
          profile);
    }
#endif
  }

  policy::MachineLevelUserCloudPolicyManager* manager =
      g_browser_process->browser_policy_connector()
          ->machine_level_user_cloud_policy_manager();

  if (manager) {
    policy::BrowserDMTokenStorage* dmTokenStorage =
        policy::BrowserDMTokenStorage::Get();

    base::Time lastCloudReportSent;
    PrefService* prefService = g_browser_process->local_state();

    if (prefService->HasPrefPath(
            enterprise_reporting::kLastUploadSucceededTimestamp)) {
      lastCloudReportSent = prefService->GetTime(
          enterprise_reporting::kLastUploadSucceededTimestamp);
    }

    machine_status_provider_ =
        std::make_unique<policy::MachineLevelUserCloudPolicyStatusProvider>(
            manager->core(),
            new policy::MachineLevelUserCloudPolicyContext(
                {dmTokenStorage->RetrieveEnrollmentToken(),
                 dmTokenStorage->RetrieveClientId(), lastCloudReportSent}));
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // We will get device policies for Lacros through Ash, using
  // AshLacrosPolicyStackBridge.
  std::unique_ptr<AshLacrosPolicyStackBridge> policy_stack_bridge =
      std::make_unique<AshLacrosPolicyStackBridge>();
  ash_lacros_policy_stack_bridge_ = policy_stack_bridge.get();
  device_status_provider_ = std::move(policy_stack_bridge);
  policy_value_provider_observations_.AddObservation(
      ash_lacros_policy_stack_bridge_);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  if (!user_status_provider_.get())
    user_status_provider_ = std::make_unique<policy::PolicyStatusProvider>();
  if (!device_status_provider_.get())
    device_status_provider_ = std::make_unique<policy::PolicyStatusProvider>();
  if (!machine_status_provider_.get())
    machine_status_provider_ = std::make_unique<policy::PolicyStatusProvider>();

  auto update_callback(base::BindRepeating(&PolicyUIHandler::SendStatus,
                                           base::Unretained(this)));
  user_status_provider_->SetStatusChangeCallback(update_callback);
  device_status_provider_->SetStatusChangeCallback(update_callback);
  machine_status_provider_->SetStatusChangeCallback(update_callback);

#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  updater_status_and_value_provider_ =
      std::make_unique<UpdaterStatusAndValueProvider>(
          Profile::FromWebUI(web_ui()));
  policy_value_provider_observations_.AddObservation(
      updater_status_and_value_provider_.get());
  updater_status_and_value_provider_->SetStatusChangeCallback(update_callback);
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)

  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(g_browser_process->local_state());
  pref_change_registrar_->Add(
      enterprise_reporting::kLastUploadSucceededTimestamp, update_callback);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  extension_policies_value_provider_ =
      std::make_unique<ExtensionPoliciesValueProvider>(
          Profile::FromWebUI(web_ui()));
  policy_value_provider_observations_.AddObservation(
      extension_policies_value_provider_.get());
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

  chrome_policies_value_provider_ =
      std::make_unique<ChromePoliciesValueProvider>(
          Profile::FromWebUI(web_ui()));
  policy_value_provider_observations_.AddObservation(
      chrome_policies_value_provider_.get());

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

void PolicyUIHandler::OnPolicyValueChanged() {
  SendPolicies();
  // Send also the status to UI because when policy value is updated, policy
  // status also might be updated and PolicyStatusProviders may not be listening
  // this change.
  SendStatus();
}

base::Value::Dict PolicyUIHandler::GetPolicyNames() {
  base::Value::Dict names;
  names.Merge(chrome_policies_value_provider_->GetNames());

#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  names.Merge(updater_status_and_value_provider_->GetNames());
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Add extension policy names.
  names.Merge(extension_policies_value_provider_->GetNames());
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

  return names;
}

base::Value::Dict PolicyUIHandler::GetPolicyValues() {
  base::Value::Dict policy_values;
  base::Value::List policy_ids;

  MergePolicyValuesAndIds(chrome_policies_value_provider_->GetValues(),
                          policy_values, policy_ids);

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // For policy values to be merged correctly, we call GetValues() for Lacros
  // device policies after Chrome policies as described in documentation of
  // AshLacrosPolicyStackBridge.
  // Only merge policy values. We don't merge Lacros policy IDs because Lacros
  // policies has the same ID as Chrome policies.
  policy_values.Merge(ash_lacros_policy_stack_bridge_->GetValues());
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(ENABLE_EXTENSIONS)
  MergePolicyValuesAndIds(extension_policies_value_provider_->GetValues(),
                          policy_values, policy_ids);
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  MergePolicyValuesAndIds(updater_status_and_value_provider_->GetValues(),
                          policy_values, policy_ids);
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)

  // Send the policy values and list of policy IDs so the UI can display values
  // in this order.
  base::Value::Dict dict;
  dict.Set(policy::kPolicyValuesKey, std::move(policy_values));
  dict.Set(policy::kPolicyIdsKey, std::move(policy_ids));
  return dict;
}

void PolicyUIHandler::SendStatus() {
  if (!IsJavascriptAllowed())
    return;

  FireWebUIListener("status-updated", GetStatusValue());
}

base::Value::Dict PolicyUIHandler::GetStatusValue() const {
  base::Value::Dict status;
  SetStatus("device", device_status_provider_->GetStatus(), status);
  SetStatus("user", user_status_provider_->GetStatus(), status);
  SetStatus("machine", machine_status_provider_->GetStatus(), status);

#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  SetStatus("updater", updater_status_and_value_provider_->GetStatus(), status);
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)

  return status;
}

void PolicyUIHandler::HandleExportPoliciesJson(const base::Value::List& args) {
#if BUILDFLAG(IS_ANDROID)
  // TODO(crbug.com/1228691): Unify download logic between all platforms to
  // use the WebUI download solution (and remove the Android check).
  if (!IsJavascriptAllowed()) {
    DVLOG(1) << "Tried to export policies as JSON but executing JavaScript is "
                "not allowed.";
    return;
  }

  // Since file selection doesn't work as well on Android as on other platforms,
  // simply download the JSON as a file via JavaScript.
  FireWebUIListener("download-json", base::Value(GetPoliciesAsJson()));
#else
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
      ui::SelectFileDialog::SELECT_SAVEAS_FILE, std::u16string(), initial_path,
      &file_type_info, 0, base::FilePath::StringType(), owning_window, nullptr);
#endif
}

void PolicyUIHandler::HandleListenPoliciesUpdates(
    const base::Value::List& args) {
  // Send initial policy values and status to UI page.
  AllowJavascript();
  SendPolicies();
  SendStatus();
}

void PolicyUIHandler::HandleReloadPolicies(const base::Value::List& args) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Allow user to manually fetch remote commands. Useful for testing or when
  // the invalidation service is not working properly.
  policy::CloudPolicyManager* const device_manager =
      g_browser_process->platform_part()
          ->browser_policy_connector_ash()
          ->GetDeviceCloudPolicyManager();
  Profile* const profile = Profile::FromWebUI(web_ui());
  policy::CloudPolicyManager* const user_manager =
      profile->GetUserCloudPolicyManagerAsh();

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

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  ash_lacros_policy_stack_bridge_->Refresh();
#endif

#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  updater_status_and_value_provider_->Refresh();
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)

  chrome_policies_value_provider_->Refresh();
}

void PolicyUIHandler::HandleCopyPoliciesJson(const base::Value::List& args) {
  std::string policies_json = GetPoliciesAsJson();
  ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
  scw.WriteText(base::UTF8ToUTF16(policies_json));
}

std::string PolicyUIHandler::GetPoliciesAsJson() {
  auto client = std::make_unique<policy::ChromePolicyConversionsClient>(
      web_ui()->GetWebContents()->GetBrowserContext());

  policy::JsonGenerationParams params = policy::GetChromeMetadataParams(
      /*application_name=*/l10n_util::GetStringUTF8(IDS_PRODUCT_NAME));

  return policy::GenerateJson(std::move(client), GetStatusValue(), params);
}

void DoWritePoliciesToJSONFile(const base::FilePath& path,
                               const std::string& data) {
  base::WriteFile(path, data.c_str(), data.size());
}

void PolicyUIHandler::WritePoliciesToJSONFile(const base::FilePath& path) {
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
