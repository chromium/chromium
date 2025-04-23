// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/page_content_proto_provider.h"

#include "base/check.h"
#include "base/functional/concurrent_closures.h"
#include "base/i18n/char_iterator.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/timer/elapsed_timer.h"
#include "components/optimization_guide/content/browser/page_content_proto_util.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace optimization_guide {

namespace {

// The maximum limit for computing the metrics.
constexpr size_t kMaxWordLimit = 100000;
constexpr size_t kMaxNodeLimit = 100000;

struct ContentNodeMetrics {
  size_t word_count = 0;
  size_t node_count = 0;
};

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

  if (base::FeatureList::IsEnabled(
          features::kAnnotatedPageContentWithActionableElements)) {
    options.enable_experimental_actionable_data = true;
    options.include_geometry = true;
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

  auto serialized_server_token =
      DocumentIdentifierUserData::GetDocumentIdentifier(
          render_frame_host->GetGlobalFrameToken());
  CHECK(serialized_server_token.has_value());

  optimization_guide::RenderFrameInfo render_frame_info;
  render_frame_info.serialized_server_token = serialized_server_token.value();
  render_frame_info.global_frame_token =
      render_frame_host->GetGlobalFrameToken();
  // We use the origin instead of last committed URL here to ensure the security
  // origin for the iframe's content is accurately tracked.
  // For example:
  // 1. data URLs use an opaque origin
  // 2. about:blank inherits its origin from the initiator while the URL doesn't
  //    convey that.
  render_frame_info.source_origin = render_frame_host->GetLastCommittedOrigin();
  render_frame_info.url = render_frame_host->GetLastCommittedURL();
  return render_frame_info;
}

// Computes the metrics in one pass.
void ComputeContentNodeMetrics(
    const optimization_guide::proto::ContentNode& content_node,
    ContentNodeMetrics* metrics) {
  bool is_previous_char_whitespace = true;
  for (base::i18n::UTF8CharIterator iter(
           content_node.content_attributes().text_data().text_content());
       metrics->word_count < kMaxWordLimit && !iter.end(); iter.Advance()) {
    bool is_current_char_whitespace = base::IsUnicodeWhitespace(iter.get());
    if (is_previous_char_whitespace && !is_current_char_whitespace) {
      // Count the start of the word.
      ++metrics->word_count;
    }
    is_previous_char_whitespace = is_current_char_whitespace;
  }
  metrics->node_count += 1;

  for (const auto& child : content_node.children_nodes()) {
    ComputeContentNodeMetrics(child, metrics);
    if (metrics->node_count > kMaxNodeLimit) {
      break;
    }
  }
}

void RecordPageContentExtractionMetrics(
    base::TimeDelta total_latency,
    ukm::SourceId source_id,
    optimization_guide::proto::AnnotatedPageContent proto) {
  ContentNodeMetrics metrics;
  auto total_size = proto.ByteSizeLong();
  base::ElapsedTimer elapsed;

  ComputeContentNodeMetrics(proto.root_node(), &metrics);
  UMA_HISTOGRAM_TIMES("OptimizationGuide.AIPageContent.TotalLatency",
                      total_latency);
  // 10KB bucket up to 5MB.
  // TODO(crbug.com/392115749): Use provided metrics when available.
  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "OptimizationGuide.AnnotatedPageContent.TotalSize2", total_size / 1024,
      10, 5000, 50);
  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "OptimizationGuide.AnnotatedPageContent.TotalNodeCount",
      metrics.node_count, 1, kMaxNodeLimit, 50);
  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "OptimizationGuide.AnnotatedPageContent.TotalWordCount",
      metrics.word_count, 1, kMaxWordLimit, 50);

  ukm::builders::OptimizationGuide_AnnotatedPageContent(source_id)
      .SetTotalSize(ukm::GetExponentialBucketMinForBytes(total_size))
      .SetExtractionLatency(ukm::GetExponentialBucketMinForUserTiming(
          total_latency.InMilliseconds()))
      .SetWordsCount(ukm::GetExponentialBucketMinForBytes(metrics.word_count))
      .SetNodeCount(ukm::GetExponentialBucketMinForBytes(metrics.node_count))
      .Record(ukm::UkmRecorder::Get());

  UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
      "OptimizationGuide.AnnotatedPageContent.ComputeMetricsLatency",
      elapsed.Elapsed(), base::Microseconds(1), base::Milliseconds(5), 50);
}

