// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/printing/browser/print_composite_client.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/services/print_compositor/public/cpp/print_service_mojo_types.h"
#include "components/services/print_compositor/public/mojom/print_compositor.mojom.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/service_process_host.h"
#include "printing/common/metafile_utils.h"
#include "printing/mojom/print.mojom.h"
#include "printing/print_settings.h"
#include "printing/printed_document.h"
#include "printing/printing_utils.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace printing {

namespace {

uint64_t GenerateFrameGuid(content::RenderFrameHost* render_frame_host) {
  int process_id = render_frame_host->GetProcess()->GetID();
  int frame_id = render_frame_host->GetRoutingID();
  return static_cast<uint64_t>(process_id) << 32 | frame_id;
}

// Converts a ContentToProxyTokenMap to ContentToFrameMap.
// ContentToProxyTokenMap maps content id to the frame token of its
// corresponding render frame proxy. This is generated when the content holder
// was created; ContentToFrameMap maps content id to its render frame's global
// unique id. The global unique id has the render process id concatenated with
// render frame routing id, which can uniquely identify a render frame.
ContentToFrameMap ConvertContentInfoMap(
    content::RenderFrameHost* render_frame_host,
    const ContentToProxyTokenMap& content_proxy_map) {
  ContentToFrameMap content_frame_map;
  int process_id = render_frame_host->GetProcess()->GetID();
  for (const auto& entry : content_proxy_map) {
    auto content_id = entry.first;
    auto proxy_token = entry.second;
    // Find the RenderFrameHost that the proxy id corresponds to.
    content::RenderFrameHost* rfh =
        content::RenderFrameHost::FromPlaceholderToken(
            process_id, blink::RemoteFrameToken(proxy_token));
    if (!rfh) {
      // If the corresponding RenderFrameHost cannot be found, just skip it.
      continue;
    }

    // Store this frame's global unique id into the map.
    content_frame_map[content_id] = GenerateFrameGuid(rfh);
  }
  return content_frame_map;
}

}  // namespace

PrintCompositeClient::PrintCompositeClient(content::WebContents* web_contents)
    : content::WebContentsUserData<PrintCompositeClient>(*web_contents),
      content::WebContentsObserver(web_contents) {}

PrintCompositeClient::~PrintCompositeClient() = default;

void PrintCompositeClient::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  if (document_cookie_ == 0) {
    DCHECK(!initiator_frame_);
    return;
  }

  if (initiator_frame_ == render_frame_host) {
    RemoveCompositeRequest(document_cookie_);
    return;
  }

  auto iter = pending_subframes_.find(render_frame_host);
  if (iter != pending_subframes_.end()) {
    // When a subframe we are expecting is deleted, we should notify the print
    // compositor service.
    auto* compositor = GetCompositeRequest(document_cookie_);
    compositor->NotifyUnavailableSubframe(GenerateFrameGuid(render_frame_host));
    pending_subframes_.erase(iter);
  }

  print_render_frames_.erase(render_frame_host);
}

PrintCompositeClient::RequestedSubFrame::RequestedSubFrame(
    content::GlobalRenderFrameHostId rfh_id,
    int document_cookie,
    mojom::DidPrintContentParamsPtr params,
    bool is_live)
    : rfh_id_(rfh_id),
      document_cookie_(document_cookie),
      params_(std::move(params)),
      is_live_(is_live) {}

PrintCompositeClient::RequestedSubFrame::~RequestedSubFrame() = default;

void PrintCompositeClient::OnDidPrintFrameContent(
    content::GlobalRenderFrameHostId rfh_id,
    int document_cookie,
    mojom::DidPrintContentParamsPtr params) {
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
    outer_client->OnDidPrintFrameContent(rfh_id, document_cookie,
                                         std::move(params));
    return;
  }

  if (!IsDocumentCookieValid(document_cookie)) {
    if (!compositor_) {
      // Queues the subframe information to |requested_subframes_| to handle it
      // after |compositor_| is created by the main frame.
      requested_subframes_.insert(std::make_unique<RequestedSubFrame>(
          rfh_id, document_cookie, std::move(params), /*is_live=*/true));
    }
    return;
  }

  auto* render_frame_host = content::RenderFrameHost::FromID(rfh_id);
  if (!render_frame_host)
    return;

  // Content in |params| is sent from untrusted source; only minimal processing
  // is done here. Most of it will be directly forwarded to print compositor
  // service.
  auto* compositor = GetCompositeRequest(document_cookie);
  auto region = params->metafile_data_region.Duplicate();
  compositor->AddSubframeContent(
      GenerateFrameGuid(render_frame_host), std::move(region),
      ConvertContentInfoMap(render_frame_host, params->subframe_content_info));

  // Update our internal states about this frame.
  pending_subframes_.erase(render_frame_host);
  printed_subframes_.insert(render_frame_host);
}

