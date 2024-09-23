// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iomanip>
#include <iostream>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/at_exit.h"
#include "base/base64.h"
#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/message_loop/message_pump_type.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/thread_pool.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "chrome/enterprise_companion/device_management_storage/dm_storage.h"
#include "chrome/updater/app/app.h"
#include "chrome/updater/configurator.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/device_management/dm_message.h"
#include "chrome/updater/device_management/dm_response_validator.h"
#include "chrome/updater/external_constants_default.h"
#include "chrome/updater/ipc/ipc_support.h"
#include "chrome/updater/policy/service.h"
#include "chrome/updater/prefs.h"
#include "chrome/updater/protos/omaha_settings.pb.h"
#include "chrome/updater/service_proxy_factory.h"
#include "chrome/updater/update_service.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace updater::tools {

constexpr char kProductSwitch[] = "product";
constexpr char kBackgroundSwitch[] = "background";
constexpr char kListAppsSwitch[] = "list-apps";
constexpr char kListUpdateSwitch[] = "list-update";
constexpr char kListPoliciesSwitch[] = "list-policies";
constexpr char kListCBCMPoliciesSwitch[] = "list-cbcm-policies";
constexpr char kCBCMPolicyPathSwitch[] = "policy-path";
constexpr char kJSONFormatSwitch[] = "json";
constexpr char kUpdateSwitch[] = "update";

namespace updater_policy {

namespace edm = ::wireless_android_enterprise_devicemanagement;

std::ostream& operator<<(std::ostream& os, edm::UpdateValue value) {
  os << base::to_underlying(value) << " ";
  switch (value) {
    case edm::UPDATES_DISABLED:
      return os << "(Disabled)";
    case edm::MANUAL_UPDATES_ONLY:
      return os << "(Manual Updates Only)";
    case edm::AUTOMATIC_UPDATES_ONLY:
      return os << "(Automatic Updates Only)";
    case edm::UPDATES_ENABLED:
    default:
      return os << "(Enabled)";
  }
}

std::ostream& operator<<(std::ostream& os, edm::InstallDefaultValue value) {
  os << base::to_underlying(value) << " ";
  switch (value) {
    case edm::INSTALL_DEFAULT_DISABLED:
      return os << "(Disabled)";
    case edm::INSTALL_DEFAULT_ENABLED_MACHINE_ONLY:
      return os << "(Enabled Machine Only)";
    case edm::INSTALL_DEFAULT_ENABLED:
    default:
      return os << "(Enabled)";
  }
}

std::ostream& operator<<(std::ostream& os, edm::InstallValue value) {
  os << base::to_underlying(value) << " ";
  switch (value) {
    case edm::INSTALL_DISABLED:
      return os << "(Disabled)";
    case edm::INSTALL_ENABLED_MACHINE_ONLY:
      return os << "(Enabled Machine Only)";
    case edm::INSTALL_FORCED:
      return os << "(Forced)";
    case edm::INSTALL_ENABLED:
    default:
      return os << "(Enabled)";
  }
}

std::ostream& operator<<(std::ostream& os,
                         PolicyValidationResult::Status status) {
  const std::string error_notes =
      base::CommandLine::ForCurrentProcess()->HasSwitch(kCBCMPolicyPathSwitch)
          ? ", expected"
          : "";
  os << base::to_underlying(status) << " ";
  switch (status) {
    case PolicyValidationResult::Status::kValidationOK:
      return os << "(OK)";
    case PolicyValidationResult::Status::kValidationBadInitialSignature:
      return os << "(Bad Initial Signature)";
    case PolicyValidationResult::Status::kValidationBadSignature:
      return os << "(Bad Signature)";
    case PolicyValidationResult::Status::kValidationErrorCodePresent:
      return os << "(Error Code Present)";
    case PolicyValidationResult::Status::kValidationPayloadParseError:
      return os << "(Payload Parse Error)";
    case PolicyValidationResult::Status::kValidationWrongPolicyType:
      return os << "(Wrong Policy Type)";
    case PolicyValidationResult::Status::kValidationWrongSettingsEntityID:
      return os << "(Wrong Settings Entity ID)";
    case PolicyValidationResult::Status::kValidationBadTimestamp:
      return os << "(Bad Timestamp" << error_notes << ")";
    case PolicyValidationResult::Status::kValidationBadDMToken:
      return os << "(Bad DMToken" << error_notes << ")";
    case PolicyValidationResult::Status::kValidationBadDeviceID:
      return os << "(Bad Device ID" << error_notes << ")";
    case PolicyValidationResult::Status::kValidationBadUser:
      return os << "(Bad User)";
    case PolicyValidationResult::Status::kValidationPolicyParseError:
      return os << "(Policy Parse Error)";
    case PolicyValidationResult::Status::kValidationBadKeyVerificationSignature:
      return os << "(Bad Key Verification Signature)";
    case PolicyValidationResult::Status::kValidationValueWarning:
      return os << "(Value Warning)";
    case PolicyValidationResult::Status::kValidationValueError:
      return os << "(Value Error)";
    default:
      return os << "(Unknown error)";
  }
}

scoped_refptr<device_management_storage::DMStorage> GetDMStorage() {
  const base::FilePath storage_path =
      base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
          kCBCMPolicyPathSwitch);
  return storage_path.empty()
             ? device_management_storage::GetDefaultDMStorage()
             : device_management_storage::CreateDMStorage(storage_path);
}

