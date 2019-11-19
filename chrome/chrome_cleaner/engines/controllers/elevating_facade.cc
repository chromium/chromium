// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/engines/controllers/elevating_facade.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/chrome_cleaner/constants/chrome_cleaner_switches.h"
#include "chrome/chrome_cleaner/os/disk_util.h"
#include "chrome/chrome_cleaner/os/pre_fetched_paths.h"
#include "chrome/chrome_cleaner/os/system_util_cleaner.h"
#include "chrome/chrome_cleaner/settings/settings.h"
#include "components/chrome_cleaner/public/constants/constants.h"
#include "mojo/public/cpp/platform/platform_channel.h"

namespace chrome_cleaner {

namespace {

constexpr base::TimeDelta kCheckPeriod = base::TimeDelta::FromSeconds(1);

// Returns true if the class name of |window| begins with the typical Chrome
// window class prefix.
bool IsChromeWindow(HWND window) {
  // The substring used to identify if a window belongs to Chrome. See also
  // https://cs.chromium.org/chromium/src/ui/gfx/win/window_impl.cc?q=Chrome_WidgetWin_
  static constexpr wchar_t kChromeWindowClassPrefix[] = L"Chrome_WidgetWin_";

  if (!window)
    return false;

  // Ask for just enough of the class name to determine if it begins with
  // |kChromeWindowClassPrefix|.
  wchar_t window_class_prefix[base::size(kChromeWindowClassPrefix)];
  int class_name_length = ::GetClassName(window, window_class_prefix,
                                         base::size(window_class_prefix));
  if (class_name_length == 0)
    return false;

  return base::EqualsCaseInsensitiveASCII(
      base::StringPiece16(window_class_prefix, class_name_length),
      base::StringPiece16(kChromeWindowClassPrefix,
                          base::size(kChromeWindowClassPrefix) - 1));
}

// Returns a handle to the foreground window if it is a Chrome window, otherwise
// returns nullptr. Must be called on the main "UI" thread.
HWND GetForegroundChromeWindow() {
  // If a Chrome window is the foreground window, return that one. This ensures
  // that the elevation prompt is not minimized on Win7 (it also works for later
  // Windows versions). Otherwise, return nullptr and let the system handle the
  // elevation prompt.
  HWND foreground_window = ::GetForegroundWindow();
  if (!IsChromeWindow(foreground_window))
    return nullptr;

  // The foreground Chrome window may be a modal dialog parented to a browser
  // window (e.g., the modal cleanup dialog). In this case, the UAC prompt
  // should be associated with the parent browser window so that closure of the
  // modal dialog doesn't result in the UAC prompt going to the background.
  HWND parent_window = ::GetParent(foreground_window);
  if (IsChromeWindow(parent_window))
    return parent_window;

  return foreground_window;
}

base::CommandLine GetElevatedCommandLine() {
  base::CommandLine elevated_cmd(
      PreFetchedPaths::GetInstance()->GetExecutablePath());
  elevated_cmd.AppendSwitchASCII(
      kExecutionModeSwitch,
      base::NumberToString(static_cast<int>(ExecutionMode::kCleanup)));
  elevated_cmd.AppendSwitch(kElevatedSwitch);

  Settings* settings = Settings::GetInstance();
  elevated_cmd.AppendSwitchASCII(kCleanupIdSwitch, settings->cleanup_id());

  if (settings->logs_allowed_in_cleanup_mode()) {
    elevated_cmd.AppendSwitch(kWithCleanupModeLogsSwitch);
  } else {
    // Just to be extra-careful, forcefully disable logs uploading if the user
    // declined to send logs in the Chrome prompt.
    elevated_cmd.AppendSwitch(kNoReportUploadSwitch);
  }

  // Just to be extra-careful, forcefully disable crash uploading if the user
  // is not in UMA/Metrics.
  if (!settings->allow_crash_report_upload())
    elevated_cmd.AppendSwitch(kNoCrashUploadSwitch);

  // Propagate needed switches to the elevated process.
  base::CommandLine* current_cmd = base::CommandLine::ForCurrentProcess();
  base::CommandLine::SwitchMap current_switches = current_cmd->GetSwitches();
  current_switches.erase(kExecutionModeSwitch);
  current_switches.erase(kChromeMojoPipeTokenSwitch);
  current_switches.erase(kChromeReadHandleSwitch);
  current_switches.erase(kChromeWriteHandleSwitch);
  // The flag that enables logs in scanning mode is not used in cleanup mode.
  current_switches.erase(kWithScanningModeLogsSwitch);
  current_switches.erase(mojo::PlatformChannel::kHandleSwitch);
  for (const auto& switch_value : current_switches)
    elevated_cmd.AppendSwitchNative(switch_value.first, switch_value.second);
  return elevated_cmd;
}

class ElevatingCleaner : public Cleaner {
 public:
  explicit ElevatingCleaner(Cleaner* real_cleaner)
      : decorated_cleaner_(real_cleaner) {}

