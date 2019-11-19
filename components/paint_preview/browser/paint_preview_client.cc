// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/browser/paint_preview_client.h"

#include <utility>

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/scoped_blocking_call.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace paint_preview {

namespace {

// A frame's GUID is (ProcessID || Routing ID) where || is a bitwise
// concatenation.
uint64_t GenerateFrameGuid(content::RenderFrameHost* render_frame_host) {
  int process_id = render_frame_host->GetProcess()->GetID();
  int frame_id = render_frame_host->GetRoutingID();
  return static_cast<uint64_t>(process_id) << 32 | frame_id;
}

// Converts gfx::Rect to its RectProto form.
void RectToRectProto(const gfx::Rect& rect, RectProto* proto) {
  proto->set_x(rect.x());
  proto->set_y(rect.y());
  proto->set_width(rect.width());
  proto->set_height(rect.height());
}

// Converts |response| into |proto|. Returns a list of the frame GUIDs
// referenced by the response.
std::vector<uint64_t> PaintPreviewCaptureResponseToPaintPreviewFrameProto(
    mojom::PaintPreviewCaptureResponsePtr response,
    content::RenderFrameHost* render_frame_host,
    PaintPreviewFrameProto* proto) {
  int process_id = render_frame_host->GetProcess()->GetID();
  proto->set_id(static_cast<uint64_t>(process_id) << 32 | response->id);

  std::vector<uint64_t> frame_guids;
  auto* proto_content_id_proxy_map = proto->mutable_content_id_proxy_id_map();
  for (const auto& id_pair : response->content_id_proxy_id_map) {
    content::RenderFrameHost* rfh =
        content::RenderFrameHost::FromPlaceholderId(process_id, id_pair.second);
    if (!rfh) {
      // The render frame host doesn't exist. We won't be able to infill this
      // content. 0 is an invalid content_id so an empty picture will be used
      // instead.
      proto_content_id_proxy_map->insert({id_pair.first, 0});
      continue;
    }
    auto guid = GenerateFrameGuid(rfh);
    frame_guids.push_back(guid);
    proto_content_id_proxy_map->insert({id_pair.first, guid});
  }

  for (const auto& link : response->links) {
    auto* link_proto = proto->add_links();
    link_proto->set_url(link->url.spec());
    RectToRectProto(link->rect, link_proto->mutable_rect());
  }

  return frame_guids;
}

}  // namespace

PaintPreviewClient::PaintPreviewParams::PaintPreviewParams() = default;
PaintPreviewClient::PaintPreviewParams::~PaintPreviewParams() = default;

PaintPreviewClient::PaintPreviewData::PaintPreviewData() = default;
PaintPreviewClient::PaintPreviewData::~PaintPreviewData() = default;
PaintPreviewClient::PaintPreviewData& PaintPreviewClient::PaintPreviewData::
operator=(PaintPreviewData&& rhs) noexcept = default;
PaintPreviewClient::PaintPreviewData::PaintPreviewData(
    PaintPreviewData&& other) noexcept = default;

PaintPreviewClient::CreateResult::CreateResult(base::File file,
                                               base::File::Error error)
    : file(std::move(file)), error(error) {}
PaintPreviewClient::CreateResult::~CreateResult() = default;
PaintPreviewClient::CreateResult::CreateResult(CreateResult&& other) = default;
PaintPreviewClient::CreateResult& PaintPreviewClient::CreateResult::operator=(
    CreateResult&& other) = default;

PaintPreviewClient::PaintPreviewClient(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}
PaintPreviewClient::~PaintPreviewClient() = default;

void PaintPreviewClient::CapturePaintPreview(
    const PaintPreviewParams& params,
    content::RenderFrameHost* render_frame_host,
    PaintPreviewCallback callback) {
  if (base::Contains(all_document_data_, params.document_guid)) {
    std::move(callback).Run(params.document_guid,
                            mojom::PaintPreviewStatus::kGuidCollision, nullptr);
    return;
  }
  all_document_data_.insert({params.document_guid, PaintPreviewData()});
  auto* document_data = &all_document_data_[params.document_guid];
  document_data->root_dir = params.root_dir;
  document_data->callback = std::move(callback);
  CapturePaintPreviewInternal(params, render_frame_host);
}

void PaintPreviewClient::CaptureSubframePaintPreview(
    const base::UnguessableToken& guid,
    const gfx::Rect& rect,
    content::RenderFrameHost* render_subframe_host) {
  PaintPreviewParams params;
  params.document_guid = guid;
  params.clip_rect = rect;
  params.is_main_frame = false;
  CapturePaintPreviewInternal(params, render_subframe_host);
}

void PaintPreviewClient::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  uint64_t frame_guid = GenerateFrameGuid(render_frame_host);
  auto it = pending_previews_on_subframe_.find(frame_guid);
  if (it == pending_previews_on_subframe_.end())
    return;
  for (const auto& document_guid : it->second) {
    auto data_it = all_document_data_.find(document_guid);
    if (data_it == all_document_data_.end())
      continue;
    data_it->second.awaiting_subframes.erase(frame_guid);
    data_it->second.finished_subframes.insert(frame_guid);
    data_it->second.had_error = true;
    if (data_it->second.awaiting_subframes.empty()) {
      interface_ptrs_.erase(frame_guid);
      OnFinished(document_guid, &data_it->second);
    }
  }
  pending_previews_on_subframe_.erase(frame_guid);
}