std::unique_ptr<device_management_storage::CachedPolicyInfo>
GetCachedPolicyInfo(
    scoped_refptr<device_management_storage::DMStorage> dm_storage) {
  const base::FilePath policy_info_file =
      dm_storage->policy_cache_folder().AppendASCII("CachedPolicyInfo");
  auto cached_info =
      std::make_unique<device_management_storage::CachedPolicyInfo>();
  std::string policy_info_data;
  if (base::ReadFileToString(policy_info_file, &policy_info_data)) {
    cached_info->Populate(policy_info_data);
  }
  return cached_info;
}

std::unique_ptr<edm::OmahaSettingsClientProto> GetOmahaPolicySettings() {
  std::string encoded_omaha_policy_type =
      base::Base64Encode(kGoogleUpdatePolicyType);

  base::FilePath omaha_policy_file = GetDMStorage()
                                         ->policy_cache_folder()
                                         .AppendASCII(encoded_omaha_policy_type)
                                         .AppendASCII("PolicyFetchResponse");
  std::string response_data;
  ::enterprise_management::PolicyFetchResponse response;
  ::enterprise_management::PolicyData policy_data;
  auto omaha_settings = std::make_unique<edm::OmahaSettingsClientProto>();
  if (!base::ReadFileToString(omaha_policy_file, &response_data) ||
      response_data.empty() || !response.ParseFromString(response_data) ||
      !policy_data.ParseFromString(response.policy_data()) ||
      !policy_data.has_policy_value() ||
      !omaha_settings->ParseFromString(policy_data.policy_value())) {
    VLOG(1) << "No Omaha policies.";
    return nullptr;
  }

  return omaha_settings;
}

