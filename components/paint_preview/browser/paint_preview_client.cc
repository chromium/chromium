// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/browser/paint_preview_client.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/containers/map_util.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/trace_event/trace_event.h"
#include "base/unguessable_token.h"
#include "base/version.h"
#include "components/paint_preview/common/capture_result.h"
#include "components/paint_preview/common/mojom/paint_preview_recorder.mojom-forward.h"
#include "components/paint_preview/common/mojom/paint_preview_types.mojom.h"
#include "components/paint_preview/common/proto_validator.h"
#include "components/paint_preview/common/serialized_recording.h"
#include "components/paint_preview/common/version.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"

namespace paint_preview {

namespace {

// Converts gfx::Rect to its RectProto form.
void RectToRectProto(const gfx::Rect& rect, RectProto* proto) {
  proto->set_x(rect.x());
  proto->set_y(rect.y());
  proto->set_width(rect.width());
  proto->set_height(rect.height());
}

// Converts |response| into |proto|. Returns a list of the frame GUIDs
// referenced by the response.
std::vector<base::UnguessableToken>
PaintPreviewCaptureResponseToPaintPreviewFrameProto(
    mojom::PaintPreviewCaptureResponsePtr response,
    base::UnguessableToken frame_guid,
    PaintPreviewFrameProto* proto) {
  proto->set_embedding_token_high(frame_guid.GetHighForSerialization());
  proto->set_embedding_token_low(frame_guid.GetLowForSerialization());
  proto->set_scroll_offset_x(response->scroll_offsets.x());
  proto->set_scroll_offset_y(response->scroll_offsets.y());
  proto->set_frame_offset_x(response->frame_offsets.x());
  proto->set_frame_offset_y(response->frame_offsets.y());

  std::vector<base::UnguessableToken> frame_guids;
  for (const auto& id_pair : response->content_id_to_embedding_token) {
    auto* content_id_embedding_token_pair =
        proto->add_content_id_to_embedding_tokens();
    content_id_embedding_token_pair->set_content_id(id_pair.first);
    content_id_embedding_token_pair->set_embedding_token_low(
        id_pair.second.GetLowForSerialization());
    content_id_embedding_token_pair->set_embedding_token_high(
        id_pair.second.GetHighForSerialization());
    frame_guids.push_back(id_pair.second);
  }

  for (const auto& link : response->links) {
    auto* link_proto = proto->add_links();
    link_proto->set_url(link->url.spec());
    RectToRectProto(link->rect, link_proto->mutable_rect());
  }

  return frame_guids;
}

// Records UKM data for the capture.
// TODO(crbug.com/40113169): Add more metrics;
// - Peak memory during capture (bucketized).
// - Compressed on disk size (bucketized).
void RecordUkmCaptureData(ukm::SourceId source_id,
                          base::TimeDelta blink_recording_time) {
  if (source_id == ukm::kInvalidSourceId) {
    return;
  }
  ukm::builders::PaintPreviewCapture(source_id)
      .SetBlinkCaptureTime(blink_recording_time.InMilliseconds())
      .Record(ukm::UkmRecorder::Get());
}

base::flat_set<base::UnguessableToken> CreateAcceptedTokenList(
    content::RenderFrameHost* render_frame_host) {
  std::vector<base::UnguessableToken> tokens;
  render_frame_host->ForEachRenderFrameHost(
      [&tokens](content::RenderFrameHost* rfh) {
        auto maybe_token = rfh->GetEmbeddingToken();
        if (maybe_token.has_value()) {
          tokens.push_back(maybe_token.value());
        }
      });
  return base::flat_set<base::UnguessableToken>(std::move(tokens));
}

mojom::PaintPreviewCaptureParamsPtr CreateRecordingRequestParams(
    RecordingPersistence persistence,
    const RecordingParams& capture_params,
    base::File file) {
  mojom::PaintPreviewCaptureParamsPtr mojo_params =
      mojom::PaintPreviewCaptureParams::New();
  mojo_params->persistence = persistence;
  mojo_params->capture_links = capture_params.capture_links;
  mojo_params->guid = capture_params.document_guid;
  mojo_params->clip_rect = capture_params.clip_rect;
  mojo_params->clip_x_coord_override = capture_params.clip_x_coord_override;
  mojo_params->clip_y_coord_override = capture_params.clip_y_coord_override;
  // For now treat all clip rects as hints only. This API should be exposed
  // when clip_rects are used intentionally to limit capture time.
  mojo_params->clip_rect_is_hint = true;
  mojo_params->is_main_frame = capture_params.is_main_frame;
  mojo_params->skip_accelerated_content =
      capture_params.skip_accelerated_content;
  mojo_params->file = std::move(file);
  mojo_params->max_capture_size = capture_params.max_capture_size;
  mojo_params->max_decoded_image_size_bytes =
      capture_params.max_decoded_image_size_bytes;
  return mojo_params;
}

// Unconditionally create or overwrite a file for writing.
base::File CreateOrOverwriteFileForWriting(const base::FilePath& path) {
  uint32_t flags = base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE;
  // This file will be passed to an untrusted process.
  flags = base::File::AddFlagsForPassingToUntrustedProcess(flags);
  base::File file(path, flags);
  return file;
}

void CloseFile(base::File file) {
  file.Close();
}

void OnSerializedRecordingFileCreated(
    const RecordingParams& capture_params,
    const base::FilePath& filename,
    PaintPreviewClient::RecordingRequestParamsReadyCallback callback,
    base::File file) {
  if (!file.IsValid()) {
    DLOG(ERROR) << "File create failed: " << file.error_details();
    std::move(callback).Run(mojom::PaintPreviewStatus::kFileCreationError, {});
  } else if (callback.IsCancelled()) {
    // The weak pointer is invalid, we should close the file on a background
    // thread to avoid it being closed implicitly via the default dtor on the UI
    // thread and triggering a scoped blocking call violation.
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::BindOnce(&CloseFile, std::move(file)));
  } else {
    std::move(callback).Run(
        mojom::PaintPreviewStatus::kOk,
        CreateRecordingRequestParams(RecordingPersistence::kFileSystem,
                                     capture_params, std::move(file)));
  }
}

}  // namespace

