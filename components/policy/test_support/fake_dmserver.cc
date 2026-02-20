// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/fake_dmserver.h"

#include <string_view>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/logging/logging_settings.h"
#include "base/notreached.h"
#include "base/scoped_observation.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/bind_post_task.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/types/optional_util.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/policy_constants_mutable.h"
#include "components/policy/proto/cloud_policy.pb.h"
#include "components/policy/test_support/client_storage.h"
#include "components/policy/test_support/embedded_policy_test_server.h"
#include "components/policy/test_support/policy_storage.h"
#include "components/policy/test_support/request_handler_for_policy.h"
#include "components/policy/test_support/test_server_helpers.h"
#include "third_party/re2/src/re2/re2.h"

#define RETURN_IF_FALSE(expr) \
  if (!expr) {                \
    return false;             \
  }

namespace fakedms {

namespace {

namespace em = enterprise_management;

constexpr char kPolicyTypeKey[] = "policy_type";
constexpr char kEntityIdKey[] = "entity_id";
constexpr char kPolicyValueKey[] = "value";
constexpr char kDeviceIdKey[] = "device_id";
constexpr char kDeviceTokenKey[] = "device_token";
constexpr char kMachineNameKey[] = "machine_name";
constexpr char kUsernameKey[] = "username";
constexpr char kStateKeysKey[] = "state_keys";
constexpr char kAllowedPolicyTypesKey[] = "allowed_policy_types";
constexpr char kPoliciesKey[] = "policies";
constexpr char kExternalPoliciesKey[] = "external_policies";
constexpr char kManagedUsersKey[] = "managed_users";
constexpr char kDeviceAffiliationIdsKey[] = "device_affiliation_ids";
constexpr char kUserAffiliationIdsKey[] = "user_affiliation_ids";
constexpr char kDirectoryApiIdKey[] = "directory_api_id";
constexpr char kRequestErrorsKey[] = "request_errors";
constexpr char kRobotApiAuthCodeKey[] = "robot_api_auth_code";
constexpr char kAllowSetDeviceAttributesKey[] = "allow_set_device_attributes";
constexpr char kUseUniversalSigningKeysKey[] = "use_universal_signing_keys";
constexpr char kInitialEnrollmentStateKey[] = "initial_enrollment_state";
constexpr char kManagementDomainKey[] = "management_domain";
constexpr char kInitialEnrollmentModeKey[] = "initial_enrollment_mode";
constexpr char kCurrentKeyIndexKey[] = "current_key_index";
constexpr char kPolicyUserKey[] = "policy_user";

constexpr char kActionKey[] = "action";
constexpr char kReasonsKey[] = "reasons";
constexpr char kActionAllow[] = "allow";
constexpr char kActionBlock[] = "block";
constexpr char kReasonBlockedCategory[] = "blocked_category";
constexpr char kReasonRiskScore[] = "risk_score";

constexpr char kDefaultPolicyBlobFilename[] = "policy.json";
constexpr char kDefaultClientStateFilename[] = "state.json";
constexpr int kDefaultMinLogLevel = logging::LOGGING_INFO;
constexpr bool kDefaultLogToConsole = false;
constexpr int kDefaultPort = 6112;

constexpr char kPolicyBlobPathSwitch[] = "policy-blob-path";
constexpr char kPolicyPathSwitch[] = "policy-path";
constexpr char kClientStatePathSwitch[] = "client-state-path";
constexpr char kGrpcUnixSocketUriSwitch[] = "grpc-unix-socket-uri";
constexpr char kLogPathSwitch[] = "log-path";
constexpr char kStartupPipeSwitch[] = "startup-pipe";
constexpr char kMinLogLevelSwitch[] = "min-log-level";
constexpr char kLogToConsoleSwitch[] = "log-to-console";
constexpr char kPortSwitch[] = "port";

constexpr base::TimeDelta kRemoteCommandTimeoutSeconds = base::Seconds(10);
constexpr int64_t kDefaultServerStopTimeoutMs = 100;

// Maps policy type to its name in the policies.json config file.
struct PolicyTypeEntry {
  const char* policy_type;
  const char* config_key;
};

const PolicyTypeEntry kPolicyTypeMapping[] = {
    // TODO(nicolaso): Implement "google/chrome/device" if ChromeOS folks are
    // interested. It uses ChromeDeviceSettingsProto, which is a completely
    // different schema. Use manual_device_policy_proto_map.yaml for this.
    //
    // {policy::dm_protocol::kChromeDevicePolicyType, "device"},
    {policy::dm_protocol::kChromeMachineLevelUserCloudPolicyType, "machine"},
    {policy::dm_protocol::GetChromeUserPolicyType(), "user"},
};

const PolicyTypeEntry kExtensionInstallPolicyTypeMapping[] = {
    {policy::dm_protocol::kChromeExtensionInstallUserCloudPolicyType,
      "user-extension-install"},
    {policy::dm_protocol::kChromeExtensionInstallMachineLevelCloudPolicyType,
      "machine-extension-install"},
};

static remote_commands::WaitRemoteCommandResultResponse
BuildWaitRemoteCommandResultResponse(const em::RemoteCommandResult& result) {
  remote_commands::WaitRemoteCommandResultResponse resp;
  em::RemoteCommandResult* remote_command_result = resp.mutable_result();
  remote_command_result->set_result(result.result());
  remote_command_result->set_command_id(result.command_id());
  remote_command_result->set_timestamp(result.timestamp());
  remote_command_result->set_payload(result.payload());
  return resp;
}

void ParsePolicyUser(const base::DictValue* dict,
                     policy::PolicyStorage* policy_storage) {
  const std::string* policy_user = dict->FindString(kPolicyUserKey);
  if (policy_user) {
    LOG(INFO) << "Adding " << *policy_user << " as a policy user";
    policy_storage->set_policy_user(*policy_user);
  } else {
    LOG(INFO) << "The policy_user key isn't found and the default policy "
                 "user "
              << policy::kDefaultUsername << " will be used";
  }
}

void ParseManagedUsers(const base::DictValue* dict,
                       policy::PolicyStorage* policy_storage) {
  const base::ListValue* managed_users = dict->FindList(kManagedUsersKey);
  if (managed_users) {
    for (const base::Value& managed_user : *managed_users) {
      const std::string* managed_val = managed_user.GetIfString();
      if (managed_val) {
        LOG(INFO) << "Adding " << *managed_val << " as a managed user";
        policy_storage->add_managed_user(*managed_val);
      }
    }
  } else {
    LOG(INFO) << "The managed_users key isn't found, all users are managed";
    policy_storage->add_managed_user("*");
  }
}

void ParseDeviceAffiliationIds(const base::DictValue* dict,
                               policy::PolicyStorage* policy_storage) {
  const base::ListValue* device_affiliation_ids =
      dict->FindList(kDeviceAffiliationIdsKey);
  if (device_affiliation_ids) {
    for (const base::Value& device_affiliation_id : *device_affiliation_ids) {
      const std::string* device_affiliation_id_val =
          device_affiliation_id.GetIfString();
      if (device_affiliation_id_val) {
        LOG(INFO) << "Adding " << *device_affiliation_id_val
                  << " as a device affiliation id";
        policy_storage->add_device_affiliation_id(*device_affiliation_id_val);
      }
    }
  }
}

void ParseUserAffiliationIds(const base::DictValue* dict,
                             policy::PolicyStorage* policy_storage) {
  const base::ListValue* user_affiliation_ids =
      dict->FindList(kUserAffiliationIdsKey);
  if (user_affiliation_ids) {
    for (const base::Value& user_affiliation_id : *user_affiliation_ids) {
      const std::string* user_affiliation_id_val =
          user_affiliation_id.GetIfString();
      if (user_affiliation_id_val) {
        LOG(INFO) << "Adding " << *user_affiliation_id_val
                  << " as a user affiliation id";
        policy_storage->add_user_affiliation_id(*user_affiliation_id_val);
      }
    }
  }
}

void ParseDirectoryApiId(const base::DictValue* dict,
                         policy::PolicyStorage* policy_storage) {
  const std::string* directory_api_id = dict->FindString(kDirectoryApiIdKey);
  if (directory_api_id) {
    LOG(INFO) << "Adding " << *directory_api_id << " as a directory API ID";
    policy_storage->set_directory_api_id(*directory_api_id);
  }
}

bool ParseAllowSetDeviceAttributes(const base::DictValue* dict,
                                   policy::PolicyStorage* policy_storage) {
  if (const base::Value* v = dict->Find(kAllowSetDeviceAttributesKey); v) {
    std::optional<bool> allow_set_device_attributes = v->GetIfBool();
    if (!allow_set_device_attributes.has_value()) {
      LOG(ERROR)
          << "The allow_set_device_attributes key isn't a bool, found type "
          << v->type() << ", found value " << *v;
      return false;
    }
    policy_storage->set_allow_set_device_attributes(
        allow_set_device_attributes.value());
  }
  return true;
}

bool ParseUseUniversalSigningKeys(const base::DictValue* dict,
                                  policy::PolicyStorage* policy_storage) {
  const base::Value* use_universal_signing_keys =
      dict->Find(kUseUniversalSigningKeysKey);
  if (use_universal_signing_keys) {
    std::optional<bool> maybe_value = use_universal_signing_keys->GetIfBool();
    if (!maybe_value.has_value()) {
      LOG(ERROR)
          << "The use_universal_signing_keys key isn't a bool, found type "
          << use_universal_signing_keys->type() << ", found value "
          << *use_universal_signing_keys;
      return false;
    }
    if (maybe_value.value()) {
      policy_storage->signature_provider()->SetUniversalSigningKeys();
    }
  }
  return true;
}

void ParseRobotApiAuthCode(const base::DictValue* dict,
                           policy::PolicyStorage* policy_storage) {
  const std::string* robot_api_auth_code =
      dict->FindString(kRobotApiAuthCodeKey);
  if (robot_api_auth_code) {
    LOG(INFO) << "Adding " << *robot_api_auth_code
              << " as a robot api auth code";
    policy_storage->set_robot_api_auth_code(*robot_api_auth_code);
  }
}

bool ParseRequestErrors(const base::DictValue* dict,
                        FakeDMServer* fake_dmserver) {
  const base::DictValue* request_errors = dict->FindDict(kRequestErrorsKey);
  if (request_errors) {
    for (auto request_error : *request_errors) {
      std::optional<int> net_error_code = request_error.second.GetIfInt();
      if (!net_error_code.has_value()) {
        LOG(ERROR) << "The error code isn't an int";
        return false;
      }
      LOG(INFO) << "Configuring request " << request_error.first << " to error "
                << net_error_code.value();
      fake_dmserver->ConfigureRequestError(
          request_error.first,
          static_cast<net::HttpStatusCode>(net_error_code.value()));
    }
  }
  return true;
}

bool ParseInitialEnrollmentState(const base::DictValue* dict,
                                 policy::PolicyStorage* policy_storage) {
  const base::DictValue* initial_enrollment_state =
      dict->FindDict(kInitialEnrollmentStateKey);
  if (initial_enrollment_state) {
    for (auto state : *initial_enrollment_state) {
      const base::DictValue* state_val = state.second.GetIfDict();
      if (!state_val) {
        LOG(ERROR) << "The current state value for key " << state.first
                   << " isn't a dict";
        return false;
      }
      const std::string* management_domain =
          state_val->FindString(kManagementDomainKey);
      if (!management_domain) {
        LOG(ERROR) << "The management_domain key isn't a string";
        return false;
      }
      std::optional<int> initial_enrollment_mode =
          state_val->FindInt(kInitialEnrollmentModeKey);
      if (!initial_enrollment_mode.has_value()) {
        LOG(ERROR) << "The initial_enrollment_mode key isn't an int";
        return false;
      }
      policy::PolicyStorage::InitialEnrollmentState initial_value;
      initial_value.management_domain = *management_domain;
      initial_value.initial_enrollment_mode = static_cast<
          enterprise_management::DeviceInitialEnrollmentStateResponse::
              InitialEnrollmentMode>(initial_enrollment_mode.value());
      policy_storage->SetInitialEnrollmentState(state.first, initial_value);
    }
  }
  return true;
}

bool ParseCurrentKeyIndex(const base::DictValue* dict,
                          policy::PolicyStorage* policy_storage) {
  if (const base::Value* v = dict->Find(kCurrentKeyIndexKey); v) {
    std::optional<int> current_key_index = v->GetIfInt();
    if (!current_key_index.has_value()) {
      LOG(ERROR) << "The current_key_index key isn't an int, found type "
                 << v->type() << ", found value " << *v;
      return false;
    }
    policy_storage->signature_provider()->set_current_key_version(
        current_key_index.value());
  }
  return true;
}

// Used to print a human-readable type name in warnings in TrySetCloudPolicySettings.
template <typename T>
const char* GetExpectedTypeName() {
  if constexpr (std::is_same_v<T, bool>) {
    return "boolean";
  } else if constexpr (std::is_same_v<T, int>) {
    return "integer";
  } else if constexpr (std::is_same_v<T, std::string>) {
    return "string";
  } else {
    static_assert(false, "Unsupported type");
  }
}

// To avoid duplicating the same code 3 times for boolean, integers, and
// strings. This function tries to convert the simple `value` to a proto for
// type `T`, and setting it in `policy_settings`.
template <typename Access, typename T, typename UnwrapMethod>
bool TrySetCloudPolicySettings(const Access& access,
                               const base::Value& value,
                               UnwrapMethod unwrap,
                               em::CloudPolicySettings& policy_settings) {
  auto unwrapped = std::invoke(unwrap, value);
  if (!unwrapped) {
    LOG(WARNING) << "Wrong policy type for '" << access.policy_key
                 << "'. Expected " << GetExpectedTypeName<T>() << ", got "
                 << value.type();
    return false;
  }
  auto* proto = access.get_proto_mutable(policy_settings);
  CHECK(proto);
  proto->mutable_policy_options()->set_mode(em::PolicyOptions::MANDATORY);
  proto->set_value(*unwrapped);
  return true;
}

// Checks that `value` is the right type for `policy_name`, and sets it in
// `policy_settings` if it is.
bool ValidateAndSetPolicyValue(std::string_view policy_name,
                               const base::Value& value,
                               em::CloudPolicySettings& policy_settings) {
  for (const auto& access : policy::test::kBooleanPolicyAccess) {
    if (policy_name == access.policy_key) {
      return TrySetCloudPolicySettings<decltype(access), bool>(
          access, value, &base::Value::GetIfBool, policy_settings);
    }
  }

  for (const auto& access : policy::test::kIntegerPolicyAccess) {
    if (policy_name == access.policy_key) {
      return TrySetCloudPolicySettings<decltype(access), int>(
          access, value, &base::Value::GetIfInt, policy_settings);
    }
  }

  for (const auto& access : policy::test::kStringPolicyAccess) {
    if (policy_name != access.policy_key) {
      continue;
    }
    switch (access.type) {
      case policy::test::StringPolicyType::STRING:
        // Strings are encoded as protos with a single string field.
        return TrySetCloudPolicySettings<decltype(access), std::string>(
            access, value,
            static_cast<const std::string* (base::Value::*)() const>(
                &base::Value::GetIfString),
            policy_settings);

      case policy::test::StringPolicyType::JSON: {
        // JSON values are converted to JSON, then encoded the same as
        // strings.
        if (!value.is_dict()) {
          LOG(WARNING) << "Failed to set policy " << policy_name
                       << " with type " << value.type();
          return false;
        }
        std::string json;
        CHECK(base::JSONWriter::Write(value, &json));
        auto* proto = access.get_proto_mutable(policy_settings);
        CHECK(proto);
        proto->set_value(json);
        return true;
      }

      case policy::test::StringPolicyType::EXTERNAL:
        // TODO(nicolaso): These are ChromeOS-only. Implement if there's
        // demand.
        LOG(ERROR) << "External policies NYI, skipping policy '" << policy_name
                   << "'";
        return false;
    }
  }

  LOG(WARNING) << "Unknown policy name: '" << policy_name << "', skipping.";
  return false;
}

// Checks that `value` is a valid ExtensionInstallPolicy proto, and returns a
// serialized ExtensionInstallPolicy proto if it is.
std::optional<std::string> ValidateAndSerializeExtensionInstallPolicyValue(
    std::string_view extension_id_and_version,
    const base::DictValue& value) {
  // Should look like "abcdefghijklmnopabcdefghijklmnop@1.0.0".
  static constexpr char kExtensionIdAndVersionRegex[] =
      "[a-p]{32}@([0-9]+(\\.[0-9]+)*)";
  if (!RE2::FullMatch(extension_id_and_version, kExtensionIdAndVersionRegex)) {
    LOG(ERROR) << "Policy for '" << extension_id_and_version
               << "' has invalid extension id and version, skipping.";
    return std::nullopt;
  }

  if (!value.FindString(kActionKey)) {
    LOG(ERROR) << "Policy for '" << extension_id_and_version
               << "' is missing the '" << kActionKey << "' field, skipping.";
    return std::nullopt;
  }
  if (!value.FindList(kReasonsKey)) {
    LOG(ERROR) << "Policy for '" << extension_id_and_version
               << "' is missing the '" << kReasonsKey << "' field, skipping.";
    return std::nullopt;
  }

  em::ExtensionInstallPolicies proto;
  em::ExtensionInstallPolicy* policy = proto.add_policies();
  policy->set_extension_id(extension_id_and_version.substr(0, 32));
  policy->set_extension_version(extension_id_and_version.substr(33));

  // Parse action.
  const std::string& action = *value.FindString(kActionKey);
  if (action == kActionAllow) {
    policy->set_action(em::ExtensionInstallPolicy::ACTION_ALLOW);
  } else if (action == kActionBlock) {
    policy->set_action(em::ExtensionInstallPolicy::ACTION_BLOCK);
  } else {
    LOG(ERROR) << "Policy for '" << extension_id_and_version
               << "' has invalid action: " << action << ", skipping.";
    return std::nullopt;
  }

  // Parse reasons.
  const base::ListValue& reasons = *value.FindList(kReasonsKey);
  for (const auto& reason : reasons) {
    if (!reason.is_string()) {
      LOG(ERROR) << "Policy for '" << extension_id_and_version
                 << "' has invalid reason type: " << reason << ", skipping.";
      return std::nullopt;
    }
    const std::string& reason_str = reason.GetString();
    if (reason_str == kReasonBlockedCategory) {
      policy->add_reasons(em::ExtensionInstallPolicy::REASON_BLOCKED_CATEGORY);
    } else if (reason_str == kReasonRiskScore) {
      policy->add_reasons(em::ExtensionInstallPolicy::REASON_RISK_SCORE);
    } else {
      LOG(ERROR) << "Policy for '" << extension_id_and_version
                 << "' has invalid reason, skipping.";
      return std::nullopt;
    }
  }

  std::string serialized_proto;
  CHECK(proto.SerializeToString(&serialized_proto));
  return serialized_proto;
}

}  // namespace

void InitLogging(const std::optional<std::string>& log_path,
                 bool log_to_console,
                 int min_log_level) {
  logging::LoggingSettings settings;
  if (log_path.has_value()) {
    settings.log_file_path = log_path.value().c_str();
    settings.logging_dest = logging::LOG_TO_FILE;
  } else {
    settings.logging_dest = logging::LOG_TO_STDERR;
  }
  // If log_to_console exists then log to everything.
  if (log_to_console) {
    settings.logging_dest |=
        logging::LOG_TO_SYSTEM_DEBUG_LOG | logging::LOG_TO_STDERR;
  }
  logging::SetMinLogLevel(min_log_level);
  logging::InitLogging(settings);
  logging::SetLogItems(/*enable_process_id=*/true, /*enable_thread_id=*/true,
                       /*enable_timestamp=*/true, /*enable_timestamp=*/false);
}

void ParseFlags(const base::CommandLine& command_line,
                std::string& policy_blob_path,
                std::string& client_state_path,
                std::string& grpc_unix_socket_uri,
                std::optional<std::string>& log_path,
                base::ScopedFD& startup_pipe,
                bool& log_to_console,
                int& min_log_level,
                int& port) {
  policy_blob_path = kDefaultPolicyBlobFilename;
  client_state_path = kDefaultClientStateFilename;
  log_to_console = kDefaultLogToConsole;
  min_log_level = kDefaultMinLogLevel;
  port = kDefaultPort;

  // kPolicyPathSwitch is an alias for kPolicyBlobPathSwitch.
  if (command_line.HasSwitch(kPolicyBlobPathSwitch) ||
      command_line.HasSwitch(kPolicyPathSwitch)) {
    policy_blob_path = command_line.GetSwitchValueASCII(kPolicyBlobPathSwitch);
    if (policy_blob_path.empty()) {
      policy_blob_path = command_line.GetSwitchValueASCII(kPolicyPathSwitch);
    }
    // If not specified, set client_state_path to the same directory as the
    // policy blob.
    client_state_path = base::FilePath(policy_blob_path)
                            .DirName()
                            .Append(kDefaultClientStateFilename)
                            .AsUTF8Unsafe();
  }

  if (command_line.HasSwitch(kLogPathSwitch)) {
    log_path = command_line.GetSwitchValueASCII(kLogPathSwitch);
  }

  if (command_line.HasSwitch(kClientStatePathSwitch)) {
    client_state_path =
        command_line.GetSwitchValueASCII(kClientStatePathSwitch);
  }

  if (command_line.HasSwitch(kGrpcUnixSocketUriSwitch)) {
    grpc_unix_socket_uri =
        command_line.GetSwitchValueASCII(kGrpcUnixSocketUriSwitch);
  }

  if (command_line.HasSwitch(kStartupPipeSwitch)) {
    std::string pipe_str = command_line.GetSwitchValueASCII(kStartupPipeSwitch);
    int pipe_val;
    CHECK(base::StringToInt(pipe_str, &pipe_val))
        << "Expected an int value for --startup-pipe switch, but got: "
        << pipe_str;
    startup_pipe = base::ScopedFD(pipe_val);
  }

  if (command_line.HasSwitch(kMinLogLevelSwitch)) {
    std::string log_str = command_line.GetSwitchValueASCII(kMinLogLevelSwitch);
    CHECK(base::StringToInt(log_str, &min_log_level))
        << "Expected an int value for --min-log-level switch, but got: "
        << log_str;
  }

  if (command_line.HasSwitch(kLogToConsoleSwitch)) {
    log_to_console = true;
  }

  if (command_line.HasSwitch(kPortSwitch)) {
    std::string port_str = command_line.GetSwitchValueASCII(kPortSwitch);
    CHECK(base::StringToInt(port_str, &port))
        << "Expected an int value for --port switch, but got: " << port_str;
  }
}

enum class RemoteCommandsWaitType { kAcknowledged, kResultAvailable };

class RemoteCommandsWaitOperation
    : public policy::RemoteCommandsState::Observer {
 public:
  // Callback for a RemoteCommandsWaitOperation.
  // `wait_operation` will refer to the RemoteCommandsWaitOperation object that
  // invoked the callback. If `success` is true, the `RemoteCommandsWaitType`
  // has happened, otherwise the wait timed out.
  using RemoteCommandsWaitCallback =
      base::OnceCallback<void(RemoteCommandsWaitOperation* wait_operation,
                              bool success)>;

  RemoteCommandsWaitOperation(
      policy::RemoteCommandsState* remote_commands_state,
      RemoteCommandsWaitType wait_type,
      RemoteCommandsWaitOperation::RemoteCommandsWaitCallback wait_callback);

  ~RemoteCommandsWaitOperation() override;

  void OnRemoteCommandResultAvailable(int64_t command_id) override;
  void OnRemoteCommandAcked(int64_t command_id) override;
  void OnTimeout();

 private:
  const raw_ptr<policy::RemoteCommandsState> remote_commands_state_;
  const RemoteCommandsWaitType wait_type_;
  RemoteCommandsWaitCallback wait_callback_;
  base::ScopedObservation<policy::RemoteCommandsState,
                          policy::RemoteCommandsState::Observer>
      state_observation_{this};
  // Timer that fires to prevent indefinite wait if the remote command result
  // takes too long.
  base::OneShotTimer result_timeout_timer_;
  base::WeakPtrFactory<RemoteCommandsWaitOperation> weak_ptr_factory_{this};
};

RemoteCommandsWaitOperation::RemoteCommandsWaitOperation(
    policy::RemoteCommandsState* remote_commands_state,
    RemoteCommandsWaitType wait_type,
    RemoteCommandsWaitOperation::RemoteCommandsWaitCallback wait_callback)
    : remote_commands_state_(remote_commands_state),
      wait_type_(wait_type),
      wait_callback_(std::move(wait_callback)) {
  state_observation_.Observe(remote_commands_state);
  // Start a timer for 10 seconds to wait for the remote command result.
  result_timeout_timer_.Start(
      FROM_HERE, kRemoteCommandTimeoutSeconds,
      base::BindOnce(&RemoteCommandsWaitOperation::OnTimeout,
                     weak_ptr_factory_.GetWeakPtr()));
}

RemoteCommandsWaitOperation::~RemoteCommandsWaitOperation() = default;

void RemoteCommandsWaitOperation::OnRemoteCommandResultAvailable(
    int64_t command_id) {
  if (wait_type_ != RemoteCommandsWaitType::kResultAvailable) {
    return;
  }
  const bool result_available =
      remote_commands_state_->IsRemoteCommandResultAvailable(command_id);
  // The result must be available now.
  CHECK(result_available);
  // Invoke the wait callback OnWaitRemoteCommandResultDone to write the result
  // to the reactor.
  std::move(wait_callback_).Run(this, result_available);
}

void RemoteCommandsWaitOperation::OnRemoteCommandAcked(int64_t command_id) {
  if (wait_type_ != RemoteCommandsWaitType::kAcknowledged) {
    return;
  }
  const bool command_acked =
      remote_commands_state_->IsRemoteCommandAcked(command_id);
  // The command must be acknowledged now.
  CHECK(command_acked);
  // Invoke the wait callback OnWaitRemoteCommandAckDone to write the ack to the
  // reactor.
  std::move(wait_callback_).Run(this, command_acked);
}

void RemoteCommandsWaitOperation::OnTimeout() {
  std::move(wait_callback_).Run(this, false);
}

FakeDMServer::FakeDMServer(const std::string& policy_blob_path,
                           const std::string& client_state_path,
                           const std::string& grpc_unix_socket_uri,
                           base::OnceClosure shutdown_cb)
    : policy_blob_path_(policy_blob_path),
      client_state_path_(client_state_path),
      grpc_unix_socket_uri_(grpc_unix_socket_uri) {
  shut_down_on_main_task_runner_ =
      base::BindPostTaskToCurrentDefault(base::BindOnce(
          &FakeDMServer::TriggerShutdown, weak_ptr_factory_.GetWeakPtr()));
  shut_down_server_ = base::BindPostTaskToCurrentDefault(
      base::BindOnce(&FakeDMServer::OnShutdownGrpcServerDone,
                     weak_ptr_factory_.GetWeakPtr(), std::move(shutdown_cb)));
  DETACH_FROM_SEQUENCE(embedded_server_sequence_checker_);
}

FakeDMServer::~FakeDMServer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(fake_dmserver_main_sequence_checker_);
}

