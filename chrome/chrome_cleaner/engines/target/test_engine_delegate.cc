// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/engines/target/test_engine_delegate.h"

#include <windows.h>

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/stl_util.h"
#include "base/strings/string_piece.h"
#include "base/task_runner_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/win/scoped_handle.h"
#include "chrome/chrome_cleaner/constants/uws_id.h"
#include "chrome/chrome_cleaner/engines/common/engine_result_codes.h"
#include "chrome/chrome_cleaner/engines/target/engine_file_requests_proxy.h"
#include "chrome/chrome_cleaner/engines/target/engine_scan_results_proxy.h"
#include "chrome/chrome_cleaner/logging/proto/shared_data.pb.h"
#include "chrome/chrome_cleaner/pup_data/pup_data.h"
#include "chrome/chrome_cleaner/pup_data/test_uws.h"

namespace chrome_cleaner {

namespace {

// Map of test UwS id to the corresponding file content to be matched.
// This is leaked to avoid the order-of-destruction problem of global statics.
static const auto& kTestUwSFileContent = *new std::map<UwSId, std::string>{
    {chrome_cleaner::kGoogleTestAUwSID, chrome_cleaner::kTestUwsAFileContents},
    {chrome_cleaner::kGoogleTestBUwSID, chrome_cleaner::kTestUwsBFileContents}};

// Scans for the UwS with id |uws_id| in a subfolder named "Startup" of the
// given |base_folder|. Returns an EngineResultCode. On success, sets
// |found_uws| to true if any UwS files were found, and also adds the file data
// to |pup| if it is not null.
uint32_t ScanStartupSubfolderForUwSWithId(
    const base::FilePath& base_folder,
    UwSId uws_id,
    scoped_refptr<EngineFileRequestsProxy> privileged_file_calls,
    bool* found_uws,
    PUPData::PUP* pup) {
  DCHECK(found_uws);

  const auto it = kTestUwSFileContent.find(uws_id);
  if (it == kTestUwSFileContent.end()) {
    // This stub implementation should only consider test UwS. No error, we
    // just don't find anything.
    return EngineResultCode::kSuccess;
  }
  const std::string uws_contents = it->second;

  base::FilePath folder = base_folder.Append(L"Startup");
  base::FilePath file_pattern = folder.Append(L"*.exe");

  WIN32_FIND_DATAW file_info;
  FindFileHandle find_handle;
  uint32_t status = privileged_file_calls->FindFirstFile(
      file_pattern, &file_info, &find_handle);
  if (status == ERROR_FILE_NOT_FOUND || status == ERROR_PATH_NOT_FOUND) {
    // Nothing found; no need to continue.
    return EngineResultCode::kSuccess;
  } else if (status != 0) {
    LOG(ERROR) << "FindFirstFile failed with status " << status;
    return EngineResultCode::kEngineInternal;
  }

  for (; status == 0;
       status = privileged_file_calls->FindNextFile(find_handle, &file_info)) {
    base::FilePath file_path = folder.Append(file_info.cFileName);
    base::win::ScopedHandle file_handle =
        privileged_file_calls->OpenReadOnlyFile(file_path, 0);
    if (!file_handle.IsValid()) {
      PLOG(ERROR) << "Failed to open file";
      continue;
    }

    std::vector<char> contents(file_info.nFileSizeLow);
    DWORD bytes_read = 0;
    if (!::ReadFile(file_handle.Get(), contents.data(), contents.size(),
                    &bytes_read, nullptr)) {
      PLOG(ERROR) << "Failed to read file";
      continue;
    }

    if (base::StringPiece(contents.data(), bytes_read) == uws_contents) {
      *found_uws = true;
      if (pup) {
        pup->AddDiskFootprint(file_path);
        pup->AddDiskFootprintTraceLocation(file_path,
                                           UwS_TraceLocation_FOUND_IN_SHELL);
      }
    }
  }
  LOG_IF(ERROR, status != ERROR_NO_MORE_FILES)
      << "find_next_file failed with status " << status;

  status = privileged_file_calls->FindClose(find_handle);
  LOG_IF(ERROR, status) << "find_close failed with status " << status;
  return EngineResultCode::kSuccess;
}

// Scans all supported locations for the UwS with id |uws_id|. Returns an
// EngineResultCode. On success, sets |found_uws| to true if any UwS files were
// found, and also adds the file data to |pup| if it is not null.
uint32_t ScanForUwSWithId(
    UwSId uws_id,
    scoped_refptr<EngineFileRequestsProxy> privileged_file_calls,
    bool* found_uws,
    PUPData::PUP* pup) {
  DCHECK(found_uws);

  // We can't retrieve the actual StartUp folder in the sandbox, and we won't
  // have access to find the current username either. So scan all users using
  // a hardcoded Start Menu location.
  // TODO(joenotcharles): The actual folder path may vary based on Windows
  // version and locale. We should find some way to get the correct system
  // Start Menu path even in the sandbox.
  base::FilePath base_folder = base::FilePath(L"C:\\Users");
  base::FilePath file_pattern = base_folder.Append(L"*");

  WIN32_FIND_DATAW file_info;
  FindFileHandle find_handle = 0;
  uint32_t status = privileged_file_calls->FindFirstFile(
      file_pattern, &file_info, &find_handle);
  if (status) {
    LOG_IF(ERROR,
           status != ERROR_FILE_NOT_FOUND && status != ERROR_PATH_NOT_FOUND)
        << "FindFirstFile failed with status " << status;
    return EngineResultCode::kEngineInternal;
  }

  for (; status == 0;
       status = privileged_file_calls->FindNextFile(find_handle, &file_info)) {
    if (!(file_info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
      continue;

    // Each subdirectory under Users is a user name.
    base::string16 user_name(file_info.cFileName);
    if (user_name == L"." || user_name == L"..")
      continue;
    base::FilePath folder = base_folder.Append(user_name)
                                .Append(L"AppData")
                                .Append(L"Roaming")
                                .Append(L"Microsoft")
                                .Append(L"Windows")
                                .Append(L"Start Menu")
                                .Append(L"Programs");
    uint32_t result = ScanStartupSubfolderForUwSWithId(
        folder, uws_id, privileged_file_calls, found_uws, pup);
    if (result != EngineResultCode::kSuccess)
      return result;
  }

  return EngineResultCode::kSuccess;
}

// Scans all supported locations that are enabled for all enabled UwS. Returns
// an EngineResultCode. On success, uses |report_result_calls| to report the
// UwSId's of all detected UwS. If |include_details| is true the reports will
// also include file details.
uint32_t ScanForUwS(
    const std::vector<UwSId>& enabled_uws,
    const std::vector<UwS::TraceLocation>& enabled_trace_locations,
    bool include_details,
    scoped_refptr<EngineFileRequestsProxy> privileged_file_calls,
    scoped_refptr<EngineScanResultsProxy> report_result_calls) {
  // Only check the Startup folder, assuming it's enabled
  if (!base::Contains(enabled_trace_locations,
                      UwS_TraceLocation_FOUND_IN_SHELL)) {
    return EngineResultCode::kSuccess;
  }

  for (const UwSId uws_id : enabled_uws) {
    PUPData::PUP pup;
    bool found_uws = false;
    uint32_t result =
        ScanForUwSWithId(uws_id, privileged_file_calls, &found_uws,
                         include_details ? &pup : nullptr);
    if (result != EngineResultCode::kSuccess)
      return result;
    if (found_uws)
      report_result_calls->FoundUwS(uws_id, pup);
  }

  return EngineResultCode::kSuccess;
}

uint32_t CleanUwS(
    const std::vector<UwSId>& enabled_uws,
    scoped_refptr<EngineFileRequestsProxy> privileged_file_calls,
    scoped_refptr<CleanerEngineRequestsProxy> privileged_removal_calls) {
  for (const UwSId uws_id : enabled_uws) {
    PUPData::PUP pup;
    bool found_uws = false;
    uint32_t result =
        ScanForUwSWithId(uws_id, privileged_file_calls, &found_uws, &pup);
    if (result != EngineResultCode::kSuccess)
      return result;
    if (found_uws) {
      for (const base::FilePath& file :
           pup.expanded_disk_footprints.file_paths()) {
        // Fall back to a post-reboot deletion if the delete fails.
        if (!privileged_removal_calls->DeleteFile(file) &&
            !privileged_removal_calls->DeleteFilePostReboot(file)) {
          return EngineResultCode::kCleaningFailed;
        }
      }
    }
  }
  return EngineResultCode::kSuccess;
}

void ScanDone(scoped_refptr<EngineFileRequestsProxy> /*privileged_file_calls*/,
              scoped_refptr<EngineRequestsProxy> /*privileged_scan_calls*/,
              scoped_refptr<EngineScanResultsProxy> report_result_calls,
              uint32_t result) {
  report_result_calls->ScanDone(result);
  // All proxies now go out of scope and can be deleted.
}

void CleanupDone(
    scoped_refptr<EngineFileRequestsProxy> /*privileged_file_calls*/,
    scoped_refptr<EngineRequestsProxy> /*privileged_scan_calls*/,
    scoped_refptr<CleanerEngineRequestsProxy> /*privileged_removal_calls*/,
    scoped_refptr<EngineCleanupResultsProxy> report_result_calls,
    uint32_t result) {
  report_result_calls->CleanupDone(result);
  // All proxies now go out of scope and can be deleted.
}

}  // namespace

TestEngineDelegate::TestEngineDelegate() = default;

TestEngineDelegate::~TestEngineDelegate() = default;

Engine::Name TestEngineDelegate::engine() const {
  return Engine::TEST_ONLY;
}

void TestEngineDelegate::Initialize(
    const base::FilePath& log_directory_path,
    scoped_refptr<EngineFileRequestsProxy> privileged_file_calls,
    mojom::EngineCommands::InitializeCallback done_callback) {
  DCHECK(!work_thread_);
  work_thread_ = std::make_unique<base::Thread>("TestEngineDelegate");
  uint32_t result = work_thread_->Start() ? EngineResultCode::kSuccess
                                          : EngineResultCode::kEngineInternal;
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(done_callback), result));
}

uint32_t TestEngineDelegate::StartScan(
    const std::vector<UwSId>& enabled_uws,
    const std::vector<UwS::TraceLocation>& enabled_trace_locations,
    bool include_details,
    scoped_refptr<EngineFileRequestsProxy> privileged_file_calls,
    scoped_refptr<EngineRequestsProxy> privileged_scan_calls,
    scoped_refptr<EngineScanResultsProxy> report_result_calls) {
  DCHECK(work_thread_);
  base::PostTaskAndReplyWithResult(
      work_thread_->task_runner().get(), FROM_HERE,
      base::BindOnce(&ScanForUwS, enabled_uws, enabled_trace_locations,
                     include_details, privileged_file_calls,
                     report_result_calls),
      base::BindOnce(&ScanDone, privileged_file_calls, privileged_scan_calls,
                     report_result_calls));
  return EngineResultCode::kSuccess;
}

uint32_t TestEngineDelegate::StartCleanup(
    const std::vector<UwSId>& enabled_uws,
    scoped_refptr<EngineFileRequestsProxy> privileged_file_calls,
    scoped_refptr<EngineRequestsProxy> privileged_scan_calls,
    scoped_refptr<CleanerEngineRequestsProxy> privileged_removal_calls,
    scoped_refptr<EngineCleanupResultsProxy> report_result_calls) {
  DCHECK(work_thread_);
  base::PostTaskAndReplyWithResult(
      work_thread_->task_runner().get(), FROM_HERE,
      base::BindOnce(&CleanUwS, enabled_uws, privileged_file_calls,
                     privileged_removal_calls),
      base::BindOnce(&CleanupDone, privileged_file_calls, privileged_scan_calls,
                     privileged_removal_calls, report_result_calls));
  return EngineResultCode::kSuccess;
}

uint32_t TestEngineDelegate::Finalize() {
  work_thread_.reset();
  return EngineResultCode::kSuccess;
}

}  // namespace chrome_cleaner