PaintPreviewClient::PaintPreviewParams::PaintPreviewParams(
    RecordingPersistence persistence)
    : persistence(persistence),
      inner(RecordingParams(base::UnguessableToken::Create())) {}

PaintPreviewClient::PaintPreviewParams::~PaintPreviewParams() = default;

PaintPreviewClient::InProgressDocumentCaptureState::
    InProgressDocumentCaptureState() = default;

PaintPreviewClient::InProgressDocumentCaptureState::
    ~InProgressDocumentCaptureState() {
  if (persistence == RecordingPersistence::kFileSystem &&
      should_clean_up_files) {
    for (const auto& subframe_guid : awaiting_subframes) {
      base::ThreadPool::PostTask(
          FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
          base::GetDeleteFileCallback(FilePathForFrame(subframe_guid)));
    }

    for (const auto& subframe_guid : finished_subframes) {
      base::ThreadPool::PostTask(
          FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
          base::GetDeleteFileCallback(FilePathForFrame(subframe_guid)));
    }
  }
}

PaintPreviewClient::InProgressDocumentCaptureState&
PaintPreviewClient::InProgressDocumentCaptureState::operator=(
    InProgressDocumentCaptureState&& rhs) noexcept = default;

PaintPreviewClient::InProgressDocumentCaptureState::
    InProgressDocumentCaptureState(
        InProgressDocumentCaptureState&& other) noexcept = default;

bool PaintPreviewClient::InProgressDocumentCaptureState::IsAllowedToCapture(
    const base::UnguessableToken& frame_token) const {
  return base::Contains(accepted_tokens, frame_token);
}

bool PaintPreviewClient::InProgressDocumentCaptureState::IsFinishedCapturing(
    const base::UnguessableToken& frame_token) const {
  return base::Contains(finished_subframes, frame_token);
}

bool PaintPreviewClient::InProgressDocumentCaptureState::IsCaptureInProgress(
    const base::UnguessableToken& frame_token) const {
  return base::Contains(awaiting_subframes, frame_token);
}

