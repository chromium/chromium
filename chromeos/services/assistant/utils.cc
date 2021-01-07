// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/utils.h"

#include <utility>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/json/json_writer.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/values.h"
#include "build/util/webkit_version.h"
#include "chromeos/assistant/internal/internal_constants.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/dbus/util/version_loader.h"
#include "chromeos/services/assistant/public/cpp/features.h"

namespace chromeos {
namespace assistant {

namespace {

void CreateUserAgent(std::string* user_agent) {
  DCHECK(user_agent->empty());
  base::StringAppendF(user_agent,
                      "Mozilla/5.0 (X11; CrOS %s %s; %s) "
                      "AppleWebKit/%d.%d (KHTML, like Gecko)",
                      base::SysInfo::OperatingSystemArchitecture().c_str(),
                      base::SysInfo::OperatingSystemVersion().c_str(),
                      base::SysInfo::GetLsbReleaseBoard().c_str(),
                      WEBKIT_VERSION_MAJOR, WEBKIT_VERSION_MINOR);

  std::string arc_version = chromeos::version_loader::GetARCVersion();
  if (!arc_version.empty())
    base::StringAppendF(user_agent, " ARC/%s", arc_version.c_str());
}

}  // namespace

// Get the root path for assistant files.
base::FilePath GetRootPath() {
  base::FilePath home_dir;
  CHECK(base::PathService::Get(base::DIR_HOME, &home_dir));
  // Ensures DIR_HOME is overridden after primary user sign-in.
  CHECK_NE(base::GetHomeDir(), home_dir);
  return home_dir;
}

base::FilePath GetBaseAssistantDir() {
  return GetRootPath().Append(FILE_PATH_LITERAL("google-assistant-library"));
}

std::string CreateLibAssistantConfig(
    base::Optional<std::string> s3_server_uri_override,
    base::Optional<std::string> device_id_override) {
  using Value = base::Value;
  using Type = base::Value::Type;

  Value config(Type::DICTIONARY);

  Value device(Type::DICTIONARY);
  device.SetKey("board_name", Value(base::SysInfo::GetLsbReleaseBoard()));
  device.SetKey("board_revision", Value("1"));
  device.SetKey("embedder_build_info",
                Value(chromeos::version_loader::GetVersion(
                    chromeos::version_loader::VERSION_FULL)));
  device.SetKey("model_id", Value(kModelId));
  device.SetKey("model_revision", Value(1));
  config.SetKey("device", std::move(device));

  Value discovery(Type::DICTIONARY);
  discovery.SetKey("enable_mdns", Value(false));
  config.SetKey("discovery", std::move(discovery));

  Value internal(Type::DICTIONARY);
  internal.SetKey("surface_type", Value("OPA_CROS"));

  std::string user_agent;
  CreateUserAgent(&user_agent);
  internal.SetKey("user_agent", Value(user_agent));

  // Prevent LibAssistant from automatically playing ready message TTS during
  // the startup sequence when the version of LibAssistant has been upgraded.
  internal.SetKey("override_ready_message", Value(true));

  // Set DeviceProperties.visibility to Visibility::PRIVATE.
  // See //libassistant/shared/proto/device_properties.proto.
  internal.SetKey("visibility", Value("PRIVATE"));

  if (base::SysInfo::IsRunningOnChromeOS()) {
    Value logging(Type::DICTIONARY);
    // Redirect libassistant logging to /var/log/chrome/ if has the switch,
    // otherwise log to 'log' sub dir in user's home dir.
    const bool redirect_logging =
        base::CommandLine::ForCurrentProcess()->HasSwitch(
            chromeos::switches::kRedirectLibassistantLogging);
    const std::string log_dir =
        redirect_logging
            ? "/var/log/chrome/"
            : GetRootPath().Append(FILE_PATH_LITERAL("log")).value();
    logging.SetKey("directory", Value(log_dir));
    // Maximum disk space consumed by all log files. There are 5 rotating log
    // files on disk.
    logging.SetKey("max_size_kb", Value(3 * 1024));
    // Empty "output_type" disables logging to stderr.
    logging.SetKey("output_type", Value(Type::LIST));
    config.SetKey("logging", std::move(logging));
  } else {
    // Print logs to console if running in desktop mode.
    internal.SetKey("disable_log_files", Value(true));
  }

  // Enable logging.
  internal.SetBoolKey("enable_logging", true);

  // This only enables logging to local disk combined with the flag above. When
  // user choose to file a Feedback report, user can examine the log and choose
  // to upload the log with the report or not.
  internal.SetBoolKey("logging_opt_in", true);

  // Allows libassistant to automatically toggle signed-out mode depending on
  // whether it has auth_tokens.
  internal.SetBoolKey("enable_signed_out_mode", true);

  config.SetKey("internal", std::move(internal));

  Value audio_input(Type::DICTIONARY);
  // Skip sending speaker ID selection info to disable user verification.
  audio_input.SetKey("should_send_speaker_id_selection_info", Value(false));

  Value sources(Type::LIST);
  Value dict(Type::DICTIONARY);
  dict.SetKey("enable_eraser", Value(features::IsAudioEraserEnabled()));
  dict.SetKey("enable_eraser_toggling",
              Value(features::IsAudioEraserEnabled()));
  sources.Append(std::move(dict));
  audio_input.SetKey("sources", std::move(sources));

  config.SetKey("audio_input", std::move(audio_input));

  if (features::IsLibAssistantBetaBackendEnabled())
    config.SetStringPath("internal.backend_type", "BETA_DOGFOOD");

  // Use http unless we're using the fake s3 server, which requires grpc.
  if (s3_server_uri_override)
    config.SetStringPath("internal.transport_type", "GRPC");
  else
    config.SetStringPath("internal.transport_type", "HTTP");

  if (device_id_override)
    config.SetStringPath("internal.cast_device_id", device_id_override.value());

  config.SetBoolPath("internal.enable_on_device_assistant_tts_as_text", true);

  // Finally add in the server uri override.
  if (s3_server_uri_override) {
    config.SetStringPath("testing.s3_grpc_server_uri",
                         s3_server_uri_override.value());
  }

  std::string json;
  base::JSONWriter::Write(config, &json);
  return json;
}

}  // namespace assistant
}  // namespace chromeos
