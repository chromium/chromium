// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/libassistant/util.h"

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/json/json_writer.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/values.h"
#include "build/util/chromium_git_revision.h"
#include "chromeos/ash/services/assistant/public/cpp/features.h"
#include "chromeos/ash/services/assistant/public/cpp/switches.h"
#include "chromeos/ash/services/libassistant/constants.h"
#include "chromeos/ash/services/libassistant/grpc/grpc_util.h"
#include "chromeos/ash/services/libassistant/public/cpp/android_app_info.h"
#include "chromeos/assistant/internal/internal_constants.h"
#include "chromeos/assistant/internal/internal_util.h"
#include "chromeos/assistant/internal/util_headers.h"
#include "chromeos/version/version_loader.h"

using ::assistant::api::Interaction;
using chromeos::assistant::shared::ClientInteraction;
using chromeos::assistant::shared::ClientOpResult;
using chromeos::assistant::shared::GetDeviceSettingsResult;
using chromeos::assistant::shared::Protobuf;
using chromeos::assistant::shared::ProviderVerificationResult;
using chromeos::assistant::shared::ResponseCode;
using chromeos::assistant::shared::SettingInfo;
using chromeos::assistant::shared::VerifyProviderClientOpResult;

namespace ash::libassistant {

namespace {

using AppStatus = assistant::AppStatus;

void CreateUserAgent(std::string* user_agent) {
  DCHECK(user_agent->empty());
  base::StringAppendF(user_agent,
                      "Mozilla/5.0 (X11; CrOS %s %s; %s) "
                      "AppleWebKit/537.36 (KHTML, like Gecko)",
                      base::SysInfo::OperatingSystemArchitecture().c_str(),
                      base::SysInfo::OperatingSystemVersion().c_str(),
                      base::SysInfo::GetLsbReleaseBoard().c_str());

  std::string arc_version = chromeos::version_loader::GetArcVersion();
  if (!arc_version.empty())
    base::StringAppendF(user_agent, " ARC/%s", arc_version.c_str());
}

ProviderVerificationResult::VerificationStatus GetProviderVerificationStatus(
    AppStatus status) {
  switch (status) {
    case AppStatus::kUnknown:
      return ProviderVerificationResult::UNKNOWN;
    case AppStatus::kAvailable:
      return ProviderVerificationResult::AVAILABLE;
    case AppStatus::kUnavailable:
      return ProviderVerificationResult::UNAVAILABLE;
    case AppStatus::kVersionMismatch:
      return ProviderVerificationResult::VERSION_MISMATCH;
    case AppStatus::kDisabled:
      return ProviderVerificationResult::DISABLED;
  }
}

SettingInfo ToSettingInfo(bool is_supported) {
  SettingInfo result;
  result.set_available(is_supported);
  result.set_setting_status(is_supported
                                ? SettingInfo::AVAILABLE_AND_MODIFY_SUPPORTED
                                : SettingInfo::UNAVAILABLE);
  return result;
}

// Helper class used for constructing V1 interaction proto messages.
class V1InteractionBuilder {
 public:
  V1InteractionBuilder() = default;
  V1InteractionBuilder(V1InteractionBuilder&) = delete;
  V1InteractionBuilder& operator=(V1InteractionBuilder&) = delete;
  ~V1InteractionBuilder() = default;

  V1InteractionBuilder& SetInResponseTo(int interaction_id) {
    interaction_.set_in_response_to(interaction_id);
    return *this;
  }

  V1InteractionBuilder& AddResult(
      const std::string& key,
      const google::protobuf::MessageLite& result_proto) {
    auto* result = client_op_result()->mutable_results()->add_result();
    result->set_key(key);
    result->mutable_value()->set_protobuf_type(result_proto.GetTypeName());
    result->mutable_value()->set_protobuf_data(
        result_proto.SerializeAsString());
    return *this;
  }

  V1InteractionBuilder& SetStatusCode(ResponseCode::Status status_code) {
    ResponseCode* response_code = client_op_result()->mutable_response_code();
    response_code->set_status_code(status_code);
    return *this;
  }

  // Set the status code to |OK| (if true) or |NOT_FOUND| (if false).
  V1InteractionBuilder& SetStatusCodeFromEntityFound(bool found) {
    SetStatusCode(found ? ResponseCode::OK : ResponseCode::NOT_FOUND);
    return *this;
  }

  V1InteractionBuilder& SetClientInputName(const std::string& name) {
    auto* client_input = client_interaction()->mutable_client_input();
    client_input->set_client_input_name(name);
    return *this;
  }

  V1InteractionBuilder& AddClientInputParams(
      const std::string& key,
      const google::protobuf::MessageLite& params_proto) {
    auto* client_input = client_interaction()->mutable_client_input();
    Protobuf& value = (*client_input->mutable_params())[key];
    value.set_protobuf_type(params_proto.GetTypeName());
    value.set_protobuf_data(params_proto.SerializeAsString());
    return *this;
  }

  std::string SerializeAsString() { return interaction_.SerializeAsString(); }