void PrintCachedPolicy(const base::FilePath& policy_path) {
  std::string policy_type;
  if (!base::Base64Decode(policy_path.BaseName().MaybeAsASCII(),
                          &policy_type)) {
    std::cout << "Directory not base64 encoded: [" << policy_path << "]";
    return;
  }

  base::FilePath policy_file = policy_path.AppendASCII("PolicyFetchResponse");
  std::string response_data;
  ::enterprise_management::PolicyFetchResponse response;
  auto omaha_settings = std::make_unique<edm::OmahaSettingsClientProto>();
  if (!base::ReadFileToString(policy_file, &response_data) ||
      response_data.empty() || !response.ParseFromString(response_data)) {
    std::cout << "  [" << policy_type << "] <not parseable>";
    return;
  }

  scoped_refptr<device_management_storage::DMStorage> storage = GetDMStorage();
  PolicyValidationResult status;
  DMResponseValidator validator(*GetCachedPolicyInfo(storage),
                                storage->GetDmToken(), storage->GetDeviceID());
  if (validator.ValidatePolicyResponse(response, status)) {
    std::cout << "  [" << policy_type << "]: satisfies all validation check."
              << std::endl;
    return;
  }

  std::cout << "  [" << policy_type << "] validation failed: " << std::endl;
  std::cout << "    Policy type: " << status.policy_type << std::endl;
  std::cout << "    Policy token: " << status.policy_token << std::endl;
  std::cout << "    Validation status: " << status.status << std::endl;
  if (!status.issues.empty()) {
    std::cout << "    Issues: " << std::endl;
    for (const auto& issue : status.issues) {
      std::cout << "      [" << issue.policy_name << "]: " << issue.severity
                << ":" << issue.message << std::endl;
    }
  }

  std::cout << "    Policy data check: "
            << (validator.ValidatePolicyData(response) ? "OK" : "failed")
            << std::endl;
}

void PrintCachedPolicyInfo(
    const device_management_storage::CachedPolicyInfo& cached_info) {
  constexpr size_t kPrintWidth = 16;

  std::cout << "Cached policy info:" << std::endl;
  std::cout << "  Key version: " << cached_info.key_version() << std::endl;
  std::cout << "  Timestamp: " << cached_info.timestamp() << std::endl;
  std::cout << "  Key data (" << cached_info.public_key().size()
            << " bytes): " << std::endl;
  const std::string key = cached_info.public_key();
  for (size_t i = 0; i < key.size(); ++i) {
    std::cout << std::setfill('0') << std::setw(2) << std::hex
              << static_cast<unsigned int>(0xff & key[i]) << ' ';
    if (i % kPrintWidth == kPrintWidth - 1) {
      std::cout << std::endl;
    }
  }
  std::cout << std::endl;
}