void FakeDMServer::EraseWaitOperation(RemoteCommandsWaitOperation* operation) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(fake_dmserver_main_sequence_checker_);
  auto it = waiters_.find(operation);
  CHECK(it != waiters_.end());
  waiters_.erase(it);
}

void FakeDMServer::StartGrpcServer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(fake_dmserver_main_sequence_checker_);
  LOG(INFO) << "Starting the gRPC server on endpoint " << grpc_unix_socket_uri_;
  grpc_server_.emplace();
  grpc_server_->SetHandler<
      remote_commands::RemoteCommandsServiceHandler::SendRemoteCommand>(
      base::BindPostTask(
          base::SingleThreadTaskRunner::GetCurrentDefault(),
          base::BindRepeating(&FakeDMServer::HandleSendRemoteCommand,
                              weak_ptr_factory_.GetWeakPtr())));
  grpc_server_->SetHandler<
      remote_commands::RemoteCommandsServiceHandler::WaitRemoteCommandResult>(
      base::BindPostTask(
          base::SingleThreadTaskRunner::GetCurrentDefault(),
          base::BindRepeating(&FakeDMServer::HandleWaitRemoteCommandResult,
                              weak_ptr_factory_.GetWeakPtr())));
  grpc_server_->SetHandler<
      remote_commands::RemoteCommandsServiceHandler::WaitRemoteCommandAcked>(
      base::BindPostTask(
          base::SingleThreadTaskRunner::GetCurrentDefault(),
          base::BindRepeating(&FakeDMServer::HandleWaitRemoteCommandAcked,
                              weak_ptr_factory_.GetWeakPtr())));
  auto status = grpc_server_->Start(grpc_unix_socket_uri_);
  // Browser runtime must crash if the runtime service failed to start to avoid
  // the process to dangle without any proper connection to the Cast Core.
  CHECK(status.ok()) << "Failed to start DM gRPC server: status="
                     << status.error_message();
}