void PrintCompositeClient::SetAccessibilityTree(
    int document_cookie,
    const ui::AXTreeUpdate& accessibility_tree) {
  if (!IsDocumentCookieValid(document_cookie))
    return;

  auto* compositor = GetCompositeRequest(document_cookie);
  compositor->SetAccessibilityTree(accessibility_tree);
}

void PrintCompositeClient::PrintCrossProcessSubframe(
    const gfx::Rect& rect,
    int document_cookie,
    content::RenderFrameHost* subframe_host) {
  auto params = mojom::PrintFrameContentParams::New(rect, document_cookie);
  if (!subframe_host->IsRenderFrameLive()) {
    if (!IsDocumentCookieValid(document_cookie)) {
      if (!compositor_) {
        // Queues the subframe information to |requested_subframes_| to handle
        // it after |compositor_| is created by the main frame.
        requested_subframes_.insert(std::make_unique<RequestedSubFrame>(
            subframe_host->GetGlobalId(), document_cookie, /*params=*/nullptr,
            /*is_live=*/false));
      }
      return;
    }

    // When the subframe is dead, no need to send message,
    // just notify the service.
    auto* compositor = GetCompositeRequest(document_cookie);
    compositor->NotifyUnavailableSubframe(GenerateFrameGuid(subframe_host));
    return;
  }

  // If this frame is already printed, no need to print again.
  if (base::Contains(pending_subframes_, subframe_host) ||
      base::Contains(printed_subframes_, subframe_host)) {
    return;
  }

  // Send the request to the destination frame.
  GetPrintRenderFrame(subframe_host)
      ->PrintFrameContent(
          std::move(params),
          base::BindOnce(&PrintCompositeClient::OnDidPrintFrameContent,
                         weak_ptr_factory_.GetWeakPtr(),
                         subframe_host->GetGlobalId()));
  pending_subframes_.insert(subframe_host);
}

void PrintCompositeClient::CompositePage(
    int document_cookie,
    content::RenderFrameHost* render_frame_host,
    const mojom::DidPrintContentParams& content,
    mojom::PrintCompositor::CompositePageCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!IsDocumentCookieValid(document_cookie))
    return;

  auto* compositor = GetCompositeRequest(document_cookie);
  auto region = content.metafile_data_region.Duplicate();
  compositor->CompositePage(
      GenerateFrameGuid(render_frame_host), std::move(region),
      ConvertContentInfoMap(render_frame_host, content.subframe_content_info),
      base::BindOnce(&PrintCompositeClient::OnDidCompositePage,
                     std::move(callback)));
}

void PrintCompositeClient::PrepareToCompositeDocument(
    int document_cookie,
    content::RenderFrameHost* render_frame_host,
    mojom::PrintCompositor::DocumentType document_type,
    mojom::PrintCompositor::PrepareToCompositeDocumentCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!GetIsDocumentConcurrentlyComposited(document_cookie));

  auto* compositor =
      CreateCompositeRequest(document_cookie, render_frame_host, document_type);
  is_doc_concurrently_composited_ = true;
  compositor->PrepareToCompositeDocument(
      document_type,
      base::BindOnce(&PrintCompositeClient::OnDidPrepareToCompositeDocument,
                     std::move(callback)));
}

void PrintCompositeClient::FinishDocumentComposition(
    int document_cookie,
    uint32_t pages_count,
    mojom::PrintCompositor::FinishDocumentCompositionCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(GetIsDocumentConcurrentlyComposited(document_cookie));

  if (!IsDocumentCookieValid(document_cookie))
    return;

  auto* compositor = GetCompositeRequest(document_cookie);

  // Since this class owns compositor, compositor will be gone when this class
  // is destructed. Mojo won't call its callback in that case so it is safe to
  // use unretained |this| pointer here.
  compositor->FinishDocumentComposition(
      pages_count,
      base::BindOnce(&PrintCompositeClient::OnDidFinishDocumentComposition,
                     base::Unretained(this), document_cookie,
                     std::move(callback)));
}

void PrintCompositeClient::CompositeDocument(
    int document_cookie,
    content::RenderFrameHost* render_frame_host,
    const mojom::DidPrintContentParams& content,
    const ui::AXTreeUpdate& accessibility_tree,
    mojom::GenerateDocumentOutline generate_document_outline,
    mojom::PrintCompositor::DocumentType document_type,
    mojom::PrintCompositor::CompositeDocumentCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!GetIsDocumentConcurrentlyComposited(document_cookie));

  auto* compositor =
      CreateCompositeRequest(document_cookie, render_frame_host, document_type);
  compositor->SetAccessibilityTree(accessibility_tree);
  compositor->SetGenerateDocumentOutline(generate_document_outline);

  for (auto& requested : requested_subframes_) {
    if (!IsDocumentCookieValid(requested->document_cookie_))
      continue;
    if (requested->is_live_) {
      OnDidPrintFrameContent(requested->rfh_id_, requested->document_cookie_,
                             std::move(requested->params_));
    } else {
      compositor->NotifyUnavailableSubframe(GenerateFrameGuid(
          content::RenderFrameHost::FromID(requested->rfh_id_)));
    }
  }
  requested_subframes_.clear();

  auto region = content.metafile_data_region.Duplicate();

  // Since this class owns compositor, compositor will be gone when this class
  // is destructed. Mojo won't call its callback in that case so it is safe to
  // use unretained |this| pointer here.
  compositor->CompositeDocument(
      GenerateFrameGuid(render_frame_host), std::move(region),
      ConvertContentInfoMap(render_frame_host, content.subframe_content_info),
      document_type,
      base::BindOnce(&PrintCompositeClient::OnDidCompositeDocument,
                     base::Unretained(this), document_cookie,
                     std::move(callback)));
}