void PrintCBCMPolicies() {
  scoped_refptr<device_management_storage::DMStorage> storage = GetDMStorage();
  if (!storage) {
    std::cerr << "Failed to instantiate DM storage instance." << std::endl;
    return;
  }

  std::cout << "-------------------------------------------------" << std::endl;
  std::cout << "Device ID: " << storage->GetDeviceID() << std::endl;
  std::cout << "Enrollment token: " << storage->GetEnrollmentToken()
            << std::endl;
  std::cout << "DM token: " << storage->GetDmToken() << std::endl;
  std::cout << "-------------------------------------------------" << std::endl;

  std::unique_ptr<device_management_storage::CachedPolicyInfo> cached_info =
      GetCachedPolicyInfo(storage);
  if (cached_info) {
    PrintCachedPolicyInfo(*cached_info);
    std::cout << "-------------------------------------------------"
              << std::endl;
  }

  std::cout << "Cached CBCM policies:" << std::endl;
  base::FileEnumerator(storage->policy_cache_folder(), false,
                       base::FileEnumerator::DIRECTORIES)
      .ForEach([](const base::FilePath& policy_path) {
        PrintCachedPolicy(policy_path);
      });

  std::unique_ptr<edm::OmahaSettingsClientProto> omaha_settings =
      GetOmahaPolicySettings();
  if (omaha_settings) {
    std::cout << "-------------------------------------------------"
              << std::endl;
    std::cout << "Google Update CBCM policies:" << std::endl;
    bool has_global_policy = false;
    std::cout << "  Global:" << std::endl;
    if (omaha_settings->has_install_default()) {
      std::cout << "    InstallDefault: " << omaha_settings->install_default()
                << std::endl;
      has_global_policy = true;
    }
    if (omaha_settings->has_update_default()) {
      std::cout << "    UpdateDefault: " << omaha_settings->update_default()
                << std::endl;
      has_global_policy = true;
    }
    if (omaha_settings->has_auto_update_check_period_minutes()) {
      std::cout << "    Auto-update check period minutes: " << std::dec
                << omaha_settings->auto_update_check_period_minutes()
                << std::endl;
      has_global_policy = true;
    }
    if (omaha_settings->has_updates_suppressed()) {
      std::cout << "    Update suppressed: " << std::endl
                << "        Start Hour: "
                << omaha_settings->updates_suppressed().start_hour()
                << std::endl
                << "        Start Minute: "
                << omaha_settings->updates_suppressed().start_minute()
                << std::endl
                << "        Duration Minute: "
                << omaha_settings->updates_suppressed().duration_min()
                << std::endl;
      has_global_policy = true;
    }
    if (omaha_settings->has_proxy_mode()) {
      std::cout << "    Proxy Mode: " << omaha_settings->proxy_mode()
                << std::endl;
      has_global_policy = true;
    }
    if (omaha_settings->has_proxy_pac_url()) {
      std::cout << "    Proxy PacURL: " << omaha_settings->proxy_pac_url()
                << std::endl;
      has_global_policy = true;
    }
    if (omaha_settings->has_proxy_server()) {
      std::cout << "    Proxy Server: " << omaha_settings->proxy_server()
                << std::endl;
      has_global_policy = true;
    }
    if (omaha_settings->has_download_preference()) {
      std::cout << "    DownloadPreference: "
                << omaha_settings->download_preference() << std::endl;
      has_global_policy = true;
    }
    if (!has_global_policy) {
      std::cout << "    (No policy)" << std::endl;
    }

    for (const auto& app_settings : omaha_settings->application_settings()) {
      bool has_policy = false;
      if (app_settings.has_app_guid()) {
        std::cout << "  App : " << app_settings.app_guid();
        if (app_settings.has_bundle_identifier()) {
          std::cout << " (" << app_settings.bundle_identifier() << ")";
        }
        std::cout << std::endl;
      }
      if (app_settings.has_install()) {
        std::cout << "    Install : " << app_settings.install() << std::endl;
        has_policy = true;
      }
      if (app_settings.has_update()) {
        std::cout << "    Update : " << app_settings.update() << std::endl;
        has_policy = true;
      }
      if (app_settings.has_rollback_to_target_version()) {
        std::cout << "    RollbackToTargetVersionAllowed : "
                  << app_settings.rollback_to_target_version() << std::endl;
        has_policy = true;
      }
      if (app_settings.has_target_version_prefix()) {
        std::cout << "    TargetVersionPrefix : "
                  << app_settings.target_version_prefix() << std::endl;
        has_policy = true;
      }
      if (app_settings.has_target_channel()) {
        std::cout << "    TargetChannel : " << app_settings.target_channel()
                  << std::endl;
        has_policy = true;
      }
      if (app_settings.has_gcpw_application_settings()) {
        std::cout << "    DomainsAllowedToLogin: ";
        for (const auto& domain : app_settings.gcpw_application_settings()
                                      .domains_allowed_to_login()) {
          std::cout << domain << ", ";
          has_policy = true;
        }
        std::cout << std::endl;
      }
      if (!has_policy) {
        std::cout << "    (No policy)" << std::endl;
      }
    }
  }
  std::cout << std::endl;
}

}  // namespace updater_policy

UpdaterScope Scope() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(kSystemSwitch)
             ? UpdaterScope::kSystem
             : UpdaterScope::kUser;
}

UpdateService::Priority Priority() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(kBackgroundSwitch)
             ? UpdateService::Priority::kBackground
             : UpdateService::Priority::kForeground;
}

std::string Quoted(const std::string& value) {
  return base::StrCat({"\"", value, "\""});
}

bool OutputInJSONFormat() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(kJSONFormatSwitch);
}

std::string ValueToJSONString(const base::Value& value) {
  std::string value_string;
  return base::JSONWriter::Write(value, &value_string) ? value_string : "";
}

