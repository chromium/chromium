// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/windows_services/service_program/crash_reporting.h"

#include <objidl.h>
#include <wrl/client.h>

#include <string>
#include <utility>

#include "base/base_paths.h"
#include "base/check.h"
#include "base/debug/leak_annotations.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/version_info/channel.h"
#include "chrome/common/chrome_version.h"
#include "chrome/install_static/install_util.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/windows_services/service_program/is_running_unattended.h"
#include "chrome/windows_services/service_program/user_crash_state.h"
#include "components/crash/core/app/crash_reporter_client.h"
#include "components/crash/core/app/crashpad.h"
#include "components/crash/core/common/crash_key.h"

namespace {

// Returns the directory holding per-user subdirectories for the crashpad
// databases, or an empty path in case of error. `directory_name` is the name of
// the directory within C:\Windows\SystemTemp that will be created to house a
// "Crashpad" directory. Each process should provide a distinct value (e.g.,
// "ChromiumTracing" for the elevated tracing service in a Chromium build).
base::FilePath GetCrashpadDir(base::FilePath::StringViewType directory_name) {
  base::FilePath system_temp;
  if (!base::PathService::Get(base::DIR_SYSTEM_TEMP, &system_temp)) {
    return base::FilePath();
  }
  return system_temp.Append(directory_name)
      .Append(FILE_PATH_LITERAL("Crashpad"));
}

class CrashClient : public crash_reporter::CrashReporterClient {
 public:
  CrashClient(base::FilePath crashpad_dir,
              std::unique_ptr<UserCrashState> user_crash_state);
  ~CrashClient() override;

  const UserCrashState& user_crash_state() const { return *user_crash_state_; }

  // crash_reporter::CrashReporterClient:
  void GetProductNameAndVersion(const std::wstring& exe_path,
                                std::wstring* product_name,
                                std::wstring* version,
                                std::wstring* special_build,
                                std::wstring* channel_name) override;
  bool GetShouldDumpLargerDumps() override;
  bool GetCrashDumpLocation(std::wstring* crash_dir) override;
  bool IsRunningUnattended() override;
  bool GetCollectStatsConsent() override;
  bool GetCollectStatsInSample() override;
  bool ReportingIsEnforcedByPolicy(bool* reporting_enabled) override;

 private:
  // Returns the current value of the collect stats consent from the Windows
  // registry.
  bool GetCurrentCollectStatsConsent();
  bool GetCurrentCollectStatsInSample();
  void OnClientStateMediumChanged();
  void OnClientStateChanged();
  void OnProductKeyChanged();
  void UpdateUploadConsent();

