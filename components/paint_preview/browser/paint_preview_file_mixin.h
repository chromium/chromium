// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAINT_PREVIEW_BROWSER_PAINT_PREVIEW_FILE_MIXIN_H_
#define COMPONENTS_PAINT_PREVIEW_BROWSER_PAINT_PREVIEW_FILE_MIXIN_H_

#include "base/files/file_path.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/paint_preview/browser/file_manager.h"

namespace paint_preview {

class PaintPreviewFileMixin {
 public:
  enum class ProtoReadStatus : int {
    kOk = 0,
    kNoProto,
    kDeserializationError,
    kExpired,
  };

  using OnReadProtoCallback =
      base::OnceCallback<void(ProtoReadStatus,
                              std::unique_ptr<PaintPreviewProto>)>;

  // Creates an instance for a profile. FileManager's root directory will be set
  // to |profile_dir|/paint_preview/|ascii_feature_name|.
  PaintPreviewFileMixin(const base::FilePath& profile_dir,
                        base::StringPiece ascii_feature_name);
  PaintPreviewFileMixin(const PaintPreviewFileMixin&) = delete;
  PaintPreviewFileMixin& operator=(const PaintPreviewFileMixin&) = delete;
  virtual ~PaintPreviewFileMixin();

  // Returns the file manager for the directory associated with the profile.
  scoped_refptr<FileManager> GetFileManager() { return file_manager_; }

  // Returns the task runner that IO tasks should be scheduled on.
  scoped_refptr<base::SequencedTaskRunner> GetTaskRunner() {
    return task_runner_;
  }

  // Acquires the PaintPreviewProto that is associated with |key| and sends it
  // to |on_read_proto_callback|. The default implementation attempts to invoke
  // GetFileManager()->DeserializePaintPreviewProto(). If |expiry_horizon| is
  // provided a proto that was last modified earlier than 'now - expiry_horizon'
  // will return the kExpired status.
  virtual void GetCapturedPaintPreviewProto(
      const DirectoryKey& key,
      base::Optional<base::TimeDelta> expiry_horizon,
      OnReadProtoCallback on_read_proto_callback);

 private:
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  scoped_refptr<FileManager> file_manager_;
};

}  // namespace paint_preview

#endif  // COMPONENTS_PAINT_PREVIEW_BROWSER_PAINT_PREVIEW_FILE_HELPER_H_
