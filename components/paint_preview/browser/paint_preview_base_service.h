// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAINT_PREVIEW_BROWSER_PAINT_PREVIEW_BASE_SERVICE_H_
#define COMPONENTS_PAINT_PREVIEW_BROWSER_PAINT_PREVIEW_BASE_SERVICE_H_

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/paint_preview/browser/file_manager.h"
#include "components/paint_preview/browser/paint_preview_policy.h"
#include "components/paint_preview/common/capture_result.h"
#include "components/paint_preview/common/file_utils.h"
#include "components/paint_preview/common/mojom/paint_preview_recorder.mojom.h"
#include "components/paint_preview/common/proto/paint_preview.pb.h"
#include "content/public/browser/web_contents.h"

namespace paint_preview {

// A base KeyedService that serves as the Public API for Paint Previews.
// Features that want to use Paint Previews should extend this class.
// The KeyedService provides a 1:1 mapping between the service and a key or
// profile allowing each feature built on Paint Previews to reliably store
// necessary data to the right directory on disk.
//
// Implementations of this service should be created by implementing a factory
// that extends one of:
// - BrowserContextKeyedServiceFactory
// OR preferably the
// - SimpleKeyedServiceFactory
class PaintPreviewBaseService : public KeyedService {
 public:
  enum class CaptureStatus : int {
    kOk = 0,
    kContentUnsupported,
    kClientCreationFailed,
    kCaptureFailed,
  };

  enum class ProtoReadStatus : int {
    kOk = 0,
    kNoProto,
    kDeserializationError,
    kExpired,
  };

  using OnCapturedCallback =
      base::OnceCallback<void(CaptureStatus, std::unique_ptr<CaptureResult>)>;

  using OnReadProtoCallback =
      base::OnceCallback<void(ProtoReadStatus,
                              std::unique_ptr<PaintPreviewProto>)>;

  // Creates a service instance for a feature. Artifacts produced will live in
  // |profile_dir|/paint_preview/|ascii_feature_name|. Implementers of the
  // factory can also elect their factory to not construct services in the event
  // a profile |is_off_the_record|. The |policy| object is responsible for
  // determining whether or not a given WebContents is amenable to paint
  // preview. If nullptr is passed as |policy| all content is deemed amenable.
  PaintPreviewBaseService(const base::FilePath& profile_dir,
                          base::StringPiece ascii_feature_name,
                          std::unique_ptr<PaintPreviewPolicy> policy,
                          bool is_off_the_record);
  ~PaintPreviewBaseService() override;

  // Returns the file manager for the directory associated with the service.
  scoped_refptr<FileManager> GetFileManager() { return file_manager_; }

  // Returns the task runner that IO tasks should be scheduled on.
  scoped_refptr<base::SequencedTaskRunner> GetTaskRunner() {
    return task_runner_;
  }

  // Returns whether the created service is off the record.
  bool IsOffTheRecord() const { return is_off_the_record_; }

  // Acquires the PaintPreviewProto that is associated with |key| and sends it
  // to |on_read_proto_callback|. The default implementation attempts to invoke
  // GetFileManager()->DeserializePaintPreviewProto(). If |expiry_horizon| is
  // provided a proto that was last modified earlier than |now - expiry_horizon|
  // will return the kExpired status.
  //
  // Derived classes may override this function; for example, the proto is
  // cached in memory.
  virtual void GetCapturedPaintPreviewProto(
      const DirectoryKey& key,
      base::Optional<base::TimeDelta> expiry_horizon,
      OnReadProtoCallback on_read_proto_callback);

  // Captures need to run on the Browser UI thread! Captures may involve child
  // frames so the PaintPreviewClient (WebContentsObserver) must be stored as
  // WebContentsUserData which is not thread safe and must only be accessible
  // from a specific sequence i.e. the UI thread.
  //
  // The following methods both capture a Paint Preview; however, their behavior
  // and intended use is different. The first method is intended for capturing
  // full page contents. Generally, this is what you should be using for most
  // features. The second method is intended for capturing just an individual
  // RenderFrameHost and its descendents. This is intended for capturing
  // individual subframes and should be used for only a few use cases.
  //
  // NOTE: |root_dir| in the following methods should be created using
  // GetFileManager()->CreateOrGetDirectoryFor(). However, to provide
  // flexibility in managing the lifetime of created objects and ease cleanup
  // if a capture fails the service implementation is responsible for
  // implementing this management and tracking the directories in existence.
  // Data in a directory will contain:
  // - a number of SKPs listed as <guid>.skp (one per frame)
  //
  // Captures the main frame of |web_contents| (an observer for capturing Paint
  // Previews is created for web contents if it does not exist). The capture is
  // attributed to the URL of the main frame and is stored in |root_dir|. The
  // captured area is clipped to |clip_rect| if it is non-zero. Caps the per
  // frame SkPicture size to |max_per_capture_size| if non-zero. On completion
  // the status of the capture is provided via |callback|.
  void CapturePaintPreview(content::WebContents* web_contents,
                           const base::FilePath& root_dir,
                           gfx::Rect clip_rect,
                           bool capture_links,
                           size_t max_per_capture_size,
                           OnCapturedCallback callback);
  // Same as above except |render_frame_host| is directly captured rather than
  // the main frame.
  void CapturePaintPreview(content::WebContents* web_contents,
                           content::RenderFrameHost* render_frame_host,
                           const base::FilePath& root_dir,
                           gfx::Rect clip_rect,
                           bool capture_links,
                           size_t max_per_capture_size,
                           OnCapturedCallback callback);

 private:
  void OnCaptured(int frame_tree_node_id,
                  base::TimeTicks start_time,
                  OnCapturedCallback callback,
                  base::UnguessableToken guid,
                  mojom::PaintPreviewStatus status,
                  std::unique_ptr<CaptureResult> result);

  std::unique_ptr<PaintPreviewPolicy> policy_ = nullptr;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  scoped_refptr<FileManager> file_manager_;
  bool is_off_the_record_;

  base::WeakPtrFactory<PaintPreviewBaseService> weak_ptr_factory_{this};

  PaintPreviewBaseService(const PaintPreviewBaseService&) = delete;
  PaintPreviewBaseService& operator=(const PaintPreviewBaseService&) = delete;
};

}  // namespace paint_preview

#endif  // COMPONENTS_PAINT_PREVIEW_BROWSER_PAINT_PREVIEW_BASE_SERVICE_H_
