// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/components/recovery_component.h"

#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/message_loop/message_pump_type.h"
#include "base/numerics/safe_conversions.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/chrome_cleaner/buildflags.h"
#include "chrome/chrome_cleaner/components/component_unpacker.h"
#include "chrome/chrome_cleaner/constants/chrome_cleaner_switches.h"
#include "chrome/chrome_cleaner/http/http_agent.h"
#include "chrome/chrome_cleaner/http/http_agent_factory.h"
#include "chrome/chrome_cleaner/http/http_response.h"
#include "chrome/chrome_cleaner/http/http_status_codes.h"
#include "chrome/chrome_cleaner/pup_data/pup_data.h"

namespace chrome_cleaner {

namespace {

const char kComponentDownloadUrl[] =
    "https://clients2.google.com/service/update2/crx?response=redirect&os=win"
    "&arch=x86&installsource=swreporter&x=id%3Dnpdjjkjlcidkjlamlmmdelcjbcpdjocm"
    "%26v%3D0.0.0.0%26uc&acceptformat=crx3";

// CRX hash. The extension id is: npdjjkjlcidkjlamlmmdelcjbcpdjocm.
const uint8_t kSha2Hash[] = {0xdf, 0x39, 0x9a, 0x9b, 0x28, 0x3a, 0x9b, 0x0c,
                             0xbc, 0xc3, 0x4b, 0x29, 0x12, 0xf3, 0x9e, 0x2c,
                             0x19, 0x7a, 0x71, 0x4b, 0x0a, 0x7c, 0x80, 0x1c,
                             0xf6, 0x29, 0x7c, 0x0a, 0x5f, 0xea, 0x67, 0xb7};

// Name of the executable file as well as the command line arg to use when run
// from the Chrome Cleanup tool.
const wchar_t kChromeRecoveryExe[] = L"ChromeRecovery.exe";
const char kChromeRecoveryArg[] = "/installsource swreporter";

const int kDownloadCrxWaitTimeInMin = 2;
const int kExecutionCrxWaitTimeInMin = 1;

const HttpAgentFactory* current_http_agent_factory{nullptr};

constexpr net::NetworkTrafficAnnotationTag kComponentDownloadTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("download_recovery_component", R"(
          semantics {
            sender: "Chrome Cleanup"
            description:
              "Chrome on Windows is able to detect and remove software that "
              "violates Google's Unwanted Software Policy "
              "(https://www.google.com/about/unwanted-software-policy.html). "
              "When potentially unwanted software is detected and the user "
              "accepts Chrome's offer to remove it, as part of the cleanup "
              "Chrome sends a request to Google to download the Chrome "
              "Recovery component, which can repair the Chrome update system "
              "to ensure that unwanted software does not block Chrome from "
              "getting security updates."
            trigger:
              "The user either accepted a prompt to remove unwanted software, "
              "or went to \"Clean up computer\" in the settings page and chose "
              "to \"Find harmful software\"."
            data: "None"
            destination: GOOGLE_OWNED_SERVICE
          }
          policy {
            cookies_allowed: NO
            setting:
              "Chrome Cleanup is offered in \"Reset and clean up\" in settings "
              "under Advanced and never happens without explicit user consent. "
            chrome_policy {
              ChromeCleanupEnabled {
                ChromeCleanupEnabled: false
              }
            }
          }
          )");

bool SaveHttpResponseDataToFile(const base::FilePath& file_path,
                                chrome_cleaner::HttpResponse* response) {
  base::File file(file_path,
                  base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);

  uint32_t count = RecoveryComponent::kReadDataFromResponseBufferSize;
  std::vector<char> buffer(count);
  while (true) {
    if (!response->ReadData(buffer.data(), &count)) {
      LOG(ERROR) << "ReadData failed";
      return false;
    } else if (!count) {
      break;
    }

    if (file.WriteAtCurrentPos(buffer.data(), base::checked_cast<int>(count)) ==
        -1) {
      PLOG(ERROR) << "WriteAtCurrentPos";
      return false;
    }
  }

  return true;
}

const HttpAgentFactory* GetHttpAgentFactory() {
  // This is "leaked" on purpose to avoid static destruction order woes.
  // Neither HttpAgentFactory nor its parent classes dtors do any work.
  static HttpAgentFactory* http_agent_factory = new HttpAgentFactory();

  if (!current_http_agent_factory) {
    current_http_agent_factory = http_agent_factory;
  }

  return current_http_agent_factory;
}

}  // namespace

// static
bool RecoveryComponent::IsAvailable() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
// Only add the recovery component in official builds, unless it's forced, and
// not if it's explicitly disabled.
#if BUILDFLAG(IS_OFFICIAL_CHROME_CLEANER_BUILD)
  return !command_line->HasSwitch(kNoRecoveryComponentSwitch);
#else
  return command_line->HasSwitch(kForceRecoveryComponentSwitch);
#endif
}

RecoveryComponent::RecoveryComponent()
    : recovery_io_thread_("RecoveryComponentIO"),
      done_expanding_crx_(base::WaitableEvent::ResetPolicy::MANUAL,
                          base::WaitableEvent::InitialState::NOT_SIGNALED) {}

// static
void RecoveryComponent::SetHttpAgentFactoryForTesting(
    const HttpAgentFactory* factory) {
  current_http_agent_factory = factory;
}