base::FilePath
PaintPreviewClient::InProgressDocumentCaptureState::FilePathForFrame(
    const base::UnguessableToken& frame_guid) const {
  CHECK_EQ(persistence, RecordingPersistence::kFileSystem);
  return root_dir.AppendASCII(base::StrCat({frame_guid.ToString(), ".skp"}));
}

void PaintPreviewClient::InProgressDocumentCaptureState::
    PrepareRecordingRequestParams(
        const RecordingParams& capture_params,
        const base::UnguessableToken& frame_guid,
        RecordingRequestParamsReadyCallback ready_callback) const {
  if (persistence == RecordingPersistence::kFileSystem) {
    const base::FilePath frame_filepath = FilePathForFrame(frame_guid);
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
        base::BindOnce(&CreateOrOverwriteFileForWriting, frame_filepath),
        base::BindOnce(&OnSerializedRecordingFileCreated, capture_params,
                       frame_filepath, std::move(ready_callback)));
  } else {
    std::move(ready_callback)
        .Run(mojom::PaintPreviewStatus::kOk,
             CreateRecordingRequestParams(persistence, capture_params, {}));
  }
}

void PaintPreviewClient::InProgressDocumentCaptureState::RecordSuccessfulFrame(
    const base::UnguessableToken& frame_guid,
    bool is_main_frame,
    mojom::PaintPreviewCaptureResponsePtr response) {
  // Records the data from a processed frame if it was captured successfully.
  had_success = true;

  PaintPreviewFrameProto* frame_proto;
  if (frame_guid == root_frame_token) {
    main_frame_blink_recording_time = response->blink_recording_time;
    frame_proto = proto.mutable_root_frame();
    frame_proto->set_is_main_frame(is_main_frame);
  } else {
    frame_proto = proto.add_subframes();
    frame_proto->set_is_main_frame(false);
  }

  if (persistence == RecordingPersistence::kFileSystem) {
    // Safe since |filename| is always in the form: "{hexadecimal}.skp".
    frame_proto->set_file_path(FilePathForFrame(frame_guid).AsUTF8Unsafe());
  } else {
    DCHECK(response->skp.has_value());
    serialized_skps.insert({frame_guid, std::move(response->skp.value())});
  }

  std::vector<base::UnguessableToken> remote_frame_guids =
      PaintPreviewCaptureResponseToPaintPreviewFrameProto(
          std::move(response), frame_guid, frame_proto);

  for (const auto& remote_frame_guid : remote_frame_guids) {
    // Don't wait again for a frame that was already captured. Also don't wait
    // on frames that navigated during capture and have new embedding tokens.
    if (!IsFinishedCapturing(remote_frame_guid) &&
        IsAllowedToCapture(remote_frame_guid)) {
      awaiting_subframes.insert(remote_frame_guid);
    }
  }
}

std::unique_ptr<CaptureResult>
PaintPreviewClient::InProgressDocumentCaptureState::IntoCaptureResult() && {
  // Do not clean up files since we're about to return to the user.
  should_clean_up_files = false;

  std::unique_ptr<CaptureResult> result =
      std::make_unique<CaptureResult>(persistence);
  result->proto = std::move(proto);
  result->serialized_skps = std::move(serialized_skps);
  result->capture_success = had_success;
  return result;
}

PaintPreviewClient::PaintPreviewClient(content::WebContents* web_contents)
    : content::WebContentsUserData<PaintPreviewClient>(*web_contents),
      content::WebContentsObserver(web_contents) {}

PaintPreviewClient::~PaintPreviewClient() = default;

