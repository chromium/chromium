// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/common/android/download_collection_bridge.h"

#include <set>
#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/files/file_util.h"
#include "components/download/public/common/download_interrupt_reasons.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/download/internal/common/jni_headers/DownloadCollectionBridge_jni.h"

using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace download {

namespace {
// Default value for expiration duration.
const int kDefaultExpirationDurationInDays = 3;

// Used by tests to simulate that a list of file names exists in the system.
// Must be used on main thread.
std::set<base::FilePath>* g_existing_file_names = nullptr;

}  // namespace
// static
base::FilePath DownloadCollectionBridge::CreateIntermediateUriForPublish(
    const GURL& original_url,
    const GURL& referrer_url,
    const base::FilePath& file_name,
    const std::string& mime_type) {
  std::string content_uri =
      Java_DownloadCollectionBridge_createIntermediateUriForPublish(
          base::android::AttachCurrentThread(), file_name.value(), mime_type,
          original_url.spec(), referrer_url.spec());
  return base::FilePath(content_uri);
}

// static
bool DownloadCollectionBridge::ShouldPublishDownload(
    const base::FilePath& file_path) {
  return Java_DownloadCollectionBridge_shouldPublishDownload(
      base::android::AttachCurrentThread(), file_path.value());
}

// static
DownloadInterruptReason DownloadCollectionBridge::MoveFileToIntermediateUri(
    const base::FilePath& source_path,
    const base::FilePath& destination_uri) {
  DCHECK(!source_path.IsContentUri());
  DCHECK(destination_uri.IsContentUri());

  bool success = Java_DownloadCollectionBridge_copyFileToIntermediateUri(
      base::android::AttachCurrentThread(), source_path.value(),
      destination_uri.value());
  base::DeleteFile(source_path);
  return success ? DOWNLOAD_INTERRUPT_REASON_NONE
                 : DOWNLOAD_INTERRUPT_REASON_FILE_FAILED;
}

// static
void DownloadCollectionBridge::DeleteIntermediateUri(
    const base::FilePath& intermediate_uri) {
  Java_DownloadCollectionBridge_deleteIntermediateUri(
      base::android::AttachCurrentThread(), intermediate_uri.value());
}

// static
base::FilePath DownloadCollectionBridge::PublishDownload(
    const base::FilePath& intermediate_uri) {
  std::string final_uri = Java_DownloadCollectionBridge_publishDownload(
      base::android::AttachCurrentThread(), intermediate_uri.value());
  return base::FilePath(final_uri);
}

// static
base::File DownloadCollectionBridge::OpenIntermediateUri(
    const base::FilePath& intermediate_uri) {
  int fd = Java_DownloadCollectionBridge_openIntermediateUri(
      base::android::AttachCurrentThread(), intermediate_uri.value());
  if (fd < 0)
    return base::File();
  return base::File(fd);
}

// static
bool DownloadCollectionBridge::FileNameExists(const base::FilePath& file_name) {
  if (g_existing_file_names) {
    return g_existing_file_names->find(file_name) !=
           g_existing_file_names->end();
  }
  return Java_DownloadCollectionBridge_fileNameExists(
      base::android::AttachCurrentThread(), file_name.value());
}

// static
bool DownloadCollectionBridge::RenameDownloadUri(
    const base::FilePath& download_uri,
    const base::FilePath& new_display_name) {
  return Java_DownloadCollectionBridge_renameDownloadUri(
      base::android::AttachCurrentThread(), download_uri.value(),
      new_display_name.value());
}

// static
void DownloadCollectionBridge::GetDisplayNamesForDownloads(
    GetDisplayNamesCallback cb) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobjectArray> jdisplay_infos =
      Java_DownloadCollectionBridge_getDisplayNamesForDownloads(env);
  auto result = std::make_unique<std::map<std::string, base::FilePath>>();
  if (!jdisplay_infos) {
    std::move(cb).Run(std::move(result));
    return;
  }
  for (auto jdisplay_info : jdisplay_infos.CreateView(env)) {
    std::string uri = Java_DisplayNameInfo_getDownloadUri(env, jdisplay_info);
    std::string display_name =
        Java_DisplayNameInfo_getDisplayName(env, jdisplay_info);
    if (!uri.empty() && !display_name.empty()) {
      result->emplace(uri, display_name);
    }
  }
  std::move(cb).Run(std::move(result));
}

// static
base::FilePath DownloadCollectionBridge::GetDisplayName(
    const base::FilePath& download_uri) {
  std::string display_name = Java_DownloadCollectionBridge_getDisplayName(
      base::android::AttachCurrentThread(), download_uri.value());
  return base::FilePath(display_name);
}

static int32_t JNI_DownloadCollectionBridge_GetExpirationDurationInDays(
    JNIEnv* env) {
  return kDefaultExpirationDurationInDays;
}

// static
void DownloadCollectionBridge::AddExistingFileNameForTesting(
    const base::FilePath& file_name) {
  g_existing_file_names->emplace(file_name);
}

// static
void DownloadCollectionBridge::ResetExistingFileNamesForTesting() {
  g_existing_file_names = new std::set<base::FilePath>();
}

}  // namespace download

DEFINE_JNI(DownloadCollectionBridge)
