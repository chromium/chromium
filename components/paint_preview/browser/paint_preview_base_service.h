// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAINT_PREVIEW_BROWSER_PAINT_PREVIEW_BASE_SERVICE_H_
#define COMPONENTS_PAINT_PREVIEW_BROWSER_PAINT_PREVIEW_BASE_SERVICE_H_

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "components/paint_preview/browser/paint_preview_file_mixin.h"
#include "components/paint_preview/browser/paint_preview_policy.h"
#include "components/paint_preview/common/capture_result.h"
#include "components/paint_preview/common/file_utils.h"
#include "components/paint_preview/common/mojom/paint_preview_recorder.mojom.h"
#include "components/paint_preview/common/proto/paint_preview.pb.h"
#include "components/paint_preview/common/serialized_recording.h"
#include "content/public/browser/web_contents.h"

namespace paint_preview {

// A base class that serves as the Public API for Paint Previews.
// Features that want to use Paint Previews should extend this class.
// This service supports both in-memery and in-file captures.
//
// The KeyedService provides a 1:1 mapping between the service and a key or
// profile allowing each feature built on Paint Previews to reliably store
// necessary data to the right directory on disk.
//
// [NOTE] for file system captures:
// - PaintPreviewFileMixin object needs to be supplied.
// - Implementations of the service should be created by implementing a factory
//   that extends one of:
//   - BrowserContextKeyedServiceFactory
//   OR preferably the
//   - SimpleKeyedServiceFactory
class PaintPreviewBaseService : public KeyedService {
 public:
  enum class CaptureStatus : int {
    kOk = 0,
    kContentUnsupported,
    kClientCreationFailed,
    kCaptureFailed,
  };

  struct CaptureParams {
    raw_ptr<content::WebContents> web_contents = nullptr;

    // In case of specifying, an individual |render_frame_host| and its
    // descendents will be captured. In case of nullptr, full page contents will
    // be captured.
    //
    // Generally, leaving this as nullptr is what you should be doing for most
    // features. Specifying a |render_frame_host| is intended for capturing
    // individual subframes and should be used for only a few use cases.
    raw_ptr<content::RenderFrameHost> render_frame_host = nullptr;

    // Store artifacts in the file system or in memory buffers.
    RecordingPersistence persistence;

    // |root_dir| should be created using
    // GetFileManager()->CreateOrGetDirectoryFor(). However, to provide
    // flexibility in managing the lifetime of created objects and ease cleanup
    // if a capture fails the service implementation is responsible for
    // implementing this management and tracking the directories in existence.
    // Data in a directory will contain:
    // - a number of SKPs listed as <guid>.skp (one per frame)
    //
    // Will be ignored if persistence = kMemoryBuffer
    raw_ptr<const base::FilePath> root_dir = nullptr;

    // The captured area is clipped to |clip_rect| if it is non-zero.
    gfx::Rect clip_rect;

    // Whether to record links.
    bool capture_links;

    // Cap the perframe SkPicture size to |max_per_capture_size| if non-zero.
    size_t max_per_capture_size;

    // Limit on the maximum size of a decoded image that can be serialized.
    // Any images with a decoded size exceeding this value will be discarded.
    // This can be used to reduce the chance of an OOM during serialization and
    // later during playback.
    uint64_t max_decoded_image_size_bytes{std::numeric_limits<uint64_t>::max()};

    // This flag will skip GPU accelerated content where applicable when
    // capturing. This reduces hangs, capture time and may also reduce OOM
    // crashes, but results in a lower fideltiy capture (i.e. the contents
    // captured may not accurately reflect the content visible to the user at
    // time of capture).
    //
    // At present this flag:
    // - Shows a poster or blank space instead of live video frames.
    bool skip_accelerated_content{false};
  };

  using OnCapturedCallback =
      base::OnceCallback<void(CaptureStatus, std::unique_ptr<CaptureResult>)>;

  // Creates a service instance for a feature. Artifacts produced will live in
  // |profile_dir|/paint_preview/|ascii_feature_name|. Implementers of the
  // factory can also elect their factory to not construct services in the event
  // a profile |is_off_the_record|. The |policy| object is responsible for
  // determining whether or not a given WebContents is amenable to paint
  // preview. If nullptr is passed as |policy| all content is deemed amenable.
  //
  // NOTE: Pass nullptr as |file_mixin| if you're planning to use the service
  // for only in-memory captures.
  PaintPreviewBaseService(std::unique_ptr<PaintPreviewFileMixin> file_mixin,
                          std::unique_ptr<PaintPreviewPolicy> policy,
                          bool is_off_the_record);
  ~PaintPreviewBaseService() override;

  PaintPreviewFileMixin* GetFileMixin() { return file_mixin_.get(); }

  // Returns whether the created service is off the record.
  bool IsOffTheRecord() const { return is_off_the_record_; }

  // Captures need to run on the Browser UI thread! Captures may involve child
  // frames so the PaintPreviewClient (WebContentsObserver) must be stored as
  // WebContentsUserData which is not thread safe and must only be accessible
  // from a specific sequence i.e. the UI thread.
  //
  // Captures the main frame of |capture_params.web_contents| (an observer for
  // capturing Paint Previews is created for web contents if it does not exist).
  // The capture is attributed to the URL of the main frame. On completion the
  // status of the capture is provided via |callback|.
  //
  // See |PaintPreviewBaseService::CaptureParams| for more info about the
  // capture parameters.
  void CapturePaintPreview(CaptureParams capture_params,
                           OnCapturedCallback callback);

 private:
  void OnCaptured(base::ScopedClosureRunner capture_handle,
                  base::TimeTicks start_time,
                  OnCapturedCallback callback,
                  base::UnguessableToken guid,
                  mojom::PaintPreviewStatus status,
                  std::unique_ptr<CaptureResult> result);

  std::unique_ptr<PaintPreviewFileMixin> file_mixin_;
  std::unique_ptr<PaintPreviewPolicy> policy_;
  bool is_off_the_record_;

  base::WeakPtrFactory<PaintPreviewBaseService> weak_ptr_factory_{this};

  PaintPreviewBaseService(const PaintPreviewBaseService&) = delete;
  PaintPreviewBaseService& operator=(const PaintPreviewBaseService&) = delete;
};

}  // namespace paint_preview

#endif  // COMPONENTS_PAINT_PREVIEW_BROWSER_PAINT_PREVIEW_BASE_SERVICE_H_