void FakeDMServer::HandleSendRemoteCommand(
    remote_commands::SendRemoteCommandRequest request,
    remote_commands::RemoteCommandsServiceHandler::SendRemoteCommand::Reactor*
        reactor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(fake_dmserver_main_sequence_checker_);
  LOG(INFO) << "Processing SendRemoteCommand grpc request.";
  int64_t command_id = remote_commands_state()->AddPendingRemoteCommand(
      request.remote_command());
  remote_commands::SendRemoteCommandResponse resp;
  resp.set_command_id(command_id);
  reactor->Write(std::move(resp));
}

void FakeDMServer::OnWaitRemoteCommandResultDone(
    remote_commands::RemoteCommandsServiceHandler::WaitRemoteCommandResult::
        Reactor* reactor,
    int64_t command_id,
    RemoteCommandsWaitOperation* wait_operation,
    bool wait_success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(fake_dmserver_main_sequence_checker_);
  auto it = waiters_.find(wait_operation);
  CHECK(it != waiters_.end());
  waiters_.erase(it);

  if (!wait_success) {
    reactor->Write(grpc::Status(
        grpc::StatusCode::CANCELLED,
        "Timeout waiting for remote command result took more than 10 seconds"));
    return;
  }
  em::RemoteCommandResult result;
  bool result_available =
      remote_commands_state()->GetRemoteCommandResult(command_id, &result);
  CHECK(result_available);
  auto resp = BuildWaitRemoteCommandResultResponse(result);
  reactor->Write(std::move(resp));
}