// Converts gfx::Size to optimization_guide::proto::ViewportGeometry.
void ConvertViewportGeometry(
    const gfx::Size& viewport,
    optimization_guide::proto::BoundingRect* viewport_geometry) {
  viewport_geometry->set_x(0);
  viewport_geometry->set_y(0);
  viewport_geometry->set_width(viewport.width());
  viewport_geometry->set_height(viewport.height());
}

void OnGotAIPageContentForAllFrames(
    blink::mojom::AIPageContentOptionsPtr main_frame_options,
    base::ElapsedTimer elapsed_timer,
    content::GlobalRenderFrameHostToken main_frame_token,
    ukm::SourceId source_id,
    const gfx::Size& main_frame_viewport,
    std::unique_ptr<optimization_guide::AIPageContentMap> page_content_map,
    OnAIPageContentDone done_callback) {
  optimization_guide::AIPageContentResult page_content;
  optimization_guide::FrameTokenSet frame_token_set;

  if (!optimization_guide::ConvertAIPageContentToProto(
          std::move(main_frame_options), main_frame_token, *page_content_map,
          base::BindRepeating(&GetRenderFrameInfo), frame_token_set,
          page_content)) {
    std::move(done_callback).Run(std::nullopt);
    return;
  }

  ConvertViewportGeometry(main_frame_viewport,
                          page_content.proto.mutable_viewport_geometry());

  // Get all the document identifiers for the frames that were seen.
  for (const auto& frame_token : frame_token_set) {
    content::RenderFrameHost* render_frame_host =
        content::RenderFrameHost::FromFrameToken(frame_token);
    CHECK(render_frame_host);
    auto token = DocumentIdentifierUserData::GetDocumentIdentifier(frame_token);
    CHECK(token.has_value());
    page_content.document_identifiers[token.value()] =
        render_frame_host->GetWeakDocumentPtr();
  }

  base::ThreadPool::PostTask(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT},
      base::BindOnce(RecordPageContentExtractionMetrics,
                     elapsed_timer.Elapsed(), source_id, page_content.proto));
  std::move(done_callback).Run(std::move(page_content));
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

AIPageContentResult::AIPageContentResult() {
  metadata = optimization_guide::mojom::PageMetadata::New();
}
AIPageContentResult::~AIPageContentResult() = default;
AIPageContentResult::AIPageContentResult(AIPageContentResult&& other) = default;
AIPageContentResult& AIPageContentResult::operator=(
    AIPageContentResult&& other) = default;

blink::mojom::AIPageContentOptionsPtr DefaultAIPageContentOptions() {
  return blink::mojom::AIPageContentOptions::New();
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
          &OnGotAIPageContentForAllFrames, std::move(options),
          base::ElapsedTimer(),
          web_contents->GetPrimaryMainFrame()->GetGlobalFrameToken(),
          web_contents->GetPrimaryMainFrame()->GetPageUkmSourceId(),
          web_contents->GetSize(), std::move(page_content_map),
          std::move(done_callback)));
}

// Allows for a DocumentIdentifier to be reused across calls to convert
std::optional<std::string> DocumentIdentifierUserData::GetDocumentIdentifier(
    content::GlobalRenderFrameHostToken token) {
  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromFrameToken(token);
  if (!render_frame_host) {
    return std::nullopt;
  }
  return DocumentIdentifierUserData::GetOrCreateForCurrentDocument(
             render_frame_host)
      ->serialized_token();
}

DOCUMENT_USER_DATA_KEY_IMPL(DocumentIdentifierUserData);

}  // namespace optimization_guide
