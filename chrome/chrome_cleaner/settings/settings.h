// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_SETTINGS_SETTINGS_H_
#define CHROME_CHROME_CLEANER_SETTINGS_SETTINGS_H_

#include <windows.h>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/command_line.h"
#include "base/memory/singleton.h"
#include "base/strings/string16.h"
#include "chrome/chrome_cleaner/logging/proto/shared_data.pb.h"
#include "chrome/chrome_cleaner/settings/settings_definitions.h"
#include "components/chrome_cleaner/public/constants/constants.h"

namespace chrome_cleaner {

// Read the given switch as a time in minutes. If it's missing or invalid,
// return false to use the default timeout. Otherwise return true and put the
// time in |timeout|.
bool GetTimeoutOverride(const base::CommandLine& command_line,
                        const char* switch_name,
                        base::TimeDelta* timeout);

std::vector<UwS::TraceLocation> GetValidTraceLocations();

class Settings {
 public:
  static Settings* GetInstance();
  static void SetInstanceForTesting(Settings* instance_for_testing);

  // Returns true if uploading crash reports is allowed. Crash reports are
  // always saved on disk regardless of this setting.
  virtual bool allow_crash_report_upload() const;

  // Returns the session id for this run as passed by Chrome to the reporter.
  virtual base::string16 session_id() const;

  virtual std::string cleanup_id() const;

  // Returns the engine to use as passed by Chrome.
  virtual Engine::Name engine() const;

  virtual std::string engine_version() const;

  // Returns true if logs uploading is allowed for this runs (can be used by
  // both the Cleaner and the Reporter).
  virtual bool logs_upload_allowed() const;

  // Returns true if logs collection is enabled for this run. When true, data
  // will be saved to a proto that can be saved to disk or sent to Google if
  // logs upload is allowed. Integration tests and automated evaluations are
  // expected to run the reporter with logs collection enabled and logs upload
  // disallowed.
  virtual bool logs_collection_enabled() const;

  // Returns true if the cleaner running in kCleanup mode will be allowed to
  // upload logs to Google. This setting is only used by the cleaner in
  // kScanning mode, which will set according to the user's response in the
  // Chrome prompt and will be used to propagate the flag that enables logging
  // to the process in kCleanup mode.
  virtual bool logs_allowed_in_cleanup_mode() const;

  virtual void set_logs_allowed_in_cleanup_mode(bool new_value);

  // Returns true if metrics reporting is enabled for the user.
  virtual bool metrics_enabled() const;

  // Returns true if Safe Browsing extended reporting is enabled for the user.
  virtual bool sber_enabled() const;

  // Returns an empty string if prompt_using_mojo() is false.
  virtual const std::string& chrome_mojo_pipe_token() const;

  // Returns false if prompt_using_mojo() is false.
  virtual bool has_parent_pipe_handle() const;

  virtual bool prompt_using_mojo() const;

  // Returns the handle value passed on the command line, even if the handle
  // has been closed. Returns INVALID_HANDLE_VALUE if prompt_using_mojo() is
  // true.
  virtual HANDLE prompt_response_read_handle() const;

  // Returns the handle value passed on the command line, even if the handle
  // has been closed. Returns INVALID_HANDLE_VALUE if prompt_using_mojo() is
  // true.
  virtual HANDLE prompt_request_write_handle() const;

  // Returns true if the command-line switches specify a valid IPC setup.
  // Since IPC is only used in scanning mode, all combinations of switches in
  // other modes are considered valid.
  virtual bool switches_valid_for_ipc() const;

  // Returns true if any IPC related switch is included on the command-line.
  // switches_valid_for_ipc() may return false even if this returns true.
  virtual bool has_any_ipc_switch() const;

  // Returns the execution mode sent by Chrome if valid, or kNone if
  // kExecutionModeSwitch is not present or the corresponding value is invalid.
  virtual ExecutionMode execution_mode() const;

  // Returns true if experimental engine should remove report-only UwS.
  virtual bool remove_report_only_uws() const;

  // Returns true if the timeout for the cleaning phase has been overridden on
  // the command-line.
  virtual bool cleaning_timeout_overridden() const;