  Interaction Proto() { return interaction_; }

 private:
  ClientInteraction* client_interaction() {
    return interaction_.mutable_from_client();
  }

  ClientOpResult* client_op_result() {
    return client_interaction()->mutable_client_op_result();
  }

  Interaction interaction_;
};

bool ShouldPutLogsInHomeDirectory() {
  const bool redirect_logging =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          assistant::switches::kRedirectLibassistantLogging);
  return !redirect_logging;
}

bool ShouldLogToFile() {
  const bool disable_logfile =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          assistant::switches::kDisableLibAssistantLogfile);
  return !disable_logfile;
}

}  // namespace

base::FilePath GetBaseAssistantDir() {
  return base::FilePath(FILE_PATH_LITERAL(kAssistantBaseDirPath));
}

std::string CreateLibAssistantConfig(
    std::optional<std::string> s3_server_uri_override,
    std::optional<std::string> device_id_override) {
  using Value = base::Value;

  Value::Dict config;

  std::optional<std::string> version = chromeos::version_loader::GetVersion(
      chromeos::version_loader::VERSION_FULL);
  config.Set("device",
             Value::Dict()
                 .Set("board_name", base::SysInfo::GetLsbReleaseBoard())
                 .Set("board_revision", "1")
                 .Set("embedder_build_info", version.value_or("0.0.0.0"))
                 .Set("model_id", chromeos::assistant::kModelId)
                 .Set("model_revision", 1));

  // Enables Libassistant gRPC server for V2.
  const bool is_chromeos_device = base::SysInfo::IsRunningOnChromeOS();
  const std::string server_addresses =
      GetLibassistantServiceAddress(is_chromeos_device) + "," +
      GetHttpConnectionServiceAddress(is_chromeos_device);

  config.Set("libas_server", Value::Dict()
                                 .Set("libas_server_address", server_addresses)
                                 .Set("enable_display_service", true)
                                 .Set("enable_http_connection_service", true));

  config.Set("discovery", Value::Dict().Set("enable_mdns", false));

  std::string user_agent;
  CreateUserAgent(&user_agent);

  auto internal =
      Value::Dict()
          .Set("surface_type", "OPA_CROS")
          .Set("user_agent", user_agent)

          // Prevent LibAssistant from automatically playing ready
          // message TTS during the startup sequence when the
          // version of LibAssistant has been upgraded.
          .Set("override_ready_message", true)

          // Set DeviceProperties.visibility to Visibility::PRIVATE.
          // See //libassistant/shared/proto/device_properties.proto.
          .Set("visibility", "PRIVATE")

          // Enable logging.
          .Set("enable_logging", true)

          // This only enables logging to local disk combined with the flag
          // above. When user choose to file a Feedback report, user can examine
          // the log and choose to upload the log with the report or not.
          .Set("logging_opt_in", true)

          // Allows libassistant to automatically toggle signed-out mode
          // depending on whether it has auth_tokens.
          .Set("enable_signed_out_mode", true);

  if (ShouldLogToFile()) {
    std::string log_dir("/var/log/chrome/");
    if (ShouldPutLogsInHomeDirectory()) {
      base::FilePath log_path =
          GetBaseAssistantDir().Append(FILE_PATH_LITERAL("log"));

      // The directory will be created by LibassistantPreSandboxHook if sandbox
      // is enabled.
      if (!assistant::features::IsLibAssistantSandboxEnabled())
        CHECK(base::CreateDirectory(log_path));

      log_dir = log_path.value();
    }

    auto logging = Value::Dict()
                       .Set("directory", log_dir)
                       // Maximum disk space consumed by all log files. There
                       // are 5 rotating log files on disk.
                       .Set("max_size_kb", 3 * 1024)
                       // Empty "output_type" disables logging to stderr.
                       .Set("output_type", Value::List());
    config.Set("logging", std::move(logging));
  } else {
    // Print logs to console if running in desktop or test mode.
    internal.Set("disable_log_files", true);
  }

  config.Set("internal", std::move(internal));

  config.Set(
      "audio_input",
      Value::Dict()
          // Skip sending speaker ID selection to disable user verification.
          .Set("should_send_speaker_id_selection_info", false)
          .Set("sources",
               Value::List().Append(
                   Value::Dict()
                       .Set("enable_eraser",
                            assistant::features::IsAudioEraserEnabled())
                       .Set("enable_eraser_toggling",
                            assistant::features::IsAudioEraserEnabled()))));

  if (assistant::features::IsLibAssistantBetaBackendEnabled()) {
    config.SetByDottedPath("internal.backend_type", "BETA_DOGFOOD");
  }

  // Use http unless we're using the fake s3 server, which requires grpc.
  if (s3_server_uri_override) {
    config.SetByDottedPath("internal.transport_type", "GRPC");
  } else {
    config.SetByDottedPath("internal.transport_type", "HTTP");
  }

  if (device_id_override) {
    config.SetByDottedPath("internal.cast_device_id",
                           device_id_override.value());
  }

  config.SetByDottedPath("internal.enable_on_device_assistant_tts_as_text",
                         true);

  // Finally add in the server uri override.
  if (s3_server_uri_override) {
    config.SetByDottedPath("testing.s3_grpc_server_uri",
                           s3_server_uri_override.value());
  }

  std::string json;
  base::JSONWriter::Write(config, &json);
  return json;
}