void RecoveryComponent::PreScan() {
  bool success = recovery_io_thread_.StartWithOptions(
      base::Thread::Options(base::MessagePumpType::IO, 0));
  DCHECK(success) << "Can't start File Thread!";

  recovery_io_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&RecoveryComponent::FetchOnIOThread,
                                base::Unretained(this)));
}

void RecoveryComponent::PostScan(const std::vector<UwSId>& found_pups) {
  // If there won't be any cleanup, then run the recovery component now.
  if (!PUPData::HasFlaggedPUP(found_pups, &PUPData::HasRemovalFlag)) {
    Run();
  }
}

void RecoveryComponent::PreCleanup() {}

void RecoveryComponent::PostCleanup(ResultCode result_code,
                                    RebooterAPI* rebooter) {
  if (result_code == RESULT_CODE_PENDING_REBOOT) {
    LOG(INFO) << "Not executing ChromeRecovery before reboot.";
    return;
  }
  Run();
}

void RecoveryComponent::PostValidation(ResultCode result_code) {
  PreScan();
  Run();
}

void RecoveryComponent::Run() {
  DCHECK(!ran_);
  ran_ = true;
  // We must make sure that the crx expansion is complete.
  if (!done_expanding_crx_.TimedWait(
          base::TimeDelta::FromMinutes(kDownloadCrxWaitTimeInMin))) {
    LOG(WARNING) << "Timed out waiting for crx expansion completion.";
    return;
  }

  if (!component_path_.IsValid()) {
    LOG(WARNING) << "No access to the component path.";
    return;
  }

  base::FilePath chrome_recovery(
      component_path_.GetPath().Append(kChromeRecoveryExe));
  DCHECK(base::PathExists(chrome_recovery));

  base::CommandLine recovery_command_line(chrome_recovery);
  recovery_command_line.AppendArg(kChromeRecoveryArg);
  base::Process recovery_process =
      base::LaunchProcess(recovery_command_line, base::LaunchOptions());
  if (!recovery_process.IsValid()) {
    LOG(WARNING) << "Failed to launch " << kChromeRecoveryExe << " "
                 << kChromeRecoveryArg;
    return;
  }

  int exit_code = -1;
  bool success = recovery_process.WaitForExitWithTimeout(
      base::TimeDelta::FromMinutes(kExecutionCrxWaitTimeInMin), &exit_code);
  LOG_IF(INFO, success) << "ChromeRecovery returned code: " << exit_code;
  PLOG_IF(ERROR, !success) << "ChromeRecovery failed to start in time.";
}

void RecoveryComponent::OnClose(ResultCode result_code) {}

void RecoveryComponent::UnpackComponent(const base::FilePath& crx_file) {
  std::vector<uint8_t> pk_hash;
  pk_hash.assign(kSha2Hash, &kSha2Hash[sizeof(kSha2Hash)]);

  ComponentUnpacker unpacker(pk_hash, crx_file);
  bool success = unpacker.Unpack(component_path_.GetPath());
  DCHECK(success) << "Failed to unpack component.";
}

void RecoveryComponent::FetchOnIOThread() {
  DCHECK(recovery_io_thread_.task_runner()->BelongsToCurrentThread());
  std::unique_ptr<chrome_cleaner::HttpAgent> http_agent =
      GetHttpAgentFactory()->CreateHttpAgent();

  LOG(INFO) << "Sending request to download Recovery Component.";
  const GURL download_url(kComponentDownloadUrl);
  std::unique_ptr<chrome_cleaner::HttpResponse> http_response = http_agent->Get(
      base::UTF8ToWide(download_url.host()),
      base::checked_cast<uint16_t>(download_url.EffectiveIntPort()),
      base::UTF8ToWide(download_url.PathForRequest()),
      download_url.SchemeIsCryptographic(),
      L"",  // No extra headers.
      kComponentDownloadTrafficAnnotation);

  // Make sure to signal the event when this method returns.
  base::ScopedClosureRunner set_event(
      base::BindOnce(base::IgnoreResult(&base::WaitableEvent::Signal),
                     base::Unretained(&done_expanding_crx_)));

  if (!http_response.get()) {
    LOG(WARNING) << "Recovery Component failed to download (no response)";
    return;
  }

  uint16_t status_code = 0;
  if (!http_response->GetStatusCode(&status_code) ||
      status_code != static_cast<uint16_t>(HttpStatus::kOk)) {
    LOG(WARNING) << "Recovery Component failed to download. Response: "
                 << status_code;
    return;
  }

  LOG(INFO) << "Recovery Component successfully downloaded.";

  base::FilePath crx_file;
  if (!base::CreateTemporaryFile(&crx_file)) {
    LOG(ERROR) << "Failed to create temporary file to save crx";
    return;
  }

  base::ScopedClosureRunner delete_file(
      base::BindOnce(base::IgnoreResult(&base::DeleteFile), crx_file, false));

  if (!SaveHttpResponseDataToFile(crx_file, http_response.get())) {
    LOG(WARNING) << "Failed to save downloaded recovery component";
    return;
  }

  if (component_path_.CreateUniqueTempDir())
    UnpackComponent(crx_file);
  else
    NOTREACHED() << "Couldn't create a temp dir?";
}

}  // namespace chrome_cleaner