  const base::FilePath crashpad_dir_;
  const std::unique_ptr<UserCrashState> user_crash_state_;
  bool last_collect_stats_consent_ = false;
  bool last_collect_stats_in_sample_ = false;
};

CrashClient::CrashClient(base::FilePath crashpad_dir,
                         std::unique_ptr<UserCrashState> user_crash_state)
    : crashpad_dir_(std::move(crashpad_dir)),
      user_crash_state_(std::move(user_crash_state)) {
  if (user_crash_state_->client_state_medium_key().Valid()) {
    // Unretained is safe because this owns the registry key by way of
    // UserCrashState.
    user_crash_state_->client_state_medium_key().StartWatching(base::BindOnce(
        &CrashClient::OnClientStateMediumChanged, base::Unretained(this)));
  }
  if (user_crash_state_->client_state_key().Valid()) {
    // Unretained is safe because this owns the registry key by way of
    // UserCrashState.
    user_crash_state_->client_state_key().StartWatching(base::BindOnce(
        &CrashClient::OnClientStateChanged, base::Unretained(this)));
  }
  if (user_crash_state_->product_key().Valid()) {
    // Unretained is safe because this owns the registry key by way of
    // UserCrashState.
    user_crash_state_->product_key().StartWatching(base::BindOnce(
        &CrashClient::OnProductKeyChanged, base::Unretained(this)));
  }
}

CrashClient::~CrashClient() = default;

void CrashClient::GetProductNameAndVersion(const std::wstring& exe_path,
                                           std::wstring* product_name,
                                           std::wstring* version,
                                           std::wstring* special_build,
                                           std::wstring* channel_name) {
  // Report crashes under the same product name as the browser. This string
  // MUST match server-side configuration.
  *product_name = base::ASCIIToWide(PRODUCT_SHORTNAME_STRING);
  *version = base::ASCIIToWide(CHROME_VERSION_STRING);
  special_build->clear();
  *channel_name =
      install_static::GetChromeChannelName(/*with_extended_stable=*/true);
}

bool CrashClient::GetShouldDumpLargerDumps() {
  // Capture larger dumps for Google Chrome beta, dev, and canary channels, and
  // Chromium builds. The Google Chrome stable channel uses smaller dumps.
  return install_static::GetChromeChannel() != version_info::Channel::STABLE;
}

bool CrashClient::GetCrashDumpLocation(std::wstring* crash_dir) {
  *crash_dir = crashpad_dir_.Append(user_crash_state_->user_sid()).value();
  return true;
}

bool CrashClient::IsRunningUnattended() {
  return internal::IsRunningUnattended();
}

bool CrashClient::GetCollectStatsConsent() {
  // Cache the last returned value for use when handling a change to one of the
  // registry keys.
  last_collect_stats_consent_ = GetCurrentCollectStatsConsent();
  return last_collect_stats_consent_;
}

bool CrashClient::GetCollectStatsInSample() {
  // Cache the last returned value for use when handling a change to one of the
  // registry keys.
  last_collect_stats_in_sample_ = GetCurrentCollectStatsInSample();
  return last_collect_stats_in_sample_;
}

bool CrashClient::ReportingIsEnforcedByPolicy(bool* reporting_enabled) {
  return install_static::ReportingIsEnforcedByPolicy(reporting_enabled);
}

bool CrashClient::GetCurrentCollectStatsConsent() {
  DWORD value = 0;

  // TODO(crbug.com/40837274): The logic here is based on Chrome's behavior of
  // conveying consent to Omaha via the value in HKLM\...\ClientStateMedium.
  // This must be updated if Chrome and Omaha switch to using a value in HKCU.

  // First check for a value in ClientStateMedium. A value here for a
  // per-machine install takes precedent over one in ClientState.
  if (user_crash_state_->client_state_medium_key().ReadValueDW(
          google_update::kRegUsageStatsField, &value) == ERROR_SUCCESS) {
    return value == google_update::TRISTATE_TRUE;
  }

  // Failing that, check for one in ClientState.
  if (user_crash_state_->client_state_key().ReadValueDW(
          google_update::kRegUsageStatsField, &value) == ERROR_SUCCESS) {
    return value == google_update::TRISTATE_TRUE;
  }

  // No value anywhere means no consent.
  return false;
}

bool CrashClient::GetCurrentCollectStatsInSample() {
  DWORD value = 0;

  // Failure to read a value (e.g., because the value is not present) is
  // considered "in the sample".
  return user_crash_state_->product_key().ReadValueDW(
             install_static::kRegValueChromeStatsSample, &value) !=
             ERROR_SUCCESS ||
         value == 1;
}

void CrashClient::OnClientStateMediumChanged() {
  // Unretained is safe because this owns the registry key by way of
  // UserCrashState.
  user_crash_state_->client_state_medium_key().StartWatching(base::BindOnce(
      &CrashClient::OnClientStateMediumChanged, base::Unretained(this)));

  UpdateUploadConsent();
}

void CrashClient::OnClientStateChanged() {
  // Unretained is safe because this owns the registry key by way of
  // UserCrashState.
  user_crash_state_->client_state_key().StartWatching(base::BindOnce(
      &CrashClient::OnClientStateChanged, base::Unretained(this)));

  UpdateUploadConsent();
}

void CrashClient::OnProductKeyChanged() {
  // Unretained is safe because this owns the registry key by way of
  // UserCrashState.
  user_crash_state_->product_key().StartWatching(base::BindOnce(
      &CrashClient::OnProductKeyChanged, base::Unretained(this)));

  UpdateUploadConsent();
}

void CrashClient::UpdateUploadConsent() {
  bool collect_stats_consent = GetCurrentCollectStatsConsent();
  bool collect_stats_in_sample = GetCurrentCollectStatsInSample();
  if (collect_stats_consent != last_collect_stats_consent_ ||
      collect_stats_in_sample != last_collect_stats_in_sample_) {
    last_collect_stats_consent_ = collect_stats_consent;
    crash_reporter::SetUploadConsent(collect_stats_consent);
  }
}

void StartCrashHandlerImpl(std::unique_ptr<UserCrashState> user_crash_state,
                           base::FilePath::StringViewType directory_name,
                           std::string_view process_type) {
  // The child process requires that the directory holding the client's "crash
  // dump location" already exists, so create it now before launching the child.
  base::FilePath crashpad_dir = GetCrashpadDir(directory_name);
  if (crashpad_dir.empty() || !base::CreateDirectory(crashpad_dir)) {
    return;
  }

  // Only one crash handler may be created for the lifetime of a process.
  // Allow multiple calls to `StartCrashHandlerImpl`, but ensure that they are
  // all on behalf of the same user.
  static auto* const client =
      new CrashClient(std::move(crashpad_dir), std::move(user_crash_state));
  ANNOTATE_LEAKING_OBJECT_PTR(client);

  // On the first call, ownership of `user_crash_state` will have been passed to
  // the client.
  if (user_crash_state) {
    // This is not the first call. Crash if an attempt is being made to start a
    // handler for a different user.
    CHECK(client->user_crash_state().user_sid() ==
          user_crash_state->user_sid());
    // This is for the same user as the previous launch, so there is nothing
    // more to be done.
    return;
  }

  // Disable COM exception handling as per
  // https://learn.microsoft.com/en-us/windows/win32/api/objidl/nn-objidl-iglobaloptions.
  if (Microsoft::WRL::ComPtr<IGlobalOptions> options; SUCCEEDED(
          ::CoCreateInstance(CLSID_GlobalOptions, nullptr, CLSCTX_INPROC_SERVER,
                             IID_PPV_ARGS(&options)))) {
    options->Set(COMGLB_EXCEPTION_HANDLING, COMGLB_EXCEPTION_DONOT_HANDLE);
  }

  crash_reporter::SetCrashReporterClient(client);
  crash_reporter::InitializeCrashKeys();

  crash_reporter::InitializeCrashpadWithEmbeddedHandler(
      /*initial_client=*/true, std::string(process_type),
      /*user_data_dir=*/std::string(),
      /*exe_path=*/base::FilePath());
}

}  // namespace

namespace windows_services {

void StartCrashHandler(std::unique_ptr<UserCrashState> user_crash_state,
                       base::FilePath::StringViewType directory_name,
                       std::string_view process_type,
                       scoped_refptr<base::SequencedTaskRunner> task_runner) {
  if (task_runner) {
    base::WaitableEvent event;
    task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](std::unique_ptr<UserCrashState> user_crash_state,
               base::FilePath::StringViewType directory_name,
               std::string_view process_type, base::WaitableEvent& event) {
              StartCrashHandlerImpl(std::move(user_crash_state), directory_name,
                                    process_type);
              event.Signal();
            },
            std::move(user_crash_state), directory_name, process_type,
            std::ref(event)));
    event.Wait();
    return;
  }
  StartCrashHandlerImpl(std::move(user_crash_state), directory_name,
                        process_type);
}

}  // namespace windows_services