void FakeDMServer::HandleWaitRemoteCommandResult(
    remote_commands::WaitRemoteCommandResultRequest request,
    remote_commands::RemoteCommandsServiceHandler::WaitRemoteCommandResult::
        Reactor* reactor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(fake_dmserver_main_sequence_checker_);
  LOG(INFO) << "Processing WaitRemoteCommandResult grpc request.";
  int64_t command_id = request.command_id();
  em::RemoteCommandResult result;
  bool result_available =
      remote_commands_state()->GetRemoteCommandResult(command_id, &result);
  if (!result_available) {
    LOG(INFO) << "Remote command result isn't available yet.";
    // Insert the wait operation into the set and bind the erase function to
    // erase it if the result is available.
    waiters_.insert(std::make_unique<RemoteCommandsWaitOperation>(
        remote_commands_state(), RemoteCommandsWaitType::kResultAvailable,
        base::BindOnce(&FakeDMServer::OnWaitRemoteCommandResultDone,
                       weak_ptr_factory_.GetWeakPtr(),
                       base::Unretained(reactor), command_id)));
    return;
  }
  LOG(INFO) << "Remote command result is available. Resolving the grpc call.";
  auto resp = BuildWaitRemoteCommandResultResponse(result);
  reactor->Write(std::move(resp));
}

