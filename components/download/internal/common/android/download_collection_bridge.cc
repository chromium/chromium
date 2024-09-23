// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/common/android/download_collection_bridge.h"

#include <set>
#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/files/file_util.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "components/download/public/common/download_features.h"
#include "components/download/public/common/download_interrupt_reasons.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/download/internal/common/jni_headers/DownloadCollectionBridge_jni.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace download {

namespace {
// Default value for |kDownloadExpirationDurationFinchKey|, when no parameter is
// specified.
const int kDefaultExpirationDurationInDays = 3;

// Finch parameter key value of the duration in days for an intermediate
// download to expire.
constexpr char kDownloadExpirationDurationFinchKey[] = "expiration_duration";

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
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jurl =
      ConvertUTF8ToJavaString(env, original_url.spec());
  ScopedJavaLocalRef<jstring> jreferrer =
      ConvertUTF8ToJavaString(env, referrer_url.spec());
  ScopedJavaLocalRef<jstring> jfile_name =
      ConvertUTF16ToJavaString(env, file_name.AsUTF16Unsafe());
  ScopedJavaLocalRef<jstring> jmime_type =
      ConvertUTF8ToJavaString(env, mime_type);
  ScopedJavaLocalRef<jstring> jcontent_uri =
      Java_DownloadCollectionBridge_createIntermediateUriForPublish(
          env, jfile_name, jmime_type, jurl, jreferrer);
  if (jcontent_uri) {
    std::string content_uri = ConvertJavaStringToUTF8(env, jcontent_uri);
    return base::FilePath(content_uri);
  }
  return base::FilePath();
}

// static
bool DownloadCollectionBridge::ShouldPublishDownload(
    const base::FilePath& file_path) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jfile_path =
      ConvertUTF16ToJavaString(env, file_path.AsUTF16Unsafe());
  return Java_DownloadCollectionBridge_shouldPublishDownload(env, jfile_path);
}

// static
DownloadInterruptReason DownloadCollectionBridge::MoveFileToIntermediateUri(
    const base::FilePath& source_path,
    const base::FilePath& destination_uri) {
  DCHECK(!source_path.IsContentUri());
  DCHECK(destination_uri.IsContentUri());

  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jsource =
      ConvertUTF8ToJavaString(env, source_path.value());
  ScopedJavaLocalRef<jstring> jdestination =
      ConvertUTF8ToJavaString(env, destination_uri.value());
  bool success = Java_DownloadCollectionBridge_copyFileToIntermediateUri(
      env, jsource, jdestination);
  base::DeleteFile(source_path);
  return success ? DOWNLOAD_INTERRUPT_REASON_NONE
                 : DOWNLOAD_INTERRUPT_REASON_FILE_FAILED;
}

// static
void DownloadCollectionBridge::DeleteIntermediateUri(
    const base::FilePath& intermediate_uri) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> juri =
      ConvertUTF8ToJavaString(env, intermediate_uri.value());
  Java_DownloadCollectionBridge_deleteIntermediateUri(env, juri);
}

// static
base::FilePath DownloadCollectionBridge::PublishDownload(
    const base::FilePath& intermediate_uri) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jintermediate_uri =
      ConvertUTF8ToJavaString(env, intermediate_uri.value());
  ScopedJavaLocalRef<jstring> jfinal_uri =
      Java_DownloadCollectionBridge_publishDownload(env, jintermediate_uri);
  if (jfinal_uri) {
    std::string final_uri = ConvertJavaStringToUTF8(env, jfinal_uri);
    return base::FilePath(final_uri);
  }
  return base::FilePath();
}

// static
base::File DownloadCollectionBridge::OpenIntermediateUri(
    const base::FilePath& intermediate_uri) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jintermediate_uri =
      ConvertUTF8ToJavaString(env, intermediate_uri.value());
  int fd =
      Java_DownloadCollectionBridge_openIntermediateUri(env, jintermediate_uri);
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
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jfile_name =
      ConvertUTF8ToJavaString(env, file_name.value());
  return Java_DownloadCollectionBridge_fileNameExists(env, jfile_name);
}

// static
bool DownloadCollectionBridge::RenameDownloadUri(
    const base::FilePath& download_uri,
    const base::FilePath& new_display_name) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jdownload_uri =
      ConvertUTF8ToJavaString(env, download_uri.value());
  ScopedJavaLocalRef<jstring> jdisplay_name =
      ConvertUTF8ToJavaString(env, new_display_name.value());
  return Java_DownloadCollectionBridge_renameDownloadUri(env, jdownload_uri,
                                                         jdisplay_name);
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
  for (auto jdisplay_info : jdisplay_infos.ReadElements<jobject>()) {
    ScopedJavaLocalRef<jstring> juri =
        Java_DisplayNameInfo_getDownloadUri(env, jdisplay_info);
    ScopedJavaLocalRef<jstring> jdisplay_name =
        Java_DisplayNameInfo_getDisplayName(env, jdisplay_info);
    if (juri && jdisplay_name) {
      std::string uri = ConvertJavaStringToUTF8(env, juri);
      std::string display_name = ConvertJavaStringToUTF8(env, jdisplay_name);
      result->emplace(uri, display_name);
    }
  }
  std::move(cb).Run(std::move(result));
}

// static
base::FilePath DownloadCollectionBridge::GetDisplayName(
    const base::FilePath& download_uri) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jdownload_uri =
      ConvertUTF8ToJavaString(env, download_uri.value());
  ScopedJavaLocalRef<jstring> jdisplay_name =
      Java_DownloadCollectionBridge_getDisplayName(env, jdownload_uri);
  if (jdisplay_name) {
    std::string display_name = ConvertJavaStringToUTF8(env, jdisplay_name);
    return base::FilePath(display_name);
  }
  return base::FilePath();
}

jint JNI_DownloadCollectionBridge_GetExpirationDurationInDays(JNIEnv* env) {
  std::string finch_value = base::GetFieldTrialParamValueByFeature(
      features::kRefreshExpirationDate, kDownloadExpirationDurationFinchKey);
  int days;
  return base::StringToInt(finch_value, &days)
             ? days
             : kDefaultExpirationDurationInDays;
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
