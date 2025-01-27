// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/page_content_proto_provider.h"

#include "base/functional/concurrent_closures.h"
#include "base/metrics/histogram_macros.h"
#include "components/optimization_guide/content/browser/page_content_proto_util.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace optimization_guide {

namespace {

void ApplyOptionsOverridesForWebContents(
    content::WebContents* web_contents,
    blink::mojom::AIPageContentOptions& options) {
  // TODO(crbug.com/389735650): Renderers with no visible Documents will
  // throttle idle tasks after a duration of 10 seconds. In order to avoid the
  // page content request from getting starved, force critical path for hidden
  // WebContents.
  if (web_contents->GetVisibility() != content::Visibility::VISIBLE) {
    options.on_critical_path = true;
  }
}

blink::mojom::AIPageContentOptionsPtr ApplyOptionsOverridesForSubframe(
    const content::RenderProcessHost* main_process,
    const content::RenderProcessHost* subframe_process,
    const blink::mojom::AIPageContentOptions& input) {
  if (main_process == subframe_process) {
    return input.Clone();
  }

  // TODO(crbug.com/389737599): There's a bug with scheduling idle tasks in an
  // OOPIF with site isolation if there are no other main frames in the process.
  // See crbug.com/40785325.
  auto new_options = blink::mojom::AIPageContentOptions::New(input);
  new_options->on_critical_path = true;
  return new_options;
}

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

int CountContentNodes(const optimization_guide::proto::ContentNode& node) {
  int node_count = 1;
  for (const auto& child : node.children_nodes()) {
    node_count += CountContentNodes(child);
  }
  return node_count;
}

void OnGotAIPageContentForAllFrames(
    base::TimeTicks start_time,
    content::GlobalRenderFrameHostToken main_frame_token,
    std::unique_ptr<optimization_guide::AIPageContentMap> page_content_map,
    OnAIPageContentDone done_callback) {
  optimization_guide::proto::AnnotatedPageContent proto;
  if (optimization_guide::ConvertAIPageContentToProto(
          main_frame_token, *page_content_map,
          base::BindRepeating(&GetRenderFrameInfo), &proto)) {
    UMA_HISTOGRAM_TIMES("OptimizationGuide.AIPageContent.TotalLatency",
                        base::TimeTicks::Now() - start_time);
    // 10KB bucket up to 5MB.
    // TODO(crbug.com/392115749): Use provided metrics when available.
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "OptimizationGuide.AnnotatedPageContent.TotalSize2",
        proto.ByteSizeLong() / 1024, 10, 5000, 50);
    auto node_count = CountContentNodes(proto.root_node());
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "OptimizationGuide.AnnotatedPageContent.TotalNodeCount", node_count, 1,
        100000, 50);
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

blink::mojom::AIPageContentOptionsPtr DefaultAIPageContentOptions() {
  auto request = blink::mojom::AIPageContentOptions::New();
  request->include_geometry = true;
  request->on_critical_path = true;
  request->include_hidden_searchable_content = true;

  return request;
}

void GetAIPageContent(content::WebContents* web_contents,
                      blink::mojom::AIPageContentOptionsPtr options,
                      OnAIPageContentDone done_callback) {
  DCHECK(web_contents);
  DCHECK(web_contents->GetPrimaryMainFrame());

  if (!web_contents->GetPrimaryMainFrame()->IsRenderFrameLive()) {
    std::move(done_callback).Run(std::nullopt);
    return;
  }

  ApplyOptionsOverridesForWebContents(web_contents, *options);
  auto page_content_map =
      std::make_unique<optimization_guide::AIPageContentMap>();
  base::ConcurrentClosures concurrent;
  const auto* main_frame_rph =
      web_contents->GetPrimaryMainFrame()->GetProcess();

  web_contents->GetPrimaryMainFrame()->ForEachRenderFrameHost(
      [&](content::RenderFrameHost* rfh) {
        if (!rfh->IsRenderFrameLive()) {
          return;
        }

        auto* parent_frame = rfh->GetParentOrOuterDocument();

        // Skip dispatching IPCs for non-local root frames. The local root
        // provides data for itself and all child local frames.
        const bool is_local_root =
            !parent_frame ||
            parent_frame->GetRenderWidgetHost() != rfh->GetRenderWidgetHost();
        if (!is_local_root) {
          return;
        }

        const bool is_subframe = parent_frame != nullptr;
        auto options_to_use =
            is_subframe ? ApplyOptionsOverridesForSubframe(
                              main_frame_rph, rfh->GetProcess(), *options)
                        : options.Clone();

        mojo::Remote<blink::mojom::AIPageContentAgent> agent;
        rfh->GetRemoteInterfaces()->GetInterface(
            agent.BindNewPipeAndPassReceiver());
        auto* agent_ptr = agent.get();
        agent_ptr->GetAIPageContent(
            std::move(options_to_use),
            mojo::WrapCallbackWithDefaultInvokeIfNotRun(
                base::BindOnce(&OnGotAIPageContentForFrame,
                               rfh->GetGlobalFrameToken(), std::move(agent),
                               page_content_map.get(),
                               concurrent.CreateClosure()),
                nullptr));
      });

  std::move(concurrent)
      .Done(base::BindOnce(
          &OnGotAIPageContentForAllFrames, base::TimeTicks::Now(),
          web_contents->GetPrimaryMainFrame()->GetGlobalFrameToken(),
          std::move(page_content_map), std::move(done_callback)));
}

}  // namespace optimization_guide