void FakeDMServer::OnWaitRemoteCommandAckDone(
    remote_commands::RemoteCommandsServiceHandler::WaitRemoteCommandAcked::
        Reactor* reactor,
    int64_t command_id,
    RemoteCommandsWaitOperation* wait_operation,
    bool wait_success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(fake_dmserver_main_sequence_checker_);
  auto it = waiters_.find(wait_operation);
  CHECK(it != waiters_.end());
  waiters_.erase(it);

  if (!wait_success) {
    reactor->Write(grpc::Status(grpc::StatusCode::CANCELLED,
                                "Timeout waiting for remote command "
                                "acknowledgement took more than 10 seconds"));
    return;
  }
  bool command_acked =
      remote_commands_state()->IsRemoteCommandAcked(command_id);
  CHECK(command_acked);
  remote_commands::WaitRemoteCommandAckedResponse resp;
  reactor->Write(std::move(resp));
}

void FakeDMServer::HandleWaitRemoteCommandAcked(
    remote_commands::WaitRemoteCommandAckedRequest request,
    remote_commands::RemoteCommandsServiceHandler::WaitRemoteCommandAcked::
        Reactor* reactor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(fake_dmserver_main_sequence_checker_);
  LOG(INFO) << "Processing WaitRemoteCommandAcked grpc request.";
  int64_t command_id = request.command_id();
  bool command_acked =
      remote_commands_state()->IsRemoteCommandAcked(command_id);
  if (!command_acked) {
    LOG(INFO) << "Remote command isn't acknowledged yet.";
    // Insert the wait operation into the set and bind the erase function to
    // erase it if the command is acknowledged.
    waiters_.insert(std::make_unique<RemoteCommandsWaitOperation>(
        remote_commands_state(), RemoteCommandsWaitType::kAcknowledged,
        base::BindOnce(&FakeDMServer::OnWaitRemoteCommandAckDone,
                       weak_ptr_factory_.GetWeakPtr(),
                       base::Unretained(reactor), command_id)));
    return;
  }
  LOG(INFO) << "Remote command is acknowledged. Resolving the grpc call.";
  remote_commands::WaitRemoteCommandAckedResponse resp;
  reactor->Write(std::move(resp));
}

