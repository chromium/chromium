// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/printing/browser/print_composite_client.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "components/discardable_memory/service/discardable_shared_memory_manager.h"
#include "components/printing/common/print_messages.h"
#include "components/services/pdf_compositor/public/cpp/pdf_service_mojo_types.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/service_process_host.h"
#include "printing/printing_utils.h"

namespace printing {

namespace {

uint64_t GenerateFrameGuid(content::RenderFrameHost* render_frame_host) {
  int process_id = render_frame_host->GetProcess()->GetID();
  int frame_id = render_frame_host->GetRoutingID();
  return static_cast<uint64_t>(process_id) << 32 | frame_id;
}

// Converts a ContentToProxyIdMap to ContentToFrameMap.
// ContentToProxyIdMap maps content id to the routing id of its corresponding
// render frame proxy. This is generated when the content holder was created;
// ContentToFrameMap maps content id to its render frame's global unique id.
// The global unique id has the render process id concatenated with render
// frame routing id, which can uniquely identify a render frame.
ContentToFrameMap ConvertContentInfoMap(
    content::RenderFrameHost* render_frame_host,
    const ContentToProxyIdMap& content_proxy_map) {
  ContentToFrameMap content_frame_map;
  int process_id = render_frame_host->GetProcess()->GetID();
  for (const auto& entry : content_proxy_map) {
    auto content_id = entry.first;
    auto proxy_id = entry.second;
    // Find the RenderFrameHost that the proxy id corresponds to.
    content::RenderFrameHost* rfh =
        content::RenderFrameHost::FromPlaceholderId(process_id, proxy_id);
    if (!rfh) {
      // If the corresponding RenderFrameHost cannot be found, just skip it.
      continue;
    }

    // Store this frame's global unique id into the map.
    content_frame_map[content_id] = GenerateFrameGuid(rfh);
  }
  return content_frame_map;
}

void BindDiscardableSharedMemoryManagerOnIOThread(
    mojo::PendingReceiver<
        discardable_memory::mojom::DiscardableSharedMemoryManager> receiver) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  discardable_memory::DiscardableSharedMemoryManager::Get()->Bind(
      std::move(receiver));
}

}  // namespace

PrintCompositeClient::PrintCompositeClient(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

PrintCompositeClient::~PrintCompositeClient() {}

bool PrintCompositeClient::OnMessageReceived(
    const IPC::Message& message,
    content::RenderFrameHost* render_frame_host) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_WITH_PARAM(PrintCompositeClient, message,
                                   render_frame_host)
    IPC_MESSAGE_HANDLER(PrintHostMsg_DidPrintFrameContent,
                        OnDidPrintFrameContent)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PrintCompositeClient::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  uint64_t frame_guid = GenerateFrameGuid(render_frame_host);
  auto iter = pending_subframe_cookies_.find(frame_guid);
  if (iter != pending_subframe_cookies_.end()) {
    // When a subframe we are expecting is deleted, we should notify pdf
    // compositor service.
    for (int doc_cookie : iter->second) {
      auto* compositor = GetCompositeRequest(doc_cookie);
      compositor->NotifyUnavailableSubframe(frame_guid);
    }
    pending_subframe_cookies_.erase(iter);
  }
}

void PrintCompositeClient::OnDidPrintFrameContent(
    content::RenderFrameHost* render_frame_host,
    int document_cookie,
    const PrintHostMsg_DidPrintContent_Params& params) {
  auto* outer_contents = web_contents()->GetOuterWebContents();
  if (outer_contents) {
    // When the printed content belongs to an extension or app page, the print
    // composition needs to be handled by its outer content.
    // TODO(weili): so far, we don't have printable web contents nested in more
    // than one level. In the future, especially after PDF plugin is moved to
    // OOPIF-based webview, we should check whether we need to handle web
    // contents nested in multiple layers.
    auto* outer_client = PrintCompositeClient::FromWebContents(outer_contents);
    DCHECK(outer_client);
    outer_client->OnDidPrintFrameContent(render_frame_host, document_cookie,
                                         params);
    return;
  }

  // Content in |params| is sent from untrusted source; only minimal processing
  // is done here. Most of it will be directly forwarded to pdf compositor
  // service.
  auto* compositor = GetCompositeRequest(document_cookie);
  auto region = params.metafile_data_region.Duplicate();
  uint64_t frame_guid = GenerateFrameGuid(render_frame_host);
  compositor->AddSubframeContent(
      frame_guid, std::move(region),
      ConvertContentInfoMap(render_frame_host, params.subframe_content_info));

  // Update our internal states about this frame.
  pending_subframe_cookies_[frame_guid].erase(document_cookie);
  if (pending_subframe_cookies_[frame_guid].empty())
    pending_subframe_cookies_.erase(frame_guid);
  printed_subframes_[document_cookie].insert(frame_guid);
}