Interaction CreateVerifyProviderResponseInteraction(
    const int interaction_id,
    const std::vector<assistant::AndroidAppInfo>& apps_info) {
  // Construct verify provider result proto.
  VerifyProviderClientOpResult result_proto;
  bool any_provider_available = false;
  for (const auto& android_app_info : apps_info) {
    auto* provider_status = result_proto.add_provider_status();
    provider_status->set_status(
        GetProviderVerificationStatus(android_app_info.status));
    auto* app_info =
        provider_status->mutable_provider_info()->mutable_android_app_info();
    app_info->set_package_name(android_app_info.package_name);
    app_info->set_app_version(android_app_info.version);
    app_info->set_localized_app_name(android_app_info.localized_app_name);
    app_info->set_android_intent(android_app_info.intent);

    if (android_app_info.status == AppStatus::kAvailable)
      any_provider_available = true;
  }

  // Construct response interaction.
  return V1InteractionBuilder()
      .SetInResponseTo(interaction_id)
      .SetStatusCodeFromEntityFound(any_provider_available)
      .AddResult(chromeos::assistant::kResultKeyVerifyProvider, result_proto)
      .Proto();
}

Interaction CreateGetDeviceSettingInteraction(
    int interaction_id,
    const std::vector<chromeos::assistant::DeviceSetting>& device_settings) {
  GetDeviceSettingsResult result_proto;
  for (const auto& setting : device_settings) {
    (*result_proto.mutable_settings_info())[setting.setting_id] =
        ToSettingInfo(setting.is_supported);
  }

  // Construct response interaction.
  return V1InteractionBuilder()
      .SetInResponseTo(interaction_id)
      .SetStatusCode(ResponseCode::OK)
      .AddResult(/*key=*/chromeos::assistant::kResultKeyGetDeviceSettings,
                 result_proto)
      .Proto();
}

Interaction CreateNotificationRequestInteraction(
    const std::string& notification_id,
    const std::string& consistent_token,
    const std::string& opaque_token,
    const int action_index) {
  auto request_param = chromeos::assistant::CreateNotificationRequestParam(
      notification_id, consistent_token, opaque_token, action_index);

  return V1InteractionBuilder()
      .SetClientInputName(chromeos::assistant::kClientInputRequestNotification)
      .AddClientInputParams(chromeos::assistant::kNotificationRequestParamsKey,
                            request_param)
      .Proto();
}

Interaction CreateNotificationDismissedInteraction(
    const std::string& notification_id,
    const std::string& consistent_token,
    const std::string& opaque_token,
    const std::vector<std::string>& grouping_keys) {
  auto dismiss_param = chromeos::assistant::CreateNotificationDismissedParam(
      notification_id, consistent_token, opaque_token, grouping_keys);

  return V1InteractionBuilder()
      .SetClientInputName(chromeos::assistant::kClientInputDismissNotification)
      .AddClientInputParams(chromeos::assistant::kNotificationDismissParamsKey,
                            dismiss_param)
      .Proto();
}

Interaction CreateEditReminderInteraction(const std::string& reminder_id) {
  auto intent_input = chromeos::assistant::CreateEditReminderParam(reminder_id);

  return V1InteractionBuilder()
      .SetClientInputName(chromeos::assistant::kClientInputEditReminder)
      .AddClientInputParams(chromeos::assistant::kEditReminderParamsKey,
                            intent_input)
      .Proto();
}

Interaction CreateOpenProviderResponseInteraction(const int interaction_id,
                                                  const bool provider_found) {
  return V1InteractionBuilder()
      .SetInResponseTo(interaction_id)
      .SetStatusCodeFromEntityFound(provider_found)
      .Proto();
}

Interaction CreateSendFeedbackInteraction(
    bool assistant_debug_info_allowed,
    const std::string& feedback_description,
    const std::string& screenshot_png) {
  auto feedback_arg = chromeos::assistant::CreateFeedbackParam(
      assistant_debug_info_allowed, feedback_description, screenshot_png);

  return V1InteractionBuilder()
      .SetClientInputName(chromeos::assistant::kClientInputText)
      .AddClientInputParams(chromeos::assistant::kTextParamsKey,
                            chromeos::assistant::CreateTextParam(
                                chromeos::assistant::kFeedbackText))
      .AddClientInputParams(chromeos::assistant::kFeedbackParamsKey,
                            feedback_arg)
      .Proto();
}

Interaction CreateTextQueryInteraction(const std::string& query) {
  return V1InteractionBuilder()
      .SetClientInputName(chromeos::assistant::kClientInputText)
      .AddClientInputParams(chromeos::assistant::kTextParamsKey,
                            chromeos::assistant::CreateTextParam(query))
      .Proto();
}

}  // namespace ash::libassistant