bool FakeDMServer::StartFakeServer(int port) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(fake_dmserver_main_sequence_checker_);
  LOG(INFO) << "Starting the FakeDMServer with args policy_blob_path="
            << policy_blob_path_ << " client_state_path=" << client_state_path_
            << " grpc_unix_socket_uri=" << grpc_unix_socket_uri_;

  if (!policy::EmbeddedPolicyTestServer::Start(port)) {
    LOG(ERROR) << "Failed to start the EmbeddedPolicyTestServer";
    return false;
  }
  LOG(INFO) << "Server started running on URL: "
            << EmbeddedPolicyTestServer::GetServiceURL();
  if (grpc_unix_socket_uri_.empty()) {
    LOG(INFO) << "grpc_unix_socket_uri is empty the grpc server won't start";
    return true;
  }
  StartGrpcServer();
  return true;
}

void FakeDMServer::ShutdownGrpcServer(
    base::OnceClosure server_stopped_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(fake_dmserver_main_sequence_checker_);
  CHECK(grpc_server_);
  grpc_server_->Stop(kDefaultServerStopTimeoutMs,
                     std::move(server_stopped_callback));
}

void FakeDMServer::OnShutdownGrpcServerDone(
    base::OnceClosure server_stopped_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(fake_dmserver_main_sequence_checker_);
  grpc_server_.reset();
  std::move(server_stopped_callback).Run();
}

void FakeDMServer::TriggerShutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(fake_dmserver_main_sequence_checker_);
  if (!grpc_server_) {
    return std::move(shut_down_server_).Run();
  }
  ShutdownGrpcServer(std::move(shut_down_server_));
}

bool FakeDMServer::WriteURLToPipe(base::ScopedFD&& startup_pipe) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(fake_dmserver_main_sequence_checker_);
  GURL server_url = EmbeddedPolicyTestServer::GetServiceURL();
  std::string server_data = base::StringPrintf(
      "{\"host\": \"%s\", \"port\": %s}", server_url.GetHost().c_str(),
      server_url.GetPort().c_str());

  base::File pipe_writer(startup_pipe.release());
  if (!pipe_writer.WriteAtCurrentPosAndCheck(base::as_byte_span(server_data))) {
    LOG(ERROR) << "Failed to write the server url data to the pipe, data: "
               << server_data;
    return false;
  }
  return true;
}

std::unique_ptr<net::test_server::HttpResponse> FakeDMServer::HandleRequest(
    const net::test_server::HttpRequest& request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(embedded_server_sequence_checker_);
  GURL url = request.GetURL();
  if (url.GetPath() == "/test/exit") {
    LOG(INFO) << "Stopping the FakeDMServer";
    std::move(shut_down_on_main_task_runner_).Run();
    return policy::CreateHttpResponse(net::HTTP_OK, "Policy Server exited.");
  }

  if (url.GetPath() == "/test/ping") {
    return policy::CreateHttpResponse(net::HTTP_OK, "Pong.");
  }

  EmbeddedPolicyTestServer::ResetServerState();

  if (!ReadPolicyBlobFile()) {
    return policy::CreateHttpResponse(net::HTTP_INTERNAL_SERVER_ERROR,
                                      "Failed to read policy blob file.");
  }

  if (!ReadClientStateFile()) {
    return policy::CreateHttpResponse(net::HTTP_INTERNAL_SERVER_ERROR,
                                      "Failed to read client state file.");
  }
  auto resp = policy::EmbeddedPolicyTestServer::HandleRequest(request);
  if (!WriteClientStateFile()) {
    return policy::CreateHttpResponse(net::HTTP_INTERNAL_SERVER_ERROR,
                                      "Failed to write client state file.");
  }
  return resp;
}

