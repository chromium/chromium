// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/utils.h"

#include <utility>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/values.h"
#include "build/util/webkit_version.h"
#include "chromeos/assistant/internal/internal_constants.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/dbus/util/version_loader.h"
#include "chromeos/services/assistant/public/features.h"

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

std::string CreateLibAssistantConfig() {
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

  std::string json;
  base::JSONWriter::Write(config, &json);
  return json;
}

}  // namespace assistant
}  // namespace chromeos