  // If the timeout for the cleaning phase has been overridden, return the new
  // timeout. Otherwise returns TimeDelta(0). Note that this function cannot be
  // used to determine if the timout value has been overridden since overriding
  // with a value of zero is indistinguishable from not having an override; use
  // cleaning_timeout_overridden() for that purpose.
  virtual base::TimeDelta cleaning_timeout() const;

  // Returns true if the timeout for the scanning phase has been overridden on
  // the command-line.
  virtual bool scanning_timeout_overridden() const;

  // If the timeout for the scanning phase has been overridden, return the new
  // timeout. Otherwise returns TimeDelta(0). Note that this function cannot be
  // used to determine if the timout value has been overridden since overriding
  // with a value of zero is indistinguishable from not having an override; use
  // scanning_timeout_overridden() for that purpose.
  virtual base::TimeDelta scanning_timeout() const;

  // Returns true if the timeout for how long the scanner waits for user
  // response from Chrome has been overridden on the command-line.
  virtual bool user_response_timeout_overridden() const;

  // If the timeout for how long the scanner waits for user response from Chrome
  // has been overridden, return the new timeout. Otherwise returns
  // TimeDelta(0). Note that this function cannot be used to determine if the
  // timout value has been overridden since overriding with a value of zero is
  // indistinguishable from not having an override; use
  // user_response_timeout_overridden() for that purpose.
  virtual base::TimeDelta user_response_timeout() const;

  // Returns list of trace locations, to which scanning should be limited.
  virtual const std::vector<UwS::TraceLocation>& locations_to_scan() const;

  // Returns true if no invalid values were provided on the command line to
  // scanning configuration switches. If there were invalid arguments, returns
  // false, but other settings properties will be set to known safe defaults.
  virtual bool scan_switches_correct() const;

  // If this returns greater than zero value, should disallow scanning of files
  // bigger than the returned limit.
  virtual int64_t open_file_size_limit() const;

  // If this returns true, engines can be loaded outside the sandbox. This can
  // only return true in developer builds.
  virtual bool run_without_sandbox_for_testing() const;

 protected:
  Settings();
  virtual ~Settings();

 private:
  friend struct base::DefaultSingletonTraits<Settings>;
  friend class CleanerSettingsTest;
  friend class ReporterSettingsTest;
  friend class SettingsTest;

  // This method is separate from the constructor, so it can be called by tests.
  void Initialize(const base::CommandLine& command_line,
                  TargetBinary target_binary);

  // Crash related settings.
  bool allow_crash_report_upload_ = false;

  // Logging related settings.
  bool logs_collection_enabled_ = false;
  bool logs_upload_allowed_ = false;
  bool metrics_enabled_ = false;
  bool sber_enabled_ = false;
  bool logs_allowed_in_cleanup_mode_ = false;

  // Statistics about the current run.
  std::string cleanup_id_;
  base::string16 session_id_;

  // Execution parameters.
  ExecutionMode execution_mode_ = ExecutionMode::kNone;
  bool remove_report_only_uws_ = false;
  bool cleaning_timeout_overridden_ = false;
  base::TimeDelta cleaning_timeout_;
  bool scanning_timeout_overridden_ = false;
  base::TimeDelta scanning_timeout_;
  bool user_response_timeout_overridden_ = false;
  base::TimeDelta user_response_timeout_;
  std::vector<UwS::TraceLocation> locations_to_scan_;
  bool scan_switches_correct_ = false;
  int64_t open_file_size_limit_ = 0;

  // Mojo related settings.
  std::string chrome_mojo_pipe_token_;
  bool has_parent_pipe_handle_ = false;
  bool prompt_using_mojo_ = false;

  // Proto related settings.
  bool prompt_using_proto_ = false;
  HANDLE prompt_response_read_handle_ = INVALID_HANDLE_VALUE;
  HANDLE prompt_request_write_handle_ = INVALID_HANDLE_VALUE;

  // Engine selection settings.
  Engine::Name engine_ = Engine::UNKNOWN;

  bool run_without_sandbox_for_testing_ = false;

  static Settings* instance_for_testing_;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_SETTINGS_SETTINGS_H_
