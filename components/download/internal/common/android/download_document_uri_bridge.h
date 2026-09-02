// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_COMMON_ANDROID_DOWNLOAD_DOCUMENT_URI_BRIDGE_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_COMMON_ANDROID_DOWNLOAD_DOCUMENT_URI_BRIDGE_H_

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "components/download/public/common/download_export.h"
#include "components/download/public/common/download_interrupt_reasons.h"

namespace download {

// Handles download operations targeting Android Storage Access Framework (SAF)
// Document URIs.
class COMPONENTS_DOWNLOAD_EXPORT DownloadDocumentUriBridge {
 public:
  DownloadDocumentUriBridge() = delete;
  DownloadDocumentUriBridge(const DownloadDocumentUriBridge&) = delete;
  DownloadDocumentUriBridge& operator=(const DownloadDocumentUriBridge&) =
      delete;

  // Returns whether the given `uri` is a Document URI.
  // Called on non UI thread.
  static bool IsDocumentUri(const base::FilePath& uri);

  // Copies the content from `source_path` to `destination_uri` and deletes
  // `source_path` on success.
  // Called on a background thread.
  static DownloadInterruptReason MoveFileToDocumentUri(
      const base::FilePath& source_path,
      const base::FilePath& destination_uri);

  // Deletes the document URI.
  // Called on a background thread.
  static void DeleteDocumentUri(const base::FilePath& document_uri);

  // Publishes the download. For SAF document URIs, this is a no-op returning
  // `document_uri`.
  // Called on a background thread.
  static base::FilePath PublishDownload(const base::FilePath& document_uri);

  // Opens the document URI for writing.
  // Called on a background thread.
  static base::File OpenDocumentUri(const base::FilePath& document_uri);

  // Gets the display name for a document URI.
  // Called on a background thread.
  static base::FilePath GetDisplayName(const base::FilePath& document_uri);

  // Renames the document URI with `new_display_name`. Returns true on success,
  // and false otherwise.
  // Called on a background thread.
  static bool RenameDocumentUri(const base::FilePath& document_uri,
                                const base::FilePath& new_display_name);
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_COMMON_ANDROID_DOWNLOAD_DOCUMENT_URI_BRIDGE_H_
