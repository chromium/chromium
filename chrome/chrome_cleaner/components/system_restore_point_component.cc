// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/components/system_restore_point_component.h"

#include <stdint.h>
#include <windows.h>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/win/registry.h"
#include "base/win/windows_version.h"

namespace {

const wchar_t* kSystemRestoreKey =
    L"Software\\Microsoft\\Windows NT\\CurrentVersion\\SystemRestore";
const wchar_t* kSystemRestoreFrequencyWin8 =
    L"SystemRestorePointCreationFrequency";
const int64_t kInvalidSequenceNumber = -1;
// Name of the restore point library.
const wchar_t kRestorePointClientLibrary[] = L"srclient.dll";

}  // namespace

namespace chrome_cleaner {

SystemRestorePointComponent::SystemRestorePointComponent(
    const base::string16& product_fullname)
    : set_restore_point_info_fn_(nullptr),
      remove_restore_point_info_fn_(nullptr),
      sequence_number_(kInvalidSequenceNumber),
      product_fullname_(product_fullname) {
  base::NativeLibraryLoadError error;
  srclient_dll_ = base::LoadNativeLibrary(
      base::FilePath(kRestorePointClientLibrary), &error);
  if (!srclient_dll_) {
    PLOG(ERROR) << "Failed to load the restore point library, error="
                << error.code;
  } else {
    // Force the DLL to stay loaded until program termination.
    base::NativeLibrary module_handle = nullptr;
    if (!::GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN,
                              kRestorePointClientLibrary, &module_handle)) {
      PLOG(ERROR) << "Failed to pin the restore point library.";
      return;
    }
    DCHECK_EQ(srclient_dll_, module_handle);
    set_restore_point_info_fn_ = reinterpret_cast<SetRestorePointInfoWFn>(
        base::GetFunctionPointerFromNativeLibrary(srclient_dll_,
                                                  "SRSetRestorePointW"));
    remove_restore_point_info_fn_ = reinterpret_cast<RemoveRestorePointFn>(
        base::GetFunctionPointerFromNativeLibrary(srclient_dll_,
                                                  "SRRemoveRestorePoint"));
  }

  if (!set_restore_point_info_fn_)
    LOG(ERROR) << "Unable to find System Restore Point library.";
}

void SystemRestorePointComponent::PreScan() {}

void SystemRestorePointComponent::PostScan(
    const std::vector<UwSId>& found_pups) {}

void SystemRestorePointComponent::PreCleanup() {
  // It is not ok to call SRSetRestorePoint recursively. Make sure this is the
  // first call.
  DCHECK_EQ(sequence_number_, kInvalidSequenceNumber);
  if (!set_restore_point_info_fn_)
    return;

  // On Windows8, a registry value needs to be created in order for restore
  // points to be deterministically created. Attempt to create this value, but
  // continue with the restore point anyway even if doing so fails. See
  // http://msdn.microsoft.com/en-us/library/windows/desktop/aa378941.aspx for
  // more information.
  if (base::win::GetVersion() >= base::win::Version::WIN8) {
    base::win::RegKey system_restore_key(HKEY_LOCAL_MACHINE, kSystemRestoreKey,
                                         KEY_SET_VALUE | KEY_QUERY_VALUE);
    if (system_restore_key.Valid() &&
        !system_restore_key.HasValue(kSystemRestoreFrequencyWin8)) {
      system_restore_key.WriteValue(kSystemRestoreFrequencyWin8,
                                    static_cast<DWORD>(0));
    }
  }

  // Take a system restore point before doing anything else.
  RESTOREPOINTINFO restore_point_spec = {};
  STATEMGRSTATUS state_manager_status = {};

  restore_point_spec.dwEventType = BEGIN_SYSTEM_CHANGE;
  // MSDN documents few of the available values here. Use APPLICATION_INSTALL
  // since that seems closest from the documented ones. The header file
  // mentions a CHECKPOINT type which looks interesting, but let's stay on the
  // beaten path for now.
  restore_point_spec.dwRestorePtType = APPLICATION_INSTALL;
  restore_point_spec.llSequenceNumber = 0;
  wcsncpy(restore_point_spec.szDescription, product_fullname_.c_str(),
          base::size(restore_point_spec.szDescription));

  if (set_restore_point_info_fn_(&restore_point_spec, &state_manager_status)) {
    sequence_number_ = state_manager_status.llSequenceNumber;
  } else {
    if (state_manager_status.nStatus == ERROR_SERVICE_DISABLED) {
      LOG(WARNING) << "System Restore is disabled.";
    } else {
      LOG(ERROR) << "Failed to start System Restore service, error: "
                 << state_manager_status.nStatus;
    }
  }
}