void PrintCompositeClient::PrintCrossProcessSubframe(
    const gfx::Rect& rect,
    int document_cookie,
    content::RenderFrameHost* subframe_host) {
  PrintMsg_PrintFrame_Params params;
  params.printable_area = rect;
  params.document_cookie = document_cookie;
  uint64_t frame_guid = GenerateFrameGuid(subframe_host);
  if (!subframe_host->IsRenderFrameLive()) {
    // When the subframe is dead, no need to send message,
    // just notify the service.
    auto* compositor = GetCompositeRequest(document_cookie);
    compositor->NotifyUnavailableSubframe(frame_guid);
    return;
  }

  auto subframe_iter = printed_subframes_.find(document_cookie);
  if (subframe_iter != printed_subframes_.end() &&
      base::Contains(subframe_iter->second, frame_guid)) {
    // If this frame is already printed, no need to print again.
    return;
  }

  auto cookie_iter = pending_subframe_cookies_.find(frame_guid);
  if (cookie_iter != pending_subframe_cookies_.end() &&
      base::Contains(cookie_iter->second, document_cookie)) {
    // If this frame is being printed, no need to print again.
    return;
  }

  // Send the request to the destination frame.
  subframe_host->Send(
      new PrintMsg_PrintFrameContent(subframe_host->GetRoutingID(), params));
  pending_subframe_cookies_[frame_guid].insert(document_cookie);
}

void PrintCompositeClient::DoCompositePageToPdf(
    int document_cookie,
    content::RenderFrameHost* render_frame_host,
    const PrintHostMsg_DidPrintContent_Params& content,
    mojom::PdfCompositor::CompositePageToPdfCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto* compositor = GetCompositeRequest(document_cookie);
  auto region = content.metafile_data_region.Duplicate();
  compositor->CompositePageToPdf(
      GenerateFrameGuid(render_frame_host), std::move(region),
      ConvertContentInfoMap(render_frame_host, content.subframe_content_info),
      base::BindOnce(&PrintCompositeClient::OnDidCompositePageToPdf,
                     std::move(callback)));
}

void PrintCompositeClient::DoPrepareForDocumentToPdf(
    int document_cookie,
    mojom::PdfCompositor::PrepareForDocumentToPdfCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!GetIsDocumentConcurrentlyComposited(document_cookie));

  is_doc_concurrently_composited_set_.insert(document_cookie);
  auto* compositor = GetCompositeRequest(document_cookie);
  compositor->PrepareForDocumentToPdf(
      base::BindOnce(&PrintCompositeClient::OnDidPrepareForDocumentToPdf,
                     std::move(callback)));
}

void PrintCompositeClient::DoCompleteDocumentToPdf(
    int document_cookie,
    uint32_t pages_count,
    mojom::PdfCompositor::CompleteDocumentToPdfCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(GetIsDocumentConcurrentlyComposited(document_cookie));

  auto* compositor = GetCompositeRequest(document_cookie);

  // Since this class owns compositor, compositor will be gone when this class
  // is destructed. Mojo won't call its callback in that case so it is safe to
  // use unretained |this| pointer here.
  compositor->CompleteDocumentToPdf(
      pages_count,
      base::BindOnce(&PrintCompositeClient::OnDidCompleteDocumentToPdf,
                     base::Unretained(this), document_cookie,
                     std::move(callback)));
}

