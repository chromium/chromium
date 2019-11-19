// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/syncable/on_disk_directory_backing_store.h"

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"

namespace syncer {
namespace syncable {

namespace {

enum HistogramResultEnum {
  FIRST_TRY_SUCCESS,
  SECOND_TRY_SUCCESS,
  SECOND_TRY_FAILURE,
  RESULT_COUNT
};

}  // namespace

OnDiskDirectoryBackingStore::OnDiskDirectoryBackingStore(
    const std::string& dir_name,
    const base::RepeatingCallback<std::string()>& cache_guid_generator,
    const base::FilePath& backing_file_path)
    : DirectoryBackingStore(dir_name, cache_guid_generator),
      backing_file_path_(backing_file_path) {
  DCHECK(backing_file_path_.IsAbsolute());
}

OnDiskDirectoryBackingStore::~OnDiskDirectoryBackingStore() {}

DirOpenResult OnDiskDirectoryBackingStore::TryLoad(
    Directory::MetahandlesMap* handles_map,
    MetahandleSet* metahandles_to_purge,
    Directory::KernelLoadInfo* kernel_load_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!IsOpen()) {
    if (!Open(backing_file_path_))
      return FAILED_OPEN_DATABASE;
  }

  bool did_start_new = false;
  if (!InitializeTables(&did_start_new))
    return FAILED_OPEN_DATABASE;

  if (!LoadEntries(handles_map, metahandles_to_purge))
    return FAILED_DATABASE_CORRUPT;
  if (!LoadInfo(kernel_load_info))
    return FAILED_DATABASE_CORRUPT;
  if (!VerifyReferenceIntegrity(handles_map))
    return FAILED_DATABASE_CORRUPT;

  return did_start_new ? OPENED_NEW : OPENED_EXISTING;
}

DirOpenResult OnDiskDirectoryBackingStore::Load(
    Directory::MetahandlesMap* handles_map,
    MetahandleSet* metahandles_to_purge,
    Directory::KernelLoadInfo* kernel_load_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DirOpenResult result =
      TryLoad(handles_map, metahandles_to_purge, kernel_load_info);
  if (result == OPENED_NEW || result == OPENED_EXISTING) {
    UMA_HISTOGRAM_ENUMERATION("Sync.DirectoryOpenResult", FIRST_TRY_SUCCESS,
                              RESULT_COUNT);
    return result;
  }

  ReportFirstTryOpenFailure();

  // The fallback: delete the current database and return a fresh one.  We can
  // fetch the user's data from the cloud.
  handles_map->clear();

  ResetAndCreateConnection();

  base::DeleteFile(backing_file_path_, false);

  result = TryLoad(handles_map, metahandles_to_purge, kernel_load_info);
  if (result == OPENED_NEW || result == OPENED_EXISTING) {
    UMA_HISTOGRAM_ENUMERATION("Sync.DirectoryOpenResult", SECOND_TRY_SUCCESS,
                              RESULT_COUNT);
  } else {
    UMA_HISTOGRAM_ENUMERATION("Sync.DirectoryOpenResult", SECOND_TRY_FAILURE,
                              RESULT_COUNT);
  }

  return result;
}

void OnDiskDirectoryBackingStore::ReportFirstTryOpenFailure() {
  // In debug builds, the last thing we want is to silently clear the database.
  // It's full of evidence that might help us determine what went wrong.  It
  // might be sqlite's fault, but it could also be a bug in sync.  We crash
  // immediately so a developer can investigate.
  //
  // Developers: If you're not interested in debugging this right now, just move
  // aside the 'Sync Data' directory in your profile.  This is similar to what
  // the code would do if this DCHECK were disabled.
  NOTREACHED() << "Crashing to preserve corrupt sync database";
}

const base::FilePath& OnDiskDirectoryBackingStore::backing_file_path() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return backing_file_path_;
}

}  // namespace syncable
}  // namespace syncer
