// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAINT_PREVIEW_BROWSER_PAINT_PREVIEW_FILE_MIXIN_H_
#define COMPONENTS_PAINT_PREVIEW_BROWSER_PAINT_PREVIEW_FILE_MIXIN_H_

#include <optional>
#include <string_view>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/paint_preview/browser/file_manager.h"
#include "ui/accessibility/ax_tree_update.h"

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

  using OnReadAXTree =
      base::OnceCallback<void(std::unique_ptr<ui::AXTreeUpdate>)>;

  // Creates an instance for a profile. FileManager's root directory will be set
  // to |profile_dir|/paint_preview/|ascii_feature_name|.
  PaintPreviewFileMixin(const base::FilePath& profile_dir,
                        std::string_view ascii_feature_name);
  PaintPreviewFileMixin(const PaintPreviewFileMixin&) = delete;
  PaintPreviewFileMixin& operator=(const PaintPreviewFileMixin&) = delete;
  virtual ~PaintPreviewFileMixin();

  // Returns the file manager for the directory associated with the profile.
  scoped_refptr<FileManager> GetFileManager() { return file_manager_; }

  // Returns the task runner that IO tasks should be scheduled on.
  scoped_refptr<base::SequencedTaskRunner> GetTaskRunner() {
    return task_runner_;
  }

  base::WeakPtr<PaintPreviewFileMixin> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // Acquires the PaintPreviewProto that is associated with |key| and sends it
  // to |on_read_proto_callback|. The default implementation attempts to invoke
  // GetFileManager()->DeserializePaintPreviewProto(). If |expiry_horizon| is
  // provided a proto that was last modified earlier than 'now - expiry_horizon'
  // will return the kExpired status.
  virtual void GetCapturedPaintPreviewProto(
      const DirectoryKey& key,
      std::optional<base::TimeDelta> expiry_horizon,
      OnReadProtoCallback on_read_proto_callback);

  // Writes an Accessibility Tree snapshot to the directory listed in key.
  void WriteAXTreeUpdate(const DirectoryKey& key,
                         base::OnceCallback<void(bool)> finished_callback,
                         ui::AXTreeUpdate& ax_tree_update);

  // Gets an Accessibility Tree snapshot for key.
  void GetAXTreeUpdate(const DirectoryKey& key, OnReadAXTree callback);

 private:
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  scoped_refptr<FileManager> file_manager_;
  base::WeakPtrFactory<PaintPreviewFileMixin> weak_ptr_factory_{this};
};

}  // namespace paint_preview

#endif  // COMPONENTS_PAINT_PREVIEW_BROWSER_PAINT_PREVIEW_FILE_MIXIN_H_