void OnAppStateChanged(const UpdateService::UpdateState& update_state) {
  switch (update_state.state) {
    case UpdateService::UpdateState::State::kCheckingForUpdates:
      std::cout << Quoted(update_state.app_id) << ": checking update... "
                << std::endl;
      break;

    case UpdateService::UpdateState::State::kUpdateAvailable:
      std::cout << Quoted(update_state.app_id)
                << ": update found, next version = "
                << update_state.next_version << std::endl;
      break;

    case UpdateService::UpdateState::State::kDownloading:
      std::cout << Quoted(update_state.app_id)
                << ": downloading update, downloaded bytes: "
                << update_state.downloaded_bytes
                << ", total: " << update_state.total_bytes << std::endl;
      break;

    case UpdateService::UpdateState::State::kInstalling:
      std::cout << Quoted(update_state.app_id)
                << ": installing update, progress at: "
                << update_state.install_progress << std::endl;
      break;

    case UpdateService::UpdateState::State::kUpdated:
      std::cout << Quoted(update_state.app_id)
                << ": updated version = " << update_state.next_version
                << std::endl;
      break;

    case UpdateService::UpdateState::State::kNoUpdate:
      std::cout << Quoted(update_state.app_id) << ": is up-to-date."
                << std::endl;
      break;

    case UpdateService::UpdateState::State::kUpdateError:
      std::cout << Quoted(update_state.app_id) << ": update failed"
                << ", error code: " << update_state.error_code
                << ", extra code: " << update_state.extra_code1 << std::endl;
      break;

    default:
      std::cout << Quoted(update_state.app_id)
                << ": unexpected update state: " << update_state.state
                << std::endl;
      break;
  }
}

void OnUpdateComplete(base::OnceCallback<void(int)> cb,
                      UpdateService::Result result) {
  if (result == UpdateService::Result::kSuccess) {
    std::cout << "App update finished successfully." << std::endl;
    std::move(cb).Run(0);
  } else {
    std::cout << "Failed to update app(s), result = " << result << std::endl;
    std::move(cb).Run(1);
  }
}

class AppState : public base::RefCountedThreadSafe<AppState> {
 public:
  AppState(const std::string& app_id, const std::string& version)
      : app_id_(app_id), current_version_(version) {}

  std::string app_id() const { return app_id_; }
  std::string current_version() const { return current_version_; }
  std::string next_version() const { return next_version_; }
  void set_next_version(const std::string& next_version) {
    next_version_ = next_version;
  }

 protected:
  virtual ~AppState() = default;

 private:
  friend class base::RefCountedThreadSafe<AppState>;

  const std::string app_id_;
  const std::string current_version_;
  std::string next_version_;
};

class UpdaterUtilApp : public App {
 public:
  UpdaterUtilApp() : service_proxy_(CreateUpdateServiceProxy(Scope())) {}

 private:
  ~UpdaterUtilApp() override = default;
  void FirstTaskRun() override;

  void PrintUsage(const std::string& error_message);
  void ListApps();
  void ListUpdate();
  void Update();
  void ListPolicies();
  void ListCBCMPolicies();

  void FindApp(const std::string& app_id,
               base::OnceCallback<void(scoped_refptr<AppState>)> callback);
  void DoListUpdate(scoped_refptr<AppState> app_state);
  void DoUpdateApp(scoped_refptr<AppState> app_state);

  scoped_refptr<UpdateService> service_proxy_;
  ScopedIPCSupportWrapper ipc_support_;
};

void UpdaterUtilApp::PrintUsage(const std::string& error_message) {
  if (!error_message.empty()) {
    LOG(ERROR) << error_message;
  }

  std::cout << "Usage: "
            << base::CommandLine::ForCurrentProcess()->GetProgram().BaseName()
            << " [action...] [parameters...]" << R"(
    Actions:
        --update              Update app(s).
        --list-apps           List all registered apps.
        --list-update         List update for an app (skip update install).
        --list-policies       List all currently effective enterprise policies.
        --list-cbcm-policies  List downloaded CBCM policies.
    Action parameters:
        --background          Use background priority.
        --product             ProductID.
        --system              Use the system scope.
        --policy-path         Location of the CBCM policy root path.
        --json                Use JSON as output format where applicable.)"
            << std::endl;
  Shutdown(error_message.empty() ? 0 : 1);
}

