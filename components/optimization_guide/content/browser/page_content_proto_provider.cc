// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/page_content_proto_provider.h"

#include "base/functional/concurrent_closures.h"
#include "components/optimization_guide/content/browser/page_content_proto_util.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom.h"

namespace optimization_guide {

namespace {

std::optional<optimization_guide::RenderFrameInfo> GetRenderFrameInfo(
    int child_process_id,
    blink::FrameToken frame_token) {
  content::RenderFrameHost* render_frame_host = nullptr;

  if (frame_token.Is<blink::RemoteFrameToken>()) {
    render_frame_host = content::RenderFrameHost::FromPlaceholderToken(
        child_process_id, frame_token.GetAs<blink::RemoteFrameToken>());
  } else {
    render_frame_host = content::RenderFrameHost::FromFrameToken(
        content::GlobalRenderFrameHostToken(
            child_process_id, frame_token.GetAs<blink::LocalFrameToken>()));
  }

  if (!render_frame_host) {
    return std::nullopt;
  }

  optimization_guide::RenderFrameInfo render_frame_info;
  render_frame_info.global_frame_token =
      render_frame_host->GetGlobalFrameToken();
  // We use the origin instead of last committed URL here to ensure the security
  // origin for the iframe's content is accurately tracked.
  // For example, for data URLs we need the source origin for the URL instead of
  // the raw URL itself.
  render_frame_info.source_origin = render_frame_host->GetLastCommittedOrigin()
                                        .GetTupleOrPrecursorTupleIfOpaque();
  return render_frame_info;
}

void OnGotAIPageContentForAllFrames(
    content::GlobalRenderFrameHostToken main_frame_token,
    std::unique_ptr<optimization_guide::AIPageContentMap> page_content_map,
    OnAIPageContentDone done_callback) {
  optimization_guide::proto::AnnotatedPageContent proto;
  if (optimization_guide::ConvertAIPageContentToProto(
          main_frame_token, *page_content_map,
          base::BindRepeating(&GetRenderFrameInfo), &proto)) {
    std::move(done_callback).Run(std::move(proto));
    return;
  }

  std::move(done_callback).Run(std::nullopt);
}

void OnGotAIPageContentForFrame(
    content::GlobalRenderFrameHostToken frame_token,
    mojo::Remote<blink::mojom::AIPageContentAgent> remote_interface,
    optimization_guide::AIPageContentMap* page_content_map,
    base::OnceClosure continue_callback,
    blink::mojom::AIPageContentPtr result) {
  CHECK(page_content_map->find(frame_token) == page_content_map->end());

  if (result) {
    (*page_content_map)[frame_token] = std::move(result);
  }
  std::move(continue_callback).Run();
}

}  // namespace

void GetAIPageContent(content::WebContents* web_contents,
                      OnAIPageContentDone done_callback) {
  DCHECK(web_contents);
  DCHECK(web_contents->GetPrimaryMainFrame());

  auto page_content_map =
      std::make_unique<optimization_guide::AIPageContentMap>();
  base::ConcurrentClosures concurrent;

  web_contents->GetPrimaryMainFrame()->ForEachRenderFrameHost(
      [&](content::RenderFrameHost* rfh) {
        auto* parent_frame = rfh->GetParentOrOuterDocument();

        // Skip dispatching IPCs for non-local root frames. The local root
        // provides data for itself and all child local frames.
        const bool is_local_root =
            !parent_frame ||
            parent_frame->GetRenderWidgetHost() != rfh->GetRenderWidgetHost();
        if (!is_local_root) {
          return;
        }

        mojo::Remote<blink::mojom::AIPageContentAgent> agent;
        rfh->GetRemoteInterfaces()->GetInterface(
            agent.BindNewPipeAndPassReceiver());
        auto* agent_ptr = agent.get();
        agent_ptr->GetAIPageContent(mojo::WrapCallbackWithDefaultInvokeIfNotRun(
            base::BindOnce(&OnGotAIPageContentForFrame,
                           rfh->GetGlobalFrameToken(), std::move(agent),
                           page_content_map.get(), concurrent.CreateClosure()),
            nullptr));
      });

  std::move(concurrent)
      .Done(base::BindOnce(
          &OnGotAIPageContentForAllFrames,
          web_contents->GetPrimaryMainFrame()->GetGlobalFrameToken(),
          std::move(page_content_map), std::move(done_callback)));
}

}  // namespace optimization_guide