void PrintCompositeClient::DoCompositeDocumentToPdf(
    int document_cookie,
    content::RenderFrameHost* render_frame_host,
    const PrintHostMsg_DidPrintContent_Params& content,
    mojom::PdfCompositor::CompositeDocumentToPdfCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!GetIsDocumentConcurrentlyComposited(document_cookie));

  auto* compositor = GetCompositeRequest(document_cookie);
  auto region = content.metafile_data_region.Duplicate();

  // Since this class owns compositor, compositor will be gone when this class
  // is destructed. Mojo won't call its callback in that case so it is safe to
  // use unretained |this| pointer here.
  compositor->CompositeDocumentToPdf(
      GenerateFrameGuid(render_frame_host), std::move(region),
      ConvertContentInfoMap(render_frame_host, content.subframe_content_info),
      base::BindOnce(&PrintCompositeClient::OnDidCompositeDocumentToPdf,
                     base::Unretained(this), document_cookie,
                     std::move(callback)));
}

// static
void PrintCompositeClient::OnDidCompositePageToPdf(
    mojom::PdfCompositor::CompositePageToPdfCallback callback,
    mojom::PdfCompositor::Status status,
    base::ReadOnlySharedMemoryRegion region) {
  std::move(callback).Run(status, std::move(region));
}

void PrintCompositeClient::OnDidCompositeDocumentToPdf(
    int document_cookie,
    mojom::PdfCompositor::CompositeDocumentToPdfCallback callback,
    mojom::PdfCompositor::Status status,
    base::ReadOnlySharedMemoryRegion region) {
  RemoveCompositeRequest(document_cookie);
  // Clear all stored printed subframes.
  printed_subframes_.erase(document_cookie);

  std::move(callback).Run(status, std::move(region));
}

// static
void PrintCompositeClient::OnDidPrepareForDocumentToPdf(
    mojom::PdfCompositor::PrepareForDocumentToPdfCallback callback,
    mojom::PdfCompositor::Status status) {
  std::move(callback).Run(status);
}

void PrintCompositeClient::OnDidCompleteDocumentToPdf(
    int document_cookie,
    mojom::PdfCompositor::CompleteDocumentToPdfCallback callback,
    mojom::PdfCompositor::Status status,
    base::ReadOnlySharedMemoryRegion region) {
  RemoveCompositeRequest(document_cookie);
  // Clear all stored printed subframes.
  printed_subframes_.erase(document_cookie);
  // No longer concurrently compositing this document.
  is_doc_concurrently_composited_set_.erase(document_cookie);
  std::move(callback).Run(status, std::move(region));
}

bool PrintCompositeClient::GetIsDocumentConcurrentlyComposited(
    int cookie) const {
  return base::Contains(is_doc_concurrently_composited_set_, cookie);
}

mojom::PdfCompositor* PrintCompositeClient::GetCompositeRequest(int cookie) {
  auto iter = compositor_map_.find(cookie);
  if (iter != compositor_map_.end()) {
    DCHECK(iter->second.is_bound());
    return iter->second.get();
  }

  iter = compositor_map_.emplace(cookie, CreateCompositeRequest()).first;
  return iter->second.get();
}

void PrintCompositeClient::RemoveCompositeRequest(int cookie) {
  size_t erased = compositor_map_.erase(cookie);
  DCHECK_EQ(erased, 1u);
}

mojo::Remote<mojom::PdfCompositor>
PrintCompositeClient::CreateCompositeRequest() {
  auto compositor = content::ServiceProcessHost::Launch<mojom::PdfCompositor>(
      content::ServiceProcessHost::Options()
          .WithDisplayName(IDS_PDF_COMPOSITOR_SERVICE_DISPLAY_NAME)
          .WithSandboxType(service_manager::SANDBOX_TYPE_PDF_COMPOSITOR)
          .Pass());

  mojo::PendingRemote<discardable_memory::mojom::DiscardableSharedMemoryManager>
      discardable_memory_manager;
  base::PostTask(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(
          &BindDiscardableSharedMemoryManagerOnIOThread,
          discardable_memory_manager.InitWithNewPipeAndPassReceiver()));
  compositor->SetDiscardableSharedMemoryManager(
      std::move(discardable_memory_manager));
  compositor->SetWebContentsURL(web_contents()->GetLastCommittedURL());
  compositor->SetUserAgent(user_agent_);
  return compositor;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PrintCompositeClient)

}  // namespace printing
