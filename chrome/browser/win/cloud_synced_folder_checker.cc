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

#include <utility>

#include "base/base_paths_win.h"
#include "base/feature_list.h"
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

namespace features {
BASE_FEATURE(kCloudSyncedFolderChecker, base::FEATURE_ENABLED_BY_DEFAULT);
}  // namespace features

CloudSyncStatus::CloudSyncStatus() = default;

CloudSyncStatus::CloudSyncStatus(const CloudSyncStatus&) = default;

CloudSyncStatus& CloudSyncStatus::operator=(const CloudSyncStatus&) = default;

CloudSyncStatus::CloudSyncStatus(CloudSyncStatus&&) = default;

CloudSyncStatus& CloudSyncStatus::operator=(CloudSyncStatus&&) = default;

CloudSyncStatus::~CloudSyncStatus() = default;

CloudSyncStatus EvaluateOneDriveSyncStatus() {
  CloudSyncStatus status;

  base::FilePath one_drive_file_path;
  if (!base::PathService::Get(base::DIR_ONE_DRIVE, &one_drive_file_path) ||
      !IsCloudStorageSynced(one_drive_file_path)) {
    return status;
  }

  // OneDrive folder is synced.
  status.one_drive_path = base::MakeAbsoluteFilePath(one_drive_file_path);

  base::FilePath desktop_file_path;
  if (base::PathService::Get(base::DIR_USER_DESKTOP, &desktop_file_path)) {
    desktop_file_path = base::MakeAbsoluteFilePath(desktop_file_path);
    if (IsSubDirectoryOrEqual(desktop_file_path,
                              status.one_drive_path.value())) {
      status.desktop_path = std::move(desktop_file_path);
    }
  }

  base::FilePath documents_file_path;
  if (base::PathService::Get(chrome::DIR_USER_DOCUMENTS,
                             &documents_file_path)) {
    documents_file_path = base::MakeAbsoluteFilePath(documents_file_path);
    if (IsSubDirectoryOrEqual(documents_file_path,
                              status.one_drive_path.value())) {
      status.documents_path = std::move(documents_file_path);
    }
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