void UpdaterUtilApp::ListApps() {
  service_proxy_->GetAppStates(base::BindOnce(
      [](base::OnceCallback<void(int)> cb,
         const std::vector<updater::UpdateService::AppState>& states) {
        if (OutputInJSONFormat()) {
          base::Value::Dict apps;
          for (updater::UpdateService::AppState app : states) {
            apps.Set(app.app_id, base::Value::Dict().Set(
                                     "version", app.version.GetString()));
          }
          std::cout << ValueToJSONString(base::Value(std::move(apps)))
                    << std::endl;
        } else {
          std::cout << "Registered apps : {" << std::endl;
          for (updater::UpdateService::AppState app : states) {
            std::cout << "\t" << Quoted(app.app_id) << " = "
                      << Quoted(app.version.GetString()) << ';' << std::endl;
          }
          std::cout << '}' << std::endl;
        }
        std::move(cb).Run(0);
      },
      base::BindOnce(&UpdaterUtilApp::Shutdown, this)));
}

void UpdaterUtilApp::FindApp(
    const std::string& app_id,
    base::OnceCallback<void(scoped_refptr<AppState>)> callback) {
  service_proxy_->GetAppStates(base::BindOnce(
      [](const std::string& app_id,
         base::OnceCallback<void(scoped_refptr<AppState>)> callback,
         const std::vector<updater::UpdateService::AppState>& states) {
        auto it = base::ranges::find_if(
            states, [&app_id](const updater::UpdateService::AppState& state) {
              return base::EqualsCaseInsensitiveASCII(state.app_id, app_id);
            });
        LOG_IF(ERROR, it == std::end(states))
            << Quoted(app_id) << " is not a registered app.";
        std::move(callback).Run(it == std::end(states)
                                    ? nullptr
                                    : base::MakeRefCounted<AppState>(
                                          app_id, it->version.GetString()));
      },
      app_id, std::move(callback)));
}

void UpdaterUtilApp::ListUpdate() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  const std::string app_id = command_line->GetSwitchValueASCII(kProductSwitch);
  if (app_id.empty()) {
    PrintUsage("Must specify a product to list update.");
    return;
  }

  FindApp(app_id, base::BindOnce(&UpdaterUtilApp::DoListUpdate, this));
}

void UpdaterUtilApp::DoListUpdate(scoped_refptr<AppState> app_state) {
  if (!app_state) {
    Shutdown(1);
    return;
  }

  service_proxy_->CheckForUpdate(
      app_state->app_id(), Priority(),
      UpdateService::PolicySameVersionUpdate::kNotAllowed,
      base::BindRepeating(
          [](scoped_refptr<AppState> app_state,
             const UpdateService::UpdateState& update_state) {
            if (update_state.state ==
                UpdateService::UpdateState::State::kUpdateAvailable) {
              app_state->set_next_version(
                  update_state.next_version.GetString());
            }
          },
          app_state),
      base::BindOnce(
          [](scoped_refptr<AppState> app_state,
             base::OnceCallback<void(int)> cb, UpdateService::Result result) {
            if (result == UpdateService::Result::kSuccess) {
              if (OutputInJSONFormat()) {
                base::Value::Dict app;
                app.Set(app_state->app_id(),
                        base::Value::Dict()
                            .Set("CurrentVersion", app_state->current_version())
                            .Set("NextVersion", app_state->next_version()));
                std::cout << ValueToJSONString(base::Value(std::move(app)))
                          << std::endl;
              } else {
                std::cout << Quoted(app_state->app_id()) << " : {" << std::endl
                          << "\tCurrent Version = "
                          << Quoted(app_state->current_version()) << ";"
                          << std::endl
                          << "\tNext Version = "
                          << Quoted(app_state->next_version()) << ";"
                          << std::endl
                          << "}" << std::endl;
              }
              std::move(cb).Run(0);
            }
          },
          app_state, base::BindOnce(&UpdaterUtilApp::Shutdown, this)));
}

