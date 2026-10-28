// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_ATTACHMENT_UPLOADER_H_
#define COMPONENTS_SYNC_MODEL_ATTACHMENT_UPLOADER_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "base/types/expected.h"

class GURL;

namespace base {
class FilePath;
}  // namespace base

namespace syncer {

enum class AttachmentUploadError {
  kTransientError,
  kPermanentError,
};

// Interface for starting and resuming blob attachment uploads.
class AttachmentUploader {
 public:
  // Invoked once the initial upload handshake succeeds and returns a valid
  // resumable session `upload_url` (`upload_url.is_valid()` is guaranteed; if
  // the server returns a missing or malformed URL, `completion_callback` is
  // invoked with an `AttachmentUploadError` instead).
  using UploadUrlCallback = base::OnceCallback<void(const GURL& upload_url)>;

  // Invoked when the upload finishes with either the server-assigned
  // `temporary_blob_id` on success or an `AttachmentUploadError` on failure.
  using CompletionCallback = base::OnceCallback<void(
      base::expected<std::string /*temporary_blob_id*/,
                     AttachmentUploadError>)>;

  virtual ~AttachmentUploader() = default;

  // Starts a new upload for the local file at `file_path`. First invokes
  // `upload_url_callback` asynchronously once the resumable session URL is
  // created, and later invokes `completion_callback` asynchronously when the
  // upload finishes (or only `completion_callback` if the initial handshake
  // fails). Callbacks are guaranteed to never be invoked synchronously during
  // `StartUpload()`. Returns a closure that can be run to cancel the in-flight
  // upload.
  [[nodiscard]] virtual base::OnceClosure StartUpload(
      const base::FilePath& file_path,
      UploadUrlCallback upload_url_callback,
      CompletionCallback completion_callback) = 0;

  // Resumes an interrupted upload session at `upload_url` for the local file
  // at `file_path`, invoking `completion_callback` asynchronously when the
  // upload finishes (callbacks are guaranteed to never be invoked synchronously
  // during `ResumeUpload()`). Returns a closure that can be run to cancel the
  // in-flight upload.
  [[nodiscard]] virtual base::OnceClosure ResumeUpload(
      const base::FilePath& file_path,
      const GURL& upload_url,
      CompletionCallback completion_callback) = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_ATTACHMENT_UPLOADER_H_