bool FakeDMServer::SetPolicyPayload(const std::string* policy_type,
                                    const std::string* entity_id,
                                    const std::string* serialized_proto) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(embedded_server_sequence_checker_);
  if (!policy_type || !serialized_proto) {
    LOG(ERROR) << "Couldn't find the policy type or value fields";
    return false;
  }
  std::string decoded_proto;
  if (!base::Base64Decode(*serialized_proto, &decoded_proto)) {
    LOG(ERROR) << "Unable to base64 decode validation value from "
               << *serialized_proto;
    return false;
  }
  if (entity_id) {
    policy_storage()->SetPolicyPayload(*policy_type, *entity_id, decoded_proto);
  } else {
    policy_storage()->SetPolicyPayload(*policy_type, decoded_proto);
  }
  return true;
}

bool FakeDMServer::SetExternalPolicyPayload(
    const std::string* policy_type,
    const std::string* entity_id,
    const std::string* serialized_raw_policy) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(embedded_server_sequence_checker_);
  if (!policy_type || !entity_id || !serialized_raw_policy) {
    LOG(ERROR) << "Couldn't find the policy type or entity id or value fields";
    return false;
  }
  std::string decoded_raw_policy;
  if (!base::Base64Decode(*serialized_raw_policy, &decoded_raw_policy)) {
    LOG(ERROR) << "Unable to base64 decode validation value from "
               << *serialized_raw_policy;
    return false;
  }
  EmbeddedPolicyTestServer::UpdateExternalPolicy(*policy_type, *entity_id,
                                                 decoded_raw_policy);
  return true;
}

bool FakeDMServer::ParsePoliciesJson(const base::DictValue* dict) {
  // Normal policies look like e.g.:
  // "user": {
  //   "AllowDinosaurEasterEgg": true,
  //   "HomepageLocation": "http://example.com/"
  // }
  em::CloudPolicySettings cloud_policy_settings;
  for (const auto& entry : kPolicyTypeMapping) {
    const base::Value* policies = dict->Find(entry.config_key);
    if (!policies) {
      continue;
    }
    if (!policies->is_dict()) {
      LOG(WARNING) << "Policy for '" << entry.config_key << "' is not a dict.";
      return false;
    }
    bool any_policy_set_for_type = false;
    for (const auto [policy_name, value] : policies->GetDict()) {
      if (!ValidateAndSetPolicyValue(policy_name, value,
                                     cloud_policy_settings)) {
        continue;
      }
      any_policy_set_for_type = true;
    }
    if (any_policy_set_for_type) {
      std::string serialized_proto;
      CHECK(cloud_policy_settings.SerializeToString(&serialized_proto));
      policy_storage()->SetPolicyPayload(entry.policy_type, serialized_proto);
    }
  }

  // Extension install policies look like e.g.:
  // "machine-extension-install": {
  //   "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa@1.2.3": {
  //     "action": "allow",
  //     "reasons": ["risk_score"]
  //   }
  // }
  for (const auto& entry : kExtensionInstallPolicyTypeMapping) {
    const base::Value* policies = dict->Find(entry.config_key);
    if (!policies) {
      continue;
    }
    if (!policies->is_dict()) {
      LOG(WARNING) << "Policy for '" << entry.config_key << "' is not a dict.";
      return false;
    }
    for (const auto [extension_id_and_version, value] : policies->GetDict()) {
      if (!value.is_dict()) {
        LOG(WARNING) << "Policy for extension '" << extension_id_and_version
                     << "' is not a dict.";
        return false;
      }
      const base::DictValue& dict_value = value.GetDict();
      std::optional<std::string> serialized_proto =
          ValidateAndSerializeExtensionInstallPolicyValue(
              extension_id_and_version, dict_value);
      if (!serialized_proto.has_value()) {
        continue;
      }
      policy_storage()->SetPolicyPayload(entry.policy_type,
                                         extension_id_and_version,
                                         serialized_proto.value());
    }
  }

  return true;
}

bool FakeDMServer::ParsePolicyBlobs(const base::DictValue* dict) {
  const base::ListValue* policies = dict->FindList(kPoliciesKey);
  if (policies) {
    for (const base::Value& policy : *policies) {
      const base::DictValue* policy_as_dict = policy.GetIfDict();
      if (!policy_as_dict) {
        LOG(ERROR) << "The current policy isn't a dict";
        return false;
      }
      if (!SetPolicyPayload(policy_as_dict->FindString(kPolicyTypeKey),
                            policy_as_dict->FindString(kEntityIdKey),
                            policy_as_dict->FindString(kPolicyValueKey))) {
        LOG(ERROR) << "Failed to set the policy";
        return false;
      }
    }
  }

  const base::ListValue* external_policies =
      dict->FindList(kExternalPoliciesKey);
  if (external_policies) {
    for (const base::Value& policy : *external_policies) {
      const base::DictValue* policy_as_dict = policy.GetIfDict();
      if (!policy_as_dict) {
        LOG(ERROR) << "The current external policy isn't a dict";
        return false;
      }
      if (!SetExternalPolicyPayload(
              policy_as_dict->FindString(kPolicyTypeKey),
              policy_as_dict->FindString(kEntityIdKey),
              policy_as_dict->FindString(kPolicyValueKey))) {
        LOG(ERROR) << "Failed to set the external policy";
        return false;
      }
    }
  }

  return true;
}