void UpdaterUtilApp::Update() {
  const std::string app_id =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          kProductSwitch);
  if (app_id.empty()) {
    service_proxy_->UpdateAll(
        base::BindRepeating(OnAppStateChanged),
        base::BindOnce(
            [](base::OnceCallback<void(int)> cb, UpdateService::Result result) {
              OnUpdateComplete(std::move(cb), result);
            },
            base::BindOnce(&UpdaterUtilApp::Shutdown, this)));
  } else {
    FindApp(app_id, base::BindOnce(&UpdaterUtilApp::DoUpdateApp, this));
  }
}

void UpdaterUtilApp::DoUpdateApp(scoped_refptr<AppState> app_state) {
  if (!app_state) {
    Shutdown(1);
    return;
  }

  service_proxy_->Update(
      app_state->app_id(), /*install_data_index=*/"", Priority(),
      UpdateService::PolicySameVersionUpdate::kNotAllowed,
      base::BindRepeating(OnAppStateChanged),
      base::BindOnce(
          [](base::OnceCallback<void(int)> cb, UpdateService::Result result) {
            OnUpdateComplete(std::move(cb), result);
          },
          base::BindOnce(&UpdaterUtilApp::Shutdown, this)));
}

void UpdaterUtilApp::ListPolicies() {
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::MayBlock(), base::WithBaseSyncPrimitives()},
      base::BindOnce([] {
        auto configurator = base::MakeRefCounted<Configurator>(
            CreateGlobalPrefs(Scope()), CreateDefaultExternalConstants());
        if (OutputInJSONFormat()) {
          std::cout << ValueToJSONString(
                           configurator->GetPolicyService()->GetAllPolicies())
                    << std::endl;
        } else {
          std::cout
              << "Updater policies: "
              << configurator->GetPolicyService()->GetAllPoliciesAsString()
              << std::endl;
        }
      }),
      base::BindOnce(&UpdaterUtilApp::Shutdown, this, 0));
}

void UpdaterUtilApp::ListCBCMPolicies() {
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::MayBlock(), base::WithBaseSyncPrimitives()},
      base::BindOnce(&updater_policy::PrintCBCMPolicies),
      base::BindOnce(&UpdaterUtilApp::Shutdown, this, 0));
}

void UpdaterUtilApp::FirstTaskRun() {
  const std::map<std::string, void (UpdaterUtilApp::*)()> commands = {
      {kListAppsSwitch, &UpdaterUtilApp::ListApps},
      {kListUpdateSwitch, &UpdaterUtilApp::ListUpdate},
      {kUpdateSwitch, &UpdaterUtilApp::Update},
      {kListPoliciesSwitch, &UpdaterUtilApp::ListPolicies},
      {kListCBCMPoliciesSwitch, &UpdaterUtilApp::ListCBCMPolicies}};

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  for (const auto& [switch_name, func] : commands) {
    if (command_line->HasSwitch(switch_name)) {
      (this->*func)();
      return;
    }
  }

  PrintUsage("");
}

int UpdaterUtilMain(int argc, char** argv) {
  base::AtExitManager exit_manager;
  base::CommandLine::Init(argc, argv);
  updater::InitLogging(Scope());
  InitializeThreadPool("updater-util");
  const base::ScopedClosureRunner shutdown_thread_pool(
      base::BindOnce([] { base::ThreadPoolInstance::Get()->Shutdown(); }));
  base::SingleThreadTaskExecutor main_task_executor(base::MessagePumpType::UI);
  return base::MakeRefCounted<UpdaterUtilApp>()->Run();
}

}  // namespace updater::tools

int main(int argc, char** argv) {
  return updater::tools::UpdaterUtilMain(argc, argv);
}
