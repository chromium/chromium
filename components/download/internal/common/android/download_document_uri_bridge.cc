// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/common/android/download_document_uri_bridge.h"

#include <string>

#include "base/android/content_uri_utils.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/check.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "components/download/internal/common/jni_headers/DownloadDocumentUriBridge_jni.h"
#include "components/download/public/common/download_interrupt_reasons.h"

namespace download {

// static
bool DownloadDocumentUriBridge::IsDocumentUri(const base::FilePath& uri) {
  if (!uri.IsContentUri()) {
    return false;
  }
  return Java_DownloadDocumentUriBridge_isDocumentUri(
      base::android::AttachCurrentThread(), uri.value());
}

// static
DownloadInterruptReason DownloadDocumentUriBridge::MoveFileToDocumentUri(
    const base::FilePath& source_path,
    const base::FilePath& destination_uri) {
  DCHECK(!source_path.IsContentUri());
  DCHECK(destination_uri.IsContentUri());

  if (base::CopyFile(source_path, destination_uri)) {
    if (!base::DeleteFile(source_path)) {
      DLOG(WARNING) << "Failed to delete temporary source file: "
                    << source_path;
    }
    return DOWNLOAD_INTERRUPT_REASON_NONE;
  }
  return DOWNLOAD_INTERRUPT_REASON_FILE_FAILED;
}

// static
void DownloadDocumentUriBridge::DeleteDocumentUri(
    const base::FilePath& document_uri) {
  DCHECK(document_uri.IsContentUri());
  if (!base::DeleteFile(document_uri)) {
    DLOG(WARNING) << "Failed to delete document URI: " << document_uri;
  }
}

// static
base::FilePath DownloadDocumentUriBridge::PublishDownload(
    const base::FilePath& document_uri) {
  DCHECK(document_uri.IsContentUri());
  return document_uri;
}

// static
base::File DownloadDocumentUriBridge::OpenDocumentUri(
    const base::FilePath& document_uri) {
  DCHECK(document_uri.IsContentUri());
  return base::File(document_uri, base::File::FLAG_OPEN_ALWAYS |
                                      base::File::FLAG_WRITE |
                                      base::File::FLAG_READ);
}

// static
base::FilePath DownloadDocumentUriBridge::GetDisplayName(
    const base::FilePath& document_uri) {
  DCHECK(document_uri.IsContentUri());
  std::u16string display_name;
  if (base::MaybeGetFileDisplayName(document_uri, &display_name)) {
    return base::FilePath(base::UTF16ToUTF8(display_name));
  }
  return base::FilePath();
}

// static
bool DownloadDocumentUriBridge::RenameDocumentUri(
    const base::FilePath& document_uri,
    const base::FilePath& new_display_name) {
  DCHECK(document_uri.IsContentUri());
  return Java_DownloadDocumentUriBridge_renameDocumentUri(
      base::android::AttachCurrentThread(), document_uri.value(),
      new_display_name.value());
}

}  // namespace download