  void Start(const std::vector<UwSId>& /*unused*/,
             DoneCallback done_callback) override {
    done_callback_ = std::move(done_callback);

    // Re-launch with administrator privileges.
    privileged_process_ =
        chrome_cleaner::HasAdminRights()
            ? base::LaunchProcess(GetElevatedCommandLine(),
                                  base::LaunchOptions())
            : LaunchElevatedProcessWithAssociatedWindow(
                  GetElevatedCommandLine(), GetForegroundChromeWindow());

    if (!privileged_process_.IsValid()) {
      ReportDone(RESULT_CODE_ELEVATION_PROMPT_DECLINED);
    } else {
      process_started_at_ = base::Time::Now();
      CheckDone();
    }
  }

  void StartPostReboot(const std::vector<UwSId>& /*unused*/,
                       DoneCallback /*unused*/) override {
    NOTIMPLEMENTED();
  }

  void Stop() override {
    if (privileged_process_.IsValid()) {
      privileged_process_.Terminate(/*exit_code=*/-1, /*wait=*/false);
      privileged_process_.Close();
    }
  }

  bool IsCompletelyDone() const override {
    return !privileged_process_.IsValid();
  }

  bool CanClean(const std::vector<UwSId>& pup_ids) override {
    return decorated_cleaner_->CanClean(pup_ids);
  }

 private:
  // Checks if the underlying process has finished and if not, schedules the
  // next check.
  void CheckDone() {
    // Not using blocking WaitForExit(...) method to be able to kill the process
    // with the above Stop() method should the need arise.
    int result;
    if (privileged_process_.WaitForExitWithTimeout(base::TimeDelta(),
                                                   &result)) {
      ReportDone(static_cast<ResultCode>(result));
      privileged_process_.Close();
    } else {
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&ElevatingCleaner::CheckDone, base::Unretained(this)),
          kCheckPeriod);
    }
  }

  // Reports result code of the underlying process.
  void ReportDone(ResultCode result) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(done_callback_), result));
  }

  Cleaner* decorated_cleaner_;
  DoneCallback done_callback_;

  base::Process privileged_process_;
  base::Time process_started_at_;
};

}  // namespace

ElevatingFacade::ElevatingFacade(std::unique_ptr<EngineFacadeInterface> facade)
    : decorated_facade_(std::move(facade)),
      cleaner_(
          std::make_unique<ElevatingCleaner>(decorated_facade_->GetCleaner())) {
}

ElevatingFacade::~ElevatingFacade() = default;

Scanner* ElevatingFacade::GetScanner() {
  return decorated_facade_->GetScanner();
}

Cleaner* ElevatingFacade::GetCleaner() {
  return cleaner_.get();
}

base::TimeDelta ElevatingFacade::GetScanningWatchdogTimeout() const {
  return decorated_facade_->GetScanningWatchdogTimeout();
}

}  // namespace chrome_cleaner
