// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/enterprise_companion/global_constants.h"

#include <memory>
#include <optional>
#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/enterprise_companion/enterprise_companion_branding.h"
#include "chrome/enterprise_companion/installer_paths.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_WIN)
#include "base/strings/utf_string_conversions.h"
#endif

namespace enterprise_companion {

const char kCompanionAppId[] = ENTERPRISE_COMPANION_APPID;

// Overrides JSON keys.
const char kCrashUploadUrlKey[] = "crash_upload_url";
const char kDMEncryptedReportingUrlKey[] = "dm_encrypted_reporting_url";
const char kDMRealtimeReportingUrlKey[] = "dm_realtime_reporting_url";
const char kDMServerUrlKey[] = "dm_server_url";
const char kEventLoggingUrlKey[] = "event_logging_url";
extern const char kEventLoggerMinTimeoutSecKey[] =
    "event-logger-min-timeout-sec";
#if BUILDFLAG(IS_WIN)
const char kNamedPipeSecurityDescriptorKey[] = "named-pipe-security-descriptor";
#endif

namespace {

class GlobalConstantsImpl : public GlobalConstants {
 public:
  GlobalConstantsImpl() {
#ifdef ENTERPRISE_COMPANION_TEST_ONLY
    ApplyOverrides();
#endif
  }

  ~GlobalConstantsImpl() override = default;

  GURL CrashUploadURL() const override { return crash_upload_url_; }

  GURL DeviceManagementEncryptedReportingURL() const override {
    return device_management_encrypted_reporting_url_;
  }

  GURL DeviceManagementRealtimeReportingURL() const override {
    return device_management_realtime_reporting_url_;
  }

  GURL DeviceManagementServerURL() const override {
    return device_management_server_url_;
  }

  GURL EnterpriseCompanionEventLoggingURL() const override {
    return enterprise_companion_event_logging_url_;
  }

  base::TimeDelta EventLoggerMinTimeout() const override {
    return event_logger_min_timeout_;
  }

#if BUILDFLAG(IS_WIN)
  std::wstring NamedPipeSecurityDescriptor() const override {
    return named_pipe_security_descriptor_;
  }
#endif

 private:
  GURL crash_upload_url_ = GURL(CRASH_UPLOAD_URL);
  GURL device_management_encrypted_reporting_url_ =
      GURL(DEVICE_MANAGEMENT_ENCRYPTED_REPORTING_URL);
  GURL device_management_realtime_reporting_url_ =
      GURL(DEVICE_MANAGEMENT_REALTIME_REPORTING_URL);
  GURL device_management_server_url_ = GURL(DEVICE_MANAGEMENT_SERVER_URL);
  GURL enterprise_companion_event_logging_url_ =
      GURL(ENTERPRISE_COMPANION_EVENT_LOGGING_URL);
  base::TimeDelta event_logger_min_timeout_ = base::Minutes(15);

#if BUILDFLAG(IS_WIN)
  // By default allow access from the local system account only.
  std::wstring named_pipe_security_descriptor_ = L"D:(A;;GA;;;SY)";
#endif

#ifdef ENTERPRISE_COMPANION_TEST_ONLY
  void ApplyOverrides() {
    std::optional<base::FilePath> overrides_json_path = GetOverridesFilePath();
    if (!overrides_json_path) {
      VLOG(1) << "Failed to apply overrides: can't get overrides file path.";
      return;
    }

    JSONFileValueDeserializer parser(*overrides_json_path,
                                     base::JSON_ALLOW_TRAILING_COMMAS);
    int error_code = 0;
    std::string error_message;
    std::unique_ptr<base::Value> parsed_value(
        parser.Deserialize(&error_code, &error_message));
    if (error_code || !parsed_value) {
      VLOG(1) << "Could not parse " << *overrides_json_path << ": error "
              << error_code << ": " << error_message;
      return;
    }

    if (!parsed_value->is_dict()) {
      VLOG(1) << "Invalid data in " << *overrides_json_path << ": not a dict.";
      return;
    }

    const base::Value::Dict& overrides = parsed_value->GetDict();

    ApplyOverride(overrides, kCrashUploadUrlKey, crash_upload_url_);
    ApplyOverride(overrides, kDMEncryptedReportingUrlKey,
                  device_management_encrypted_reporting_url_);
    ApplyOverride(overrides, kDMRealtimeReportingUrlKey,
                  device_management_realtime_reporting_url_);
    ApplyOverride(overrides, kDMServerUrlKey, device_management_server_url_);
    ApplyOverride(overrides, kEventLoggingUrlKey,
                  enterprise_companion_event_logging_url_);
    ApplyOverride(overrides, kEventLoggerMinTimeoutSecKey,
                  event_logger_min_timeout_);

#if BUILDFLAG(IS_WIN)
    ApplyOverride(overrides, kNamedPipeSecurityDescriptorKey,
                  named_pipe_security_descriptor_);
#endif
  }

  void ApplyOverride(const base::Value::Dict& overrides,
                     const std::string& key,
                     GURL& value) {
    const std::string* str = overrides.FindString(key);
    if (str) {
      value = GURL(*str);
    }
  }

  void ApplyOverride(const base::Value::Dict& overrides,
                     const std::string& key,
                     base::TimeDelta& value) {
    std::optional<int> override_val = overrides.FindInt(key);
    if (override_val) {
      value = base::Seconds(*override_val);
    }
  }

#if BUILDFLAG(IS_WIN)
  void ApplyOverride(const base::Value::Dict& overrides,
                     const std::string& key,
                     std::wstring& value) {
    const std::string* str = overrides.FindString(key);
    if (str) {
      value = base::ASCIIToWide(*str);
    }
  }
#endif
#endif  // ENTERPRISE_COMPANION_TEST_ONLY
};

}  // namespace

std::optional<base::FilePath> GetOverridesFilePath() {
  std::optional<base::FilePath> install_dir = GetInstallDirectory();
  if (!install_dir) {
    VLOG(1) << "Can't get overrides file path: can't get install dir.";
    return std::nullopt;
  }
  return install_dir->AppendASCII("overrides.json");
}

const GlobalConstants* GetGlobalConstants() {
  static const base::NoDestructor<GlobalConstantsImpl> global_constants;
  return global_constants.get();
}

}  // namespace enterprise_companion