void PaintPreviewClient::CapturePaintPreview(
    const PaintPreviewParams& params,
    content::RenderFrameHost* render_frame_host,
    PaintPreviewCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(callback);
  auto document_data_it = all_document_data_.find(params.inner.document_guid);
  if (document_data_it != all_document_data_.end()) {
    std::move(callback).Run(params.inner.document_guid,
                            mojom::PaintPreviewStatus::kGuidCollision, {});
    return;
  }
  if (!render_frame_host || params.inner.document_guid.is_empty()) {
    std::move(callback).Run(params.inner.document_guid,
                            mojom::PaintPreviewStatus::kFailed, {});
    return;
  }
  const GURL& url = render_frame_host->GetLastCommittedURL();
  if (!url.is_valid()) {
    std::move(callback).Run(params.inner.document_guid,
                            mojom::PaintPreviewStatus::kFailed, {});
    return;
  }

  InProgressDocumentCaptureState document_data;
  document_data.should_clean_up_files = true;
  document_data.persistence = params.persistence;
  document_data.root_dir = params.root_dir;
  auto* metadata = document_data.proto.mutable_metadata();
  metadata->set_url(url.spec());
  metadata->set_version(kPaintPreviewVersion);
  auto* chromeVersion = metadata->mutable_chrome_version();
  const auto& current_chrome_version = version_info::GetVersion();
  chromeVersion->set_major(current_chrome_version.components()[0]);
  chromeVersion->set_minor(current_chrome_version.components()[1]);
  chromeVersion->set_build(current_chrome_version.components()[2]);
  chromeVersion->set_patch(current_chrome_version.components()[3]);
  document_data.callback = std::move(callback);

  // Ensure the frame is not under prerendering state as the UKM cannot be
  // recorded while prerendering. Current callers pass frames that are under
  // the primary page.
  CHECK(!render_frame_host->IsInLifecycleState(
      content::RenderFrameHost::LifecycleState::kPrerendering));
  document_data.source_id = render_frame_host->GetPageUkmSourceId();

  document_data.accepted_tokens = CreateAcceptedTokenList(render_frame_host);
  auto token = render_frame_host->GetEmbeddingToken();
  CHECK(token.has_value())
      << "Error: Root frame does not have an embedding token.";
  document_data.root_frame_token = token.value();
  document_data.capture_links = params.inner.capture_links;
  document_data.max_per_capture_size = params.inner.max_capture_size;
  document_data.max_decoded_image_size_bytes =
      params.inner.max_decoded_image_size_bytes;
  document_data.skip_accelerated_content =
      params.inner.skip_accelerated_content;
  document_data_it = all_document_data_.insert(
      document_data_it, {params.inner.document_guid, std::move(document_data)});
  TRACE_EVENT_BEGIN("paint_preview", "PaintPreviewClient::CapturePaintPreview",
                    perfetto::Track::FromPointer(&document_data_it->second));
  CapturePaintPreviewInternal(params.inner, render_frame_host,
                              document_data_it->second);
}

void PaintPreviewClient::CaptureSubframePaintPreview(
    const base::UnguessableToken& guid,
    const gfx::Rect& rect,
    content::RenderFrameHost* render_subframe_host) {
  if (guid.is_empty()) {
    return;
  }

  auto* document_data = base::FindOrNull(all_document_data_, guid);
  if (!document_data) {
    return;
  }

  RecordingParams params(guid);
  params.clip_rect = rect;
  // Note: no need to set `params.clip_x_coordinate_override` or
  // `params.clip_y_coordinate_override` for subframes.
  params.is_main_frame = false;
  params.capture_links = document_data->capture_links;
  params.max_capture_size = document_data->max_per_capture_size;
  params.max_decoded_image_size_bytes =
      document_data->max_decoded_image_size_bytes;
  params.skip_accelerated_content = document_data->skip_accelerated_content;
  CapturePaintPreviewInternal(params, render_subframe_host, *document_data);
}