void SystemRestorePointComponent::PostCleanup(ResultCode result_code,
                                              RebooterAPI* rebooter) {
  if (!set_restore_point_info_fn_ || sequence_number_ == kInvalidSequenceNumber)
    return;

  RESTOREPOINTINFO restore_point_spec = {};
  STATEMGRSTATUS state_manager_status = {};

  restore_point_spec.dwEventType = END_SYSTEM_CHANGE;
  restore_point_spec.llSequenceNumber = sequence_number_;

  if (result_code == RESULT_CODE_SUCCESS ||
      result_code == RESULT_CODE_PENDING_REBOOT ||
      result_code == RESULT_CODE_POST_REBOOT_SUCCESS ||
      result_code == RESULT_CODE_POST_REBOOT_ELEVATION_DENIED) {
    // Success! For now... Commit the restore point.
    restore_point_spec.dwRestorePtType = APPLICATION_INSTALL;

    if (!SetRestorePointInfoWrapper(&restore_point_spec,
                                    &state_manager_status)) {
      LOG(ERROR) << "Failed to commit System Restore point, error: "
                 << state_manager_status.nStatus;
    }
  } else {
    // No mutations were made, either because we found nothing, the user
    // canceled or an error occurred. Abort the restore point.
    restore_point_spec.dwRestorePtType = CANCELLED_OPERATION;

    if (!SetRestorePointInfoWrapper(&restore_point_spec,
                                    &state_manager_status)) {
      LOG(ERROR) << "Failed to cancel System Restore point, error: "
                 << state_manager_status.nStatus;
    }

    // I have observed, at least on Win8, that cancelling the restore point
    // still leaves it behind, so explicitly remove it as well.
    if (remove_restore_point_info_fn_ &&
        !RemoveRestorePointWrapper(sequence_number_)) {
      LOG(ERROR) << "Failed to remove cancelled Restore point.";
    }
  }
}

void SystemRestorePointComponent::PostValidation(ResultCode result_code) {}

void SystemRestorePointComponent::OnClose(ResultCode result_code) {}

bool SystemRestorePointComponent::IsLoadedRestorePointLibrary() {
  base::NativeLibrary module_handle = nullptr;
  if (!::GetModuleHandleExW(0, kRestorePointClientLibrary, &module_handle)) {
    PLOG(ERROR) << "Restore point library no longer present.";
    return false;
  }
  DCHECK_EQ(srclient_dll_, module_handle);
  return true;
}

bool SystemRestorePointComponent::SetRestorePointInfoWrapper(
    PRESTOREPOINTINFOW info,
    PSTATEMGRSTATUS status) {
  if (!set_restore_point_info_fn_ ||
      !SystemRestorePointComponent::IsLoadedRestorePointLibrary()) {
    return false;
  }
  return set_restore_point_info_fn_(info, status) != FALSE;
}

bool SystemRestorePointComponent::RemoveRestorePointWrapper(DWORD sequence) {
  if (!remove_restore_point_info_fn_ ||
      !SystemRestorePointComponent::IsLoadedRestorePointLibrary()) {
    return false;
  }
  return remove_restore_point_info_fn_(sequence) != FALSE;
}

}  // namespace chrome_cleaner
