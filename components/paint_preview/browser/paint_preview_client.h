// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAINT_PREVIEW_BROWSER_PAINT_PREVIEW_CLIENT_H_
#define COMPONENTS_PAINT_PREVIEW_BROWSER_PAINT_PREVIEW_CLIENT_H_

#include <stdint.h>

#include <memory>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "components/paint_preview/common/capture_result.h"
#include "components/paint_preview/common/mojom/paint_preview_recorder.mojom-shared.h"
#include "components/paint_preview/common/mojom/paint_preview_recorder.mojom.h"
#include "components/paint_preview/common/proto/paint_preview.pb.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

namespace paint_preview {

// Client responsible for making requests to the mojom::PaintPreviewRecorder. A
// client coordinates between multiple frames and handles capture and
// aggreagation of data from both the main frame and subframes.
//
// Should be created and accessed from the UI thread as WebContentsUserData
// requires this behavior.
class PaintPreviewClient
    : public content::WebContentsUserData<PaintPreviewClient>,
      public content::WebContentsObserver {
 public:
  using PaintPreviewCallback =
      base::OnceCallback<void(base::UnguessableToken,
                              mojom::PaintPreviewStatus,
                              std::unique_ptr<CaptureResult>)>;

  // Augmented version of mojom::PaintPreviewServiceParams.
  struct PaintPreviewParams {
    explicit PaintPreviewParams(RecordingPersistence persistence);
    ~PaintPreviewParams();

    // Indicates where the PaintPreviewRecorder should store its intermediate
    // artifacts.
    RecordingPersistence persistence;

    // The root directory in which to store paint_previews. This should be
    // a subdirectory inside the active user profile's directory.
    base::FilePath root_dir;

    RecordingParams inner;
  };

  ~PaintPreviewClient() override;

  // IMPORTANT: The Capture* methods must be called on the UI thread!

  // Captures a paint preview corresponding to the content of
  // |render_frame_host|. This will work for capturing entire documents if
  // passed the main frame or for just a specific subframe depending on
  // |render_frame_host|. |callback| is invoked on completion.
  void CapturePaintPreview(const PaintPreviewParams& params,
                           content::RenderFrameHost* render_frame_host,
                           PaintPreviewCallback callback);

  // Captures a paint preview of the subframe corresponding to
  // |render_subframe_host|.
  void CaptureSubframePaintPreview(
      const base::UnguessableToken& guid,
      const gfx::Rect& rect,
      content::RenderFrameHost* render_subframe_host);

  // WebContentsObserver implementation ---------------------------------------

  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;

 private:
  explicit PaintPreviewClient(content::WebContents* web_contents);
  friend class content::WebContentsUserData<PaintPreviewClient>;

  // Internal Storage Classes -------------------------------------------------

  // Ephemeral state for a document being captured. This will be accumulated to
  // as the capture progresses and results in a |CaptureResult|.
  struct InProgressDocumentCaptureState {
   public:
    InProgressDocumentCaptureState();
    ~InProgressDocumentCaptureState();

    RecordingPersistence persistence;

    // If |RecordingPersistence::kFileSystem|, the root directory to store
    // artifacts to. Ignored if |RecordingPersistence::kMemoryBuffer|.
    base::FilePath root_dir;

    // This corresponds to RenderFrameHost::EmbeddingToken.
    base::UnguessableToken root_frame_token;

    // From the first recording params. Whether to capture links and the
    // size limit per capture respectively.
    bool capture_links = true;
    size_t max_per_capture_size = 0;
    uint64_t max_decoded_image_size_bytes{std::numeric_limits<uint64_t>::max()};

    // UKM Source ID of the WebContent.
    ukm::SourceId source_id;

    // Main frame capture time.
    base::TimeDelta main_frame_blink_recording_time;

    // Callback that is invoked on completion of data.
    PaintPreviewCallback callback;

    // All the render frames that are still required.
    base::flat_set<base::UnguessableToken> awaiting_subframes;

    // All the render frames that have finished.
    base::flat_set<base::UnguessableToken> finished_subframes;

    // All the render frames that are allowed to be captured.
    base::flat_set<base::UnguessableToken> accepted_tokens;

    // If |RecordingPersistence::kMemoryBuffer|, this will contain the
    // successful recordings. Empty if |RecordingPersistence::FileSystem|
    base::flat_map<base::UnguessableToken, mojo_base::BigBuffer>
        serialized_skps;

    PaintPreviewProto proto;

    // Indicates that at least one subframe finished unsuccessfully.
    bool had_error = false;

    // Indicates that at least one subframe finished successfully.
    bool had_success = false;

    // Indicates if we should clean up files associated with awaiting frames on
    // destruction
    bool should_clean_up_files = false;

    // This flag will skip GPU accelerated content where applicable when
    // capturing. This reduces hangs, capture time and may also reduce OOM
    // crashes, but results in a lower fideltiy capture (i.e. the contents
    // captured may not accurately reflect the content visible to the user at
    // time of capture). See PaintPreviewBaseService::CaptureParams for a
    // description of the effects of this flag.
    bool skip_accelerated_content = false;

    // Generates a file path based off |root_dir| and |frame_guid|. Will be in
    // the form "{hexadecimal}.skp".
    base::FilePath FilePathForFrame(const base::UnguessableToken& frame_guid);

    // Record a successful recording into this capture state.
    void RecordSuccessfulFrame(const base::UnguessableToken& frame_guid,
                               bool is_main_frame,
                               mojom::PaintPreviewCaptureResponsePtr response);

    // Convert this capture state into a form that can be returned to the
    // original paint preview capture request.
    std::unique_ptr<CaptureResult> IntoCaptureResult() &&;

    InProgressDocumentCaptureState& operator=(
        InProgressDocumentCaptureState&& other) noexcept;
    InProgressDocumentCaptureState(
        InProgressDocumentCaptureState&& other) noexcept;

   private:
    InProgressDocumentCaptureState(const InProgressDocumentCaptureState&) =
        delete;
    InProgressDocumentCaptureState& operator=(
        const InProgressDocumentCaptureState&) = delete;
  };

  // Sets up for a capture of a frame on |render_frame_host| according to
  // |params|.
  void CapturePaintPreviewInternal(const RecordingParams& params,
                                   content::RenderFrameHost* render_frame_host);

  // Initiates capture via the PaintPreviewRecorder associated with
  // |render_frame_host| using |params| to configure the request. |frame_guid|
  // is the GUID associated with the frame. |path| is file path associated with
  // the File stored in |result| (base::File isn't aware of its file path).
  void RequestCaptureOnUIThread(
      const base::UnguessableToken& frame_guid,
      const RecordingParams& params,
      const content::GlobalRenderFrameHostId& render_frame_id,
      mojom::PaintPreviewStatus status,
      mojom::PaintPreviewCaptureParamsPtr capture_params);

  // Handles recording the frame and updating client state when capture is
  // complete.
  void OnPaintPreviewCapturedCallback(
      const base::UnguessableToken& frame_guid,
      const RecordingParams& params,
      const content::GlobalRenderFrameHostId& render_frame_id,
      mojom::PaintPreviewStatus status,
      mojom::PaintPreviewCaptureResponsePtr response);

  // Marks a frame as having been processed, this should occur regardless of
  // whether the processed frame is valid as there is no retry.
  void MarkFrameAsProcessed(base::UnguessableToken guid,
                            const base::UnguessableToken& frame_guid);

  // Handles finishing the capture once all frames are received.
  void OnFinished(base::UnguessableToken guid,
                  InProgressDocumentCaptureState* document_data);

  // Storage ------------------------------------------------------------------

  // Maps a RenderFrameHost and document to a remote interface.
  base::flat_map<base::UnguessableToken,
                 mojo::AssociatedRemote<mojom::PaintPreviewRecorder>>
      interface_ptrs_;

  // Maps render frame's GUID and document cookies that requested the frame.
  base::flat_map<base::UnguessableToken, base::flat_set<base::UnguessableToken>>
      pending_previews_on_subframe_;

  // Maps a document GUID to its capture state while it is in-progress. Entries
  // in this map should be cleaned up when a capture completes (either
  // successfully or not).
  base::flat_map<base::UnguessableToken, InProgressDocumentCaptureState>
      all_document_data_;

  base::WeakPtrFactory<PaintPreviewClient> weak_ptr_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  PaintPreviewClient(const PaintPreviewClient&) = delete;
  PaintPreviewClient& operator=(const PaintPreviewClient&) = delete;
};

}  // namespace paint_preview

#endif  // COMPONENTS_PAINT_PREVIEW_BROWSER_PAINT_PREVIEW_CLIENT_H_