PaintPreviewClient::CreateResult PaintPreviewClient::CreateFileHandle(
    const base::FilePath& path) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  base::File file(path,
                  base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  return CreateResult(std::move(file), file.error_details());
}

mojom::PaintPreviewCaptureParamsPtr PaintPreviewClient::CreateMojoParams(
    const PaintPreviewParams& params,
    base::File file) {
  mojom::PaintPreviewCaptureParamsPtr mojo_params =
      mojom::PaintPreviewCaptureParams::New();
  mojo_params->guid = params.document_guid;
  mojo_params->clip_rect = params.clip_rect;
  mojo_params->is_main_frame = params.is_main_frame;
  mojo_params->file = std::move(file);
  return mojo_params;
}

void PaintPreviewClient::CapturePaintPreviewInternal(
    const PaintPreviewParams& params,
    content::RenderFrameHost* render_frame_host) {
  uint64_t frame_guid = GenerateFrameGuid(render_frame_host);
  auto* document_data = &all_document_data_[params.document_guid];
  // Deduplicate data if a subframe is required multiple times.
  if (base::Contains(document_data->awaiting_subframes, frame_guid) ||
      base::Contains(document_data->finished_subframes, frame_guid))
    return;
  base::FilePath file_path = document_data->root_dir.AppendASCII(
      base::StrCat({base::NumberToString(frame_guid), ".skp"}));
  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&CreateFileHandle, file_path),
      base::BindOnce(&PaintPreviewClient::RequestCaptureOnUIThread,
                     weak_ptr_factory_.GetWeakPtr(), params, frame_guid,
                     base::Unretained(render_frame_host), file_path));
}

void PaintPreviewClient::RequestCaptureOnUIThread(
    const PaintPreviewParams& params,
    uint64_t frame_guid,
    content::RenderFrameHost* render_frame_host,
    const base::FilePath& file_path,
    CreateResult result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto* document_data = &all_document_data_[params.document_guid];
  if (result.error != base::File::FILE_OK) {
    // Don't block up the UI thread and answer the callback on a different
    // thread.
    base::PostTask(
        FROM_HERE,
        base::BindOnce(std::move(document_data->callback), params.document_guid,
                       mojom::PaintPreviewStatus::kFileCreationError, nullptr));
    return;
  }

  document_data->awaiting_subframes.insert(frame_guid);
  auto it = pending_previews_on_subframe_.find(frame_guid);
  if (it != pending_previews_on_subframe_.end()) {
    it->second.insert(params.document_guid);
  } else {
    pending_previews_on_subframe_.insert(std::make_pair(
        frame_guid,
        base::flat_set<base::UnguessableToken>({params.document_guid})));
  }

  if (!base::Contains(interface_ptrs_, frame_guid)) {
    interface_ptrs_.insert(
        {frame_guid, mojo::AssociatedRemote<mojom::PaintPreviewRecorder>()});
    render_frame_host->GetRemoteAssociatedInterfaces()->GetInterface(
        &interface_ptrs_[frame_guid]);
  }
  interface_ptrs_[frame_guid]->CapturePaintPreview(
      CreateMojoParams(params, std::move(result.file)),
      base::BindOnce(&PaintPreviewClient::OnPaintPreviewCapturedCallback,
                     weak_ptr_factory_.GetWeakPtr(), params.document_guid,
                     frame_guid, params.is_main_frame, file_path,
                     base::Unretained(render_frame_host)));
}

