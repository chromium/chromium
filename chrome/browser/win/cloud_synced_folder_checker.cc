// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/cloud_synced_folder_checker.h"

#include <shlobj.h>
// NOTE: MUST be included below shlobj.h
#include <propkey.h>
#include <windows.storage.h>
#include <wrl/client.h>
#include <wrl/implements.h>

#include "base/base_paths_win.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/threading/scoped_thread_priority.h"
#include "chrome/common/chrome_paths.h"

namespace {
// Returns whether filepath `a` is a subdirectory of or is filepath `b`.
bool IsSubDirectoryOrEqual(const base::FilePath& a, const base::FilePath& b) {
  return a == b || b.IsParent(a);
}
}  // namespace

namespace cloud_synced_folder_checker {

CloudSyncStatus EvaluateOneDriveSyncStatus() {
  CloudSyncStatus status;

  base::FilePath one_drive_file_path;
  if (!base::PathService::Get(base::DIR_ONE_DRIVE, &one_drive_file_path) ||
      !IsCloudStorageSynced(one_drive_file_path)) {
    return status;
  }

  // OneDrive folder is synced.
  status.synced = true;

  one_drive_file_path = base::MakeAbsoluteFilePath(one_drive_file_path);

  base::FilePath desktop_file_path;
  if (base::PathService::Get(base::DIR_USER_DESKTOP, &desktop_file_path)) {
    status.desktop_synced = IsSubDirectoryOrEqual(
        base::MakeAbsoluteFilePath(desktop_file_path), one_drive_file_path);
  }

  base::FilePath documents_file_path;
  if (base::PathService::Get(chrome::DIR_USER_DOCUMENTS,
                             &documents_file_path)) {
    documents_file_path = base::MakeAbsoluteFilePath(documents_file_path);
    status.documents_synced = IsSubDirectoryOrEqual(
        base::MakeAbsoluteFilePath(documents_file_path), one_drive_file_path);
  }

  return status;
}

bool IsCloudStorageSynced(const base::FilePath& file_path) {
  // Because SHCreateItemFromParsingName can load DLLS, we wrap it with a
  // background priority scope.
  SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();

  Microsoft::WRL::ComPtr<IShellItem2> shell_item;
  HRESULT hr = SHCreateItemFromParsingName(file_path.value().c_str(), nullptr,
                                           IID_PPV_ARGS(&shell_item));
  if (FAILED(hr) || !shell_item) {
    return false;
  }

  Microsoft::WRL::ComPtr<IPropertyStore> property_store;
  hr = shell_item->GetPropertyStore(GPS_DEFAULT, IID_PPV_ARGS(&property_store));
  if (FAILED(hr) || !property_store) {
    return false;
  }

  PROPVARIANT prop_value;
  PropVariantInit(&prop_value);

  // Retrieve the PKEY_StorageProviderState property which will indicate that
  // `file_path` is managed.
  hr = property_store->GetValue(PKEY_StorageProviderState, &prop_value);
  if (SUCCEEDED(hr) && prop_value.vt == VT_UI4) {
    return true;
  }

  PropVariantClear(&prop_value);
  return false;
}

}  // namespace cloud_synced_folder_checker
