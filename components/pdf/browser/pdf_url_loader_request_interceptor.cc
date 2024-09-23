// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pdf/browser/pdf_url_loader_request_interceptor.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "components/pdf/browser/pdf_stream_delegate.h"
#include "components/pdf/browser/plugin_response_writer.h"
#include "content/public/browser/url_loader_request_interceptor.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "services/network/public/mojom/url_loader.mojom-forward.h"
#include "url/gurl.h"

namespace pdf {

namespace {

void FinishLoader(std::unique_ptr<PluginResponseWriter> /*response_writer*/) {
  // Implicitly deletes `PluginResponseWriter` after loading completes.
}

void CreateLoaderAndStart(
    const PdfStreamDelegate::StreamInfo& stream_info,
    const network::ResourceRequest& request,
    mojo::PendingReceiver<network::mojom::URLLoader> receiver,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client) {
  auto response_writer =
      std::make_unique<PluginResponseWriter>(stream_info, std::move(client));

  auto* unowned_response_writer = response_writer.get();
  unowned_response_writer->Start(
      base::BindOnce(FinishLoader, std::move(response_writer)));
}

}  // namespace

// static
std::unique_ptr<content::URLLoaderRequestInterceptor>
PdfURLLoaderRequestInterceptor::MaybeCreateInterceptor(
    content::FrameTreeNodeId frame_tree_node_id,
    std::unique_ptr<PdfStreamDelegate> stream_delegate) {
  return std::make_unique<PdfURLLoaderRequestInterceptor>(
      frame_tree_node_id, std::move(stream_delegate));
}

PdfURLLoaderRequestInterceptor::PdfURLLoaderRequestInterceptor(
    content::FrameTreeNodeId frame_tree_node_id,
    std::unique_ptr<PdfStreamDelegate> stream_delegate)
    : frame_tree_node_id_(frame_tree_node_id),
      stream_delegate_(std::move(stream_delegate)) {}

PdfURLLoaderRequestInterceptor::~PdfURLLoaderRequestInterceptor() = default;

void PdfURLLoaderRequestInterceptor::MaybeCreateLoader(
    const network::ResourceRequest& tentative_resource_request,
    content::BrowserContext* browser_context,
    content::URLLoaderRequestInterceptor::LoaderCallback callback) {
  std::move(callback).Run(CreateRequestHandler(tentative_resource_request));
}

content::URLLoaderRequestInterceptor::RequestHandler
PdfURLLoaderRequestInterceptor::CreateRequestHandler(
    const network::ResourceRequest& tentative_resource_request) {
  // Only intercept navigation requests.
  if (tentative_resource_request.mode != network::mojom::RequestMode::kNavigate)
    return {};

  // Only intercept requests within a `MimeHandlerViewGuest` containing the PDF
  // viewer extension.
  content::WebContents* contents =
      content::WebContents::FromFrameTreeNodeId(frame_tree_node_id_);
  if (!contents)
    return {};

  // Normally, `content::WebContents::UnsafeFindFrameByFrameTreeNodeId()` should
  // not be used, since a FrameTreeNode's `RenderFrameHost` may change over its
  // lifetime. However, the only use for this `RenderFrameHost` is to get its
  // parent `RenderFrameHost`, which cannot change during the lifetime of the
  // FrameTreeNode.
  content::RenderFrameHost* content_frame =
      contents->UnsafeFindFrameByFrameTreeNodeId(frame_tree_node_id_);

  std::optional<PdfStreamDelegate::StreamInfo> stream =
      stream_delegate_->GetStreamInfo(content_frame->GetParent());
  if (!stream.has_value())
    return {};

  // Only intercept requests that are navigations to the original URL. The only
  // source of such requests should be `PdfNavigationThrottle`, but in the
  // worst case, we'll just navigate to the synthetic response again, which will
  // then fail to load the PDF from the single-use stream URL.
  if (tentative_resource_request.url != stream->original_url)
    return {};

  return base::BindOnce(&CreateLoaderAndStart, std::move(stream.value()));
}

}  // namespace pdf