bool FakeDMServer::ReadPolicyBlobFile() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(embedded_server_sequence_checker_);
  if (!base::PathExists(policy_blob_path_)) {
    LOG(INFO) << "Policy blob file doesn't exist yet.";
    return true;
  }
  JSONFileValueDeserializer deserializer(policy_blob_path_);
  int error_code = 0;
  std::string error_msg;
  std::unique_ptr<base::Value> value =
      deserializer.Deserialize(&error_code, &error_msg);
  if (!value) {
    LOG(ERROR) << "Failed to read the policy blob file " << policy_blob_path_
               << ": " << error_msg;
    return false;
  }
  LOG(INFO) << "Deserialized value of the policy blob: " << *value;
  const base::DictValue* dict = value->GetIfDict();
  if (!dict) {
    LOG(ERROR) << "Policy blob isn't a dict";
    return false;
  }

  ParsePolicyUser(dict, policy_storage());
  ParseManagedUsers(dict, policy_storage());
  ParseDeviceAffiliationIds(dict, policy_storage());
  ParseUserAffiliationIds(dict, policy_storage());
  ParseDirectoryApiId(dict, policy_storage());
  ParseRobotApiAuthCode(dict, policy_storage());
  RETURN_IF_FALSE(ParseAllowSetDeviceAttributes(dict, policy_storage()));
  RETURN_IF_FALSE(ParseUseUniversalSigningKeys(dict, policy_storage()));
  RETURN_IF_FALSE(ParseRequestErrors(dict, this));
  RETURN_IF_FALSE(ParseInitialEnrollmentState(dict, policy_storage()));
  RETURN_IF_FALSE(ParseCurrentKeyIndex(dict, policy_storage()));
  if (dict->contains(kPoliciesKey) || dict->contains(kExternalPoliciesKey)) {
    LOG(INFO) << "Parsing policies from base64 blobs.";
    RETURN_IF_FALSE(ParsePolicyBlobs(dict));
  } else {
    LOG(INFO) << "Parsing policies from JSON.";
    RETURN_IF_FALSE(ParsePoliciesJson(dict));
  }
  return true;
}

base::DictValue FakeDMServer::GetValueFromClient(
    const policy::ClientStorage::ClientInfo& c) {
  base::DictValue dict;
  dict.Set(kDeviceIdKey, c.device_id);
  dict.Set(kDeviceTokenKey, c.device_token);
  dict.Set(kMachineNameKey, c.machine_name);
  if (c.username.has_value()) {
    dict.Set(kUsernameKey, c.username.value());
  }
  base::ListValue state_keys, allowed_policy_types;
  for (auto& key : c.state_keys) {
    state_keys.Append(key);
  }
  dict.Set(kStateKeysKey, std::move(state_keys));
  for (auto& policy_type : c.allowed_policy_types) {
    allowed_policy_types.Append(policy_type);
  }
  dict.Set(kAllowedPolicyTypesKey, std::move(allowed_policy_types));
  return dict;
}

bool FakeDMServer::WriteClientStateFile() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(embedded_server_sequence_checker_);
  std::vector<policy::ClientStorage::ClientInfo> clients =
      client_storage()->GetAllClients();
  base::DictValue dict_clients;
  for (auto& c : clients) {
    dict_clients.Set(c.device_id, GetValueFromClient(c));
  }

  JSONFileValueSerializer serializer(client_state_path_);
  return serializer.Serialize(base::ValueView(dict_clients));
}

bool FakeDMServer::FindKey(const base::DictValue& dict,
                           const std::string& key,
                           base::Value::Type type) {
  switch (type) {
    case base::Value::Type::STRING: {
      const std::string* str_val = dict.FindString(key);
      if (!str_val) {
        LOG(ERROR) << "Key `" << key << "` is missing or not a string.";
        return false;
      }
      return true;
    }
    case base::Value::Type::LIST: {
      const base::ListValue* list_val = dict.FindList(key);
      if (!list_val) {
        LOG(ERROR) << "Key `" << key << "` is missing or not a list.";
        return false;
      }
      return true;
    }
    default: {
      NOTREACHED() << "Unsupported type for client file key";
    }
  }
}

std::optional<policy::ClientStorage::ClientInfo>
FakeDMServer::GetClientFromValue(const base::Value& v) {
  policy::ClientStorage::ClientInfo client_info;
  const base::DictValue* dict = v.GetIfDict();
  if (!dict) {
    LOG(ERROR) << "Client value isn't a dict";
    return std::nullopt;
  }

  if (!FindKey(*dict, kDeviceIdKey, base::Value::Type::STRING) ||
      !FindKey(*dict, kDeviceTokenKey, base::Value::Type::STRING) ||
      !FindKey(*dict, kMachineNameKey, base::Value::Type::STRING) ||
      !FindKey(*dict, kStateKeysKey, base::Value::Type::LIST) ||
      !FindKey(*dict, kAllowedPolicyTypesKey, base::Value::Type::LIST)) {
    return std::nullopt;
  }

  client_info.device_id = *dict->FindString(kDeviceIdKey);
  client_info.device_token = *dict->FindString(kDeviceTokenKey);
  client_info.machine_name = *dict->FindString(kMachineNameKey);
  client_info.username = base::OptionalFromPtr(dict->FindString(kUsernameKey));
  const base::ListValue* state_keys = dict->FindList(kStateKeysKey);
  for (const auto& it : *state_keys) {
    const std::string* key = it.GetIfString();
    if (!key) {
      LOG(ERROR) << "State key list entry is not a string: " << it;
      return std::nullopt;
    }
    client_info.state_keys.emplace_back(*key);
  }
  const base::ListValue* policy_types = dict->FindList(kAllowedPolicyTypesKey);
  for (const auto& it : *policy_types) {
    const std::string* key = it.GetIfString();
    if (!key) {
      LOG(ERROR) << "Policy type list entry is not a string: " << it;
      return std::nullopt;
    }
    client_info.allowed_policy_types.insert(*key);
  }
  return client_info;
}

bool FakeDMServer::ReadClientStateFile() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(embedded_server_sequence_checker_);
  if (!base::PathExists(client_state_path_)) {
    LOG(INFO) << "Client state file doesn't exist yet.";
    return true;
  }
  JSONFileValueDeserializer deserializer(client_state_path_);
  int error_code = 0;
  std::string error_msg;
  std::unique_ptr<base::Value> value =
      deserializer.Deserialize(&error_code, &error_msg);
  if (!value) {
    LOG(ERROR) << "Failed to read client state file " << client_state_path_
               << ": " << error_msg;
    return false;
  }
  const base::DictValue* dict = value->GetIfDict();
  if (!dict) {
    LOG(ERROR) << "The client state file isn't a dict.";
    return false;
  }
  for (auto it : *dict) {
    std::optional<policy::ClientStorage::ClientInfo> c =
        GetClientFromValue(it.second);
    if (!c.has_value()) {
      LOG(ERROR) << "The client isn't configured correctly.";
      return false;
    }
    client_storage()->RegisterClient(c.value());
  }
  return true;
}

}  // namespace fakedms