void PaintPreviewClient::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  // TODO(crbug.com/40115832): Investigate possible issues with cleanup if just
  // a single subframe gets deleted.
  auto maybe_token = render_frame_host->GetEmbeddingToken();
  if (!maybe_token.has_value()) {
    return;
  }

  bool is_main_frame = render_frame_host->GetParentOrOuterDocument() == nullptr;
  base::UnguessableToken frame_guid = maybe_token.value();
  auto* tokens = base::FindOrNull(pending_previews_on_subframe_, frame_guid);
  if (!tokens) {
    return;
  }

  for (const auto& document_guid : *tokens) {
    auto* document_data = base::FindOrNull(all_document_data_, document_guid);
    if (!document_data) {
      continue;
    }

    document_data->awaiting_subframes.erase(frame_guid);
    document_data->finished_subframes.insert(frame_guid);
    document_data->had_error = true;
    if (document_data->awaiting_subframes.empty() || is_main_frame) {
      if (is_main_frame) {
        for (const auto& subframe_guid : document_data->awaiting_subframes) {
          auto subframe_docs = pending_previews_on_subframe_[subframe_guid];
          subframe_docs.erase(document_guid);
          if (subframe_docs.empty()) {
            pending_previews_on_subframe_.erase(subframe_guid);
          }
        }
        document_data->awaiting_subframes.clear();
      }
      interface_ptrs_.erase(frame_guid);
      OnFinished(document_guid, *document_data);
    }
  }
  pending_previews_on_subframe_.erase(frame_guid);
}

void PaintPreviewClient::CapturePaintPreviewInternal(
    const RecordingParams& params,
    content::RenderFrameHost* render_frame_host,
    const InProgressDocumentCaptureState& document_data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Use a frame's embedding token as its GUID.
  auto token = render_frame_host->GetEmbeddingToken();

  // This should be impossible, but if it happens in a release build just abort.
  if (!token.has_value()) {
    DVLOG(1) << "Error: Attempted to capture a frame without an "
                "embedding token.";
    DUMP_WILL_BE_NOTREACHED();
    return;
  }

  // The embedding token should be in the list of tokens in the tree when
  // capture was started. If this is not the case then the frame may have
  // navigated. This is unsafe to capture.
  base::UnguessableToken frame_guid = token.value();
  if (!document_data.IsAllowedToCapture(frame_guid)) {
    return;
  }

  // Deduplicate data if a subframe is required multiple times.
  if (document_data.IsCaptureInProgress(frame_guid) ||
      document_data.IsFinishedCapturing(frame_guid)) {
    return;
  }

  document_data.PrepareRecordingRequestParams(
      params, frame_guid,
      base::BindOnce(&PaintPreviewClient::RequestCaptureOnUIThread,
                     weak_ptr_factory_.GetWeakPtr(), frame_guid, params,
                     content::GlobalRenderFrameHostId(
                         render_frame_host->GetProcess()->GetDeprecatedID(),
                         render_frame_host->GetRoutingID())));
}

void PaintPreviewClient::RequestCaptureOnUIThread(
    const base::UnguessableToken& frame_guid,
    const RecordingParams& params,
    const content::GlobalRenderFrameHostId& render_frame_id,
    mojom::PaintPreviewStatus status,
    mojom::PaintPreviewCaptureParamsPtr capture_params) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto* document_data =
      base::FindOrNull(all_document_data_, params.document_guid);
  if (!document_data) {
    return;
  }
  CHECK(document_data->callback);

  if (status != mojom::PaintPreviewStatus::kOk) {
    std::move(document_data->callback).Run(params.document_guid, status, {});
    return;
  }

  // If the RenderFrameHost navigated or is no longer around treat this as a
  // failure as a navigation occurring during capture is bad.
  auto* render_frame_host = content::RenderFrameHost::FromID(render_frame_id);

  if (!render_frame_host ||
      render_frame_host->GetEmbeddingToken().value_or(
          base::UnguessableToken::Null()) != frame_guid ||
      !capture_params) {
    std::move(document_data->callback)
        .Run(params.document_guid, mojom::PaintPreviewStatus::kCaptureFailed,
             {});
    return;
  }

  document_data->awaiting_subframes.insert(frame_guid);
  pending_previews_on_subframe_[frame_guid].insert(params.document_guid);

  auto interface_it = interface_ptrs_.find(frame_guid);
  if (interface_it == interface_ptrs_.end()) {
    interface_it = interface_ptrs_.insert(
        interface_it,
        {frame_guid, mojo::AssociatedRemote<mojom::PaintPreviewRecorder>()});
    render_frame_host->GetRemoteAssociatedInterfaces()->GetInterface(
        &interface_it->second);
  }

  // For the main frame, apply a clip rect if one is provided.
  if (params.is_main_frame) {
    capture_params->clip_rect_is_hint = false;
  }

  interface_it->second->CapturePaintPreview(
      std::move(capture_params),
      base::BindOnce(&PaintPreviewClient::OnPaintPreviewCapturedCallback,
                     weak_ptr_factory_.GetWeakPtr(), frame_guid, params,
                     render_frame_id));
}