void PaintPreviewClient::OnPaintPreviewCapturedCallback(
    base::UnguessableToken guid,
    uint64_t frame_guid,
    bool is_main_frame,
    const base::FilePath& filename,
    content::RenderFrameHost* render_frame_host,
    mojom::PaintPreviewStatus status,
    mojom::PaintPreviewCaptureResponsePtr response) {
  // There is no retry logic so always treat a frame as processed regardless of
  // |status|
  MarkFrameAsProcessed(guid, frame_guid);

  if (status == mojom::PaintPreviewStatus::kOk)
    status = RecordFrame(guid, frame_guid, is_main_frame, filename,
                         render_frame_host, std::move(response));
  auto* document_data = &all_document_data_[guid];
  if (status != mojom::PaintPreviewStatus::kOk)
    document_data->had_error = true;

  if (document_data->awaiting_subframes.empty())
    OnFinished(guid, document_data);
}

void PaintPreviewClient::MarkFrameAsProcessed(base::UnguessableToken guid,
                                              uint64_t frame_guid) {
  pending_previews_on_subframe_[frame_guid].erase(guid);
  if (pending_previews_on_subframe_[frame_guid].empty())
    interface_ptrs_.erase(frame_guid);
  all_document_data_[guid].finished_subframes.insert(frame_guid);
  all_document_data_[guid].awaiting_subframes.erase(frame_guid);
}

mojom::PaintPreviewStatus PaintPreviewClient::RecordFrame(
    base::UnguessableToken guid,
    uint64_t frame_guid,
    bool is_main_frame,
    const base::FilePath& filename,
    content::RenderFrameHost* render_frame_host,
    mojom::PaintPreviewCaptureResponsePtr response) {
  auto it = all_document_data_.find(guid);
  if (!it->second.proto)
    it->second.proto = std::make_unique<PaintPreviewProto>();

  PaintPreviewProto* proto_ptr = it->second.proto.get();

  PaintPreviewFrameProto* frame_proto;
  if (is_main_frame) {
    frame_proto = proto_ptr->mutable_root_frame();
    frame_proto->set_is_main_frame(true);
  } else {
    frame_proto = proto_ptr->add_subframes();
    frame_proto->set_is_main_frame(false);
  }
  // Safe since always <#>.skp.
  frame_proto->set_file_path(filename.AsUTF8Unsafe());

  std::vector<uint64_t> remote_frame_guids =
      PaintPreviewCaptureResponseToPaintPreviewFrameProto(
          std::move(response), render_frame_host, frame_proto);

  for (const auto& remote_frame_guid : remote_frame_guids) {
    if (!base::Contains(it->second.finished_subframes, remote_frame_guid))
      it->second.awaiting_subframes.insert(remote_frame_guid);
  }
  return mojom::PaintPreviewStatus::kOk;
}

void PaintPreviewClient::OnFinished(base::UnguessableToken guid,
                                    PaintPreviewData* document_data) {
  if (!document_data)
    return;
  if (document_data->proto) {
    // At a minimum one frame was captured successfully, it is up to the
    // caller to decide if a partial success is acceptable based on what is
    // contained in the proto.
    base::PostTask(
        FROM_HERE,
        base::BindOnce(std::move(document_data->callback), guid,
                       document_data->had_error
                           ? mojom::PaintPreviewStatus::kPartialSuccess
                           : mojom::PaintPreviewStatus::kOk,
                       std::move(document_data->proto)));
  } else {
    // A proto could not be created indicating all frames failed to capture.
    base::PostTask(FROM_HERE,
                   base::BindOnce(std::move(document_data->callback), guid,
                                  mojom::PaintPreviewStatus::kFailed, nullptr));
  }
  all_document_data_.erase(guid);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PaintPreviewClient)

}  // namespace paint_preview