// static
void PrintCompositeClient::OnDidCompositePage(
    mojom::PrintCompositor::CompositePageCallback callback,
    mojom::PrintCompositor::Status status,
    base::ReadOnlySharedMemoryRegion region) {
  std::move(callback).Run(status, std::move(region));
}

void PrintCompositeClient::OnDidCompositeDocument(
    int document_cookie,
    mojom::PrintCompositor::CompositeDocumentCallback callback,
    mojom::PrintCompositor::Status status,
    base::ReadOnlySharedMemoryRegion region) {
  RemoveCompositeRequest(document_cookie);
  std::move(callback).Run(status, std::move(region));
}

// static
void PrintCompositeClient::OnDidPrepareToCompositeDocument(
    mojom::PrintCompositor::PrepareToCompositeDocumentCallback callback,
    mojom::PrintCompositor::Status status) {
  std::move(callback).Run(status);
}

void PrintCompositeClient::OnDidFinishDocumentComposition(
    int document_cookie,
    mojom::PrintCompositor::FinishDocumentCompositionCallback callback,
    mojom::PrintCompositor::Status status,
    base::ReadOnlySharedMemoryRegion region) {
  RemoveCompositeRequest(document_cookie);
  std::move(callback).Run(status, std::move(region));
}

bool PrintCompositeClient::GetIsDocumentConcurrentlyComposited(
    int cookie) const {
  return is_doc_concurrently_composited_ && document_cookie_ == cookie;
}

mojom::PrintCompositor* PrintCompositeClient::CreateCompositeRequest(
    int cookie,
    content::RenderFrameHost* initiator_frame,
    mojom::PrintCompositor::DocumentType document_type) {
  DCHECK(initiator_frame);

  if (document_cookie_ != 0) {
    DCHECK_NE(document_cookie_, cookie);
    RemoveCompositeRequest(document_cookie_);
  }
  document_cookie_ = cookie;

  // Track which frame kicked off the composite request.
  initiator_frame_ = initiator_frame;

  compositor_ = content::ServiceProcessHost::Launch<mojom::PrintCompositor>(
      content::ServiceProcessHost::Options()
          .WithDisplayName(IDS_PRINT_COMPOSITOR_SERVICE_DISPLAY_NAME)
          .Pass());

  compositor_->SetTitle(base::UTF16ToUTF8(web_contents()->GetTitle()));
  compositor_->SetWebContentsURL(web_contents()->GetLastCommittedURL());
  compositor_->SetUserAgent(user_agent_);

  return compositor_.get();
}

void PrintCompositeClient::RemoveCompositeRequest(int cookie) {
  DCHECK_EQ(document_cookie_, cookie);
  compositor_.reset();
  document_cookie_ = PrintSettings::NewInvalidCookie();
  initiator_frame_ = nullptr;

  // Reset state of the client.
  pending_subframes_.clear();
  printed_subframes_.clear();
  print_render_frames_.clear();

  // No longer concurrently compositing this document.
  is_doc_concurrently_composited_ = false;
}

bool PrintCompositeClient::IsDocumentCookieValid(int document_cookie) const {
  return document_cookie != 0 && document_cookie == document_cookie_;
}

mojom::PrintCompositor* PrintCompositeClient::GetCompositeRequest(
    int cookie) const {
  DCHECK(IsDocumentCookieValid(cookie));
  DCHECK(compositor_.is_bound());
  return compositor_.get();
}

const mojo::AssociatedRemote<mojom::PrintRenderFrame>&
PrintCompositeClient::GetPrintRenderFrame(content::RenderFrameHost* rfh) {
  auto it = print_render_frames_.find(rfh);
  if (it == print_render_frames_.end()) {
    mojo::AssociatedRemote<mojom::PrintRenderFrame> remote;
    rfh->GetRemoteAssociatedInterfaces()->GetInterface(&remote);
    it = print_render_frames_.emplace(rfh, std::move(remote)).first;
  }

  return it->second;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PrintCompositeClient);

}  // namespace printing