void PaintPreviewClient::OnPaintPreviewCapturedCallback(
    const base::UnguessableToken& frame_guid,
    const RecordingParams& params,
    const content::GlobalRenderFrameHostId& render_frame_id,
    mojom::PaintPreviewStatus status,
    mojom::PaintPreviewCaptureResponsePtr response) {
  auto* document_data =
      base::FindOrNull(all_document_data_, params.document_guid);

  // There is no retry logic so always treat a frame as processed regardless of
  // |status|
  MarkFrameAsProcessed(params.document_guid, frame_guid, document_data);

  // If the RenderFrameHost navigated or is no longer around treat this as a
  // failure as a navigation occurring during capture is bad.
  auto* render_frame_host = content::RenderFrameHost::FromID(render_frame_id);
  if (!render_frame_host || render_frame_host->GetEmbeddingToken().value_or(
                                base::UnguessableToken::Null()) != frame_guid) {
    status = mojom::PaintPreviewStatus::kCaptureFailed;
  }

  if (!document_data) {
    return;
  }

  if (status == mojom::PaintPreviewStatus::kOk) {
    document_data->RecordSuccessfulFrame(frame_guid, params.is_main_frame,
                                         std::move(response));
  } else {
    document_data->had_error = true;

    // If this is the main frame we should just abort the capture on failure.
    if (params.is_main_frame) {
      document_data->awaiting_subframes.clear();
      OnFinished(params.document_guid, *document_data);
      return;
    }
  }

  if (document_data->awaiting_subframes.empty()) {
    OnFinished(params.document_guid, *document_data);
  }
}

void PaintPreviewClient::MarkFrameAsProcessed(
    base::UnguessableToken guid,
    const base::UnguessableToken& frame_guid,
    InProgressDocumentCaptureState* document_data) {
  auto& tokens = pending_previews_on_subframe_[frame_guid];
  tokens.erase(guid);
  if (tokens.empty()) {
    interface_ptrs_.erase(frame_guid);
  }
  if (!document_data) {
    return;
  }
  document_data->finished_subframes.insert(frame_guid);
  document_data->awaiting_subframes.erase(frame_guid);
}

void PaintPreviewClient::OnFinished(
    base::UnguessableToken guid,
    InProgressDocumentCaptureState& document_data) {
  CHECK_EQ(document_data.awaiting_subframes.size(), 0U);
  CHECK(document_data.callback);

  if (!PaintPreviewProtoValid(document_data.proto)) {
    document_data.had_success = false;
  }

  TRACE_EVENT_NESTABLE_ASYNC_END2(
      "paint_preview", "PaintPreviewClient::CapturePaintPreview",
      TRACE_ID_LOCAL(&document_data), "success", document_data.had_success,
      "subframes", document_data.finished_subframes.size());

  base::UmaHistogramBoolean("Browser.PaintPreview.Capture.Success",
                            document_data.had_success);
  if (document_data.had_success) {
    base::UmaHistogramCounts100(
        "Browser.PaintPreview.Capture.NumberOfFramesCaptured",
        document_data.finished_subframes.size());

    RecordUkmCaptureData(document_data.source_id,
                         document_data.main_frame_blink_recording_time);

    // At a minimum one frame was captured successfully, it is up to the
    // caller to decide if a partial success is acceptable based on what is
    // contained in the proto.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(document_data.callback), guid,
                       document_data.had_error
                           ? mojom::PaintPreviewStatus::kPartialSuccess
                           : mojom::PaintPreviewStatus::kOk,
                       std::move(document_data).IntoCaptureResult()));
  } else {
    // A proto could not be created indicating all frames failed to capture.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(document_data.callback), guid,
                                  mojom::PaintPreviewStatus::kFailed, nullptr));
  }
  all_document_data_.erase(guid);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PaintPreviewClient);

}  // namespace paint_preview
