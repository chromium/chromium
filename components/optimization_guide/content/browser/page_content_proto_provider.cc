// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/page_content_proto_provider.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/barrier_closure.h"
#include "base/cancelable_callback.h"
#include "base/check.h"
#include "base/functional/callback.h"
#include "base/functional/concurrent_closures.h"
#include "base/i18n/char_iterator.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/timer/elapsed_timer.h"
#include "base/types/expected.h"
#include "components/optimization_guide/content/browser/autofill_annotations_provider.h"
#include "components/optimization_guide/content/browser/media_transcript_provider.h"
#include "components/optimization_guide/content/browser/page_content_proto_util.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/schemeful_site.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom-data-view.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom-forward.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom.h"

namespace optimization_guide {

namespace {

// The maximum limit for computing the metrics.
constexpr size_t kMaxWordLimit = 100000;
constexpr size_t kMaxNodeLimit = 100000;

struct ContentNodeMetrics {
  size_t word_count = 0;
  size_t node_count = 0;
};

// To avoid blocking on subframe responses for too long, we proceed on a timeout
// (if provided) as long as we have received a response from the main frame.
class GetAIPageContentTimeoutHelper {
 public:
  GetAIPageContentTimeoutHelper()
      : page_content_map_(
            std::make_unique<optimization_guide::AIPageContentMap>()) {}

  // Must be called before `Start()`.
  base::OnceClosure GetClosureForSubframe() {
    return concurrent_for_subframes_.CreateClosure();
  }
  base::OnceClosure GetClosureForMainFrame() {
    return base::BindOnce(
        &GetAIPageContentTimeoutHelper::OnMainFrameRunOrTimeout,
        weak_ptr_factory_.GetWeakPtr());
  }

  void Start(base::OnceClosure callback,
             std::optional<base::TimeDelta> subframe_timeout,
             std::optional<base::TimeDelta> main_frame_timeout) {
    CHECK(callback);
    callback_ = std::move(callback);
    if (subframe_timeout.has_value()) {
      // `base::Unretained` is safe here because `this` owns
      // `subframe_timeout_timer_`.
      subframe_timeout_timer_.Start(
          FROM_HERE, subframe_timeout.value(),
          base::BindOnce(
              &GetAIPageContentTimeoutHelper::OnAllSubframesRespondedOrTimeout,
              base::Unretained(this)));
    }
    if (main_frame_timeout.has_value()) {
      // `base::Unretained` is safe here because `this` owns
      // `main_frame_timeout_timer_`.
      main_frame_timeout_timer_.Start(
          FROM_HERE, main_frame_timeout.value(),
          base::BindOnce(
              &GetAIPageContentTimeoutHelper::OnMainFrameRunOrTimeout,
              base::Unretained(this)));
    }
    std::move(concurrent_for_subframes_)
        .Done(base::BindOnce(
            &GetAIPageContentTimeoutHelper::OnAllSubframesRespondedOrTimeout,
            weak_ptr_factory_.GetWeakPtr()));
  }

  void OnGotAIPageContentForFrame(
      content::GlobalRenderFrameHostToken frame_token,
      mojo::Remote<blink::mojom::AIPageContentAgent> remote_interface,
      base::OnceClosure continue_callback,
      blink::mojom::AIPageContentPtr result) {
    CHECK(page_content_map_->find(frame_token) == page_content_map_->end());

    if (result) {
      (*page_content_map_)[frame_token] = std::move(result);
    }
    std::move(continue_callback).Run();
  }

  void OnMainFrameRunOrTimeout() {
    CHECK(!has_main_frame_run_or_timed_out_);
    has_main_frame_run_or_timed_out_ = true;

    if (have_all_subframes_responded_or_timed_out_) {
      std::move(callback_).Run();
    }
  }

  void OnAllSubframesRespondedOrTimeout() {
    have_all_subframes_responded_or_timed_out_ = true;
    subframe_timeout_timer_.Stop();
    if (has_main_frame_run_or_timed_out_ && callback_) {
      std::move(callback_).Run();
    }
  }

  base::WeakPtr<GetAIPageContentTimeoutHelper> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  optimization_guide::AIPageContentMap* raw_page_content_map() {
    return page_content_map_.get();
  }

 private:
  base::OneShotTimer subframe_timeout_timer_;
  base::OneShotTimer main_frame_timeout_timer_;
  bool has_main_frame_run_or_timed_out_ = false;
  bool have_all_subframes_responded_or_timed_out_ = false;
  std::unique_ptr<optimization_guide::AIPageContentMap> page_content_map_;
  base::ConcurrentClosures concurrent_for_subframes_;
  base::OnceClosure callback_;
  base::WeakPtrFactory<GetAIPageContentTimeoutHelper> weak_ptr_factory_{this};
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
    options.mode = blink::mojom::AIPageContentMode::kActionableElements;
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
  auto new_options = input.Clone();
  new_options->on_critical_path = true;
  return new_options;
}

// Validate that the media session has all the required data before proceeding
// to create media data.
bool ValidateMediaSession(
    const media_session::mojom::MediaSessionInfoPtr& media_session_info,
    const std::optional<media_session::MediaPosition>& media_position) {
  return media_session_info && media_session_info->audio_video_states &&
         !media_session_info->audio_video_states->empty() && media_position &&
         !media_position->duration().is_zero();
}

// Find the media data from the web contents for the given render frame host if
// the media data exists, otherwise return nullopt.
std::optional<optimization_guide::proto::MediaData> ComputeMediaData(
    content::RenderFrameHost* render_frame_host) {
  CHECK(render_frame_host);
  if (!base::FeatureList::IsEnabled(
          features::kAnnotatedPageContentWithMediaData)) {
    return std::nullopt;
  }

  auto* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  if (!web_contents) {
    return std::nullopt;
  }

  // Populate the transcripts field in media data if the transcripts exist for
  // this render frame host. The transcripts could have been generated for
  // previous media sessions in this render frame host, or are being generated
  // for the currently active media session. The transcripts will be cleared
  // when the render frame host changes.
  std::optional<optimization_guide::proto::MediaData> media_data;
  if (auto* media_transcript_provider =
          MediaTranscriptProvider::GetFor(web_contents)) {
    auto transcripts =
        media_transcript_provider->GetTranscriptsForFrame(render_frame_host);
    if (!transcripts.empty()) {
      media_data.emplace();
      media_data->mutable_transcripts()->Add(transcripts.begin(),
                                             transcripts.end());
    }
  }

  // If there is an active media session in the web page, we may generate other
  // fields in media data if the given render frame host controls the media
  // session.
  auto* media_session = content::MediaSession::GetIfExists(web_contents);
  if (!media_session ||
      (render_frame_host != media_session->GetRoutedFrame())) {
    return media_data;
  }

  // Validate the media data for other fields before populating them.
  auto media_session_info = media_session->GetMediaSessionInfoSync();
  auto media_position = media_session->GetMediaSessionPosition();
  if (!ValidateMediaSession(media_session_info, media_position)) {
    return media_data;
  }

  // Initialize the media data if there are no transcripts so that it was not
  // initialized before.
  if (!media_data) {
    media_data.emplace();
  }

  media_data->set_is_playing(
      media_session_info->playback_state ==
      media_session::mojom::MediaPlaybackState::kPlaying);
  media_data->set_duration_milliseconds(
      media_position->duration().InMilliseconds());
  media_data->set_current_position_milliseconds(
      media_position->GetPosition().InMilliseconds());

  // Find the media data type via the audio video states in the media session
  // info. If there are multiple media in the frame which is rare, select the
  // first media for simplicity.
  auto& first_state = media_session_info->audio_video_states->at(0);
  switch (first_state) {
    case media_session::mojom::MediaAudioVideoState::kAudioOnly:
      media_data->set_media_data_type(
          optimization_guide::proto::MediaDataType::MEDIA_DATA_TYPE_AUDIO);
      break;
    case media_session::mojom::MediaAudioVideoState::kAudioVideo:
    case media_session::mojom::MediaAudioVideoState::kVideoOnly:
      media_data->set_media_data_type(
          optimization_guide::proto::MediaDataType::MEDIA_DATA_TYPE_VIDEO);
      break;
    case media_session::mojom::MediaAudioVideoState::kDeprecatedUnknown:
      NOTREACHED();
  }

  // Set the media metadata.
  const media_session::MediaMetadata& media_metadata =
      media_session->GetMediaSessionMetadata();
  media_data->set_title(base::UTF16ToUTF8(media_metadata.title));
  media_data->set_artist(base::UTF16ToUTF8(media_metadata.artist));
  media_data->set_album(base::UTF16ToUTF8(media_metadata.album));

  return media_data;
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
  render_frame_info.media_data = ComputeMediaData(render_frame_host);
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
    blink::mojom::AIPageContentMode mode,
    bool on_critical_path,
    optimization_guide::proto::AnnotatedPageContent proto) {
  ContentNodeMetrics metrics;
  auto total_size = proto.ByteSizeLong();
  base::ElapsedTimer elapsed;

  ComputeContentNodeMetrics(proto.root_node(), &metrics);
  UMA_HISTOGRAM_TIMES("OptimizationGuide.AIPageContent.TotalLatency",
                      total_latency);
  if (mode == blink::mojom::AIPageContentMode::kDefault) {
    if (on_critical_path) {
      UMA_HISTOGRAM_TIMES(
          "OptimizationGuide.AIPageContent.TotalLatency.Default.CriticalPath",
          total_latency);
    } else {
      UMA_HISTOGRAM_TIMES(
          "OptimizationGuide.AIPageContent.TotalLatency.Default."
          "NotCriticalPath",
          total_latency);
    }
  } else if (mode == blink::mojom::AIPageContentMode::kActionableElements) {
    if (on_critical_path) {
      UMA_HISTOGRAM_TIMES(
          "OptimizationGuide.AIPageContent.TotalLatency.ActionableElements."
          "CriticalPath",
          total_latency);
    } else {
      UMA_HISTOGRAM_TIMES(
          "OptimizationGuide.AIPageContent.TotalLatency.ActionableElements."
          "NotCriticalPath",
          total_latency);
    }
  }
  // 10KB bucket up to 5MB.
  // TODO(crbug.com/392115749): Use provided metrics when available.
  if (mode == blink::mojom::AIPageContentMode::kDefault) {
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "OptimizationGuide.AnnotatedPageContent.TotalSize2.Default",
        total_size / 1024, 10, 5000, 50);
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "OptimizationGuide.AnnotatedPageContent.TotalNodeCount.Default",
        metrics.node_count, 1, kMaxNodeLimit, 50);
  } else if (mode == blink::mojom::AIPageContentMode::kActionableElements) {
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "OptimizationGuide.AnnotatedPageContent.TotalSize2.ActionableElements",
        total_size / 1024, 10, 5000, 50);
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "OptimizationGuide.AnnotatedPageContent.TotalNodeCount."
        "ActionableElements",
        metrics.node_count, 1, kMaxNodeLimit, 50);
  }
  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "OptimizationGuide.AnnotatedPageContent.TotalWordCount",
      metrics.word_count, 1, kMaxWordLimit, 50);

  ukm::builders::OptimizationGuide_AnnotatedPageContent(source_id)
      .SetMode(static_cast<int64_t>(mode))
      .SetOnCriticalPath(on_critical_path)
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

void OnGotAIPageContentOrTimedOutForAllFrames(
    blink::mojom::AIPageContentOptionsPtr main_frame_options,
    base::ElapsedTimer elapsed_timer,
    content::GlobalRenderFrameHostToken main_frame_token,
    ukm::SourceId source_id,
    const gfx::Size& main_frame_viewport,
    std::unique_ptr<GetAIPageContentTimeoutHelper> timeout_helper,
    OnAIPageContentDone done_callback) {
  optimization_guide::AIPageContentResult page_content;
  optimization_guide::FrameTokenSet frame_token_set;
  auto mode = main_frame_options->mode;
  bool on_critical_path = main_frame_options->on_critical_path;

  if (auto result = optimization_guide::ConvertAIPageContentToProto(
          std::move(main_frame_options), main_frame_token,
          *timeout_helper->raw_page_content_map(),
          base::BindRepeating(&GetRenderFrameInfo), frame_token_set,
          page_content);
      !result.has_value()) {
    std::move(done_callback).Run(base::unexpected(result.error()));
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

  if (base::FeatureList::IsEnabled(
          features::kAnnotatedPageContentWithAutofillAnnotations)) {
    content::RenderFrameHost* render_frame_host =
        content::RenderFrameHost::FromFrameToken(main_frame_token);
    content::WebContents* web_contents =
        content::WebContents::FromRenderFrameHost(render_frame_host);
    if (auto* autofill_annotations_provider =
            AutofillAnnotationsProvider::GetFor(web_contents)) {
      AutofillAvailability autofill_availability =
          autofill_annotations_provider->GetAutofillAvailability(
              *render_frame_host);
      proto::AutofillInformation* autofill_information =
          page_content.proto.mutable_profile_information()
              ->mutable_autofill_information();
      if (autofill_availability.has_fillable_address) {
        autofill_information->add_fillable_data(
            proto::AutofillInformation_FillableData_ADDRESS);
      }
      if (autofill_availability.has_fillable_credit_card) {
        autofill_information->add_fillable_data(
            proto::AutofillInformation_FillableData_CREDIT_CARD);
      }
    }
  }

  base::ThreadPool::PostTask(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT},
      base::BindOnce(RecordPageContentExtractionMetrics,
                     elapsed_timer.Elapsed(), source_id, mode, on_critical_path,
                     page_content.proto));
  std::move(done_callback).Run(std::move(page_content));
}

}  // namespace

AIPageContentResult::AIPageContentResult() {
  metadata = blink::mojom::PageMetadata::New();
}
AIPageContentResult::~AIPageContentResult() = default;
AIPageContentResult::AIPageContentResult(AIPageContentResult&& other) = default;
AIPageContentResult& AIPageContentResult::operator=(
    AIPageContentResult&& other) = default;

blink::mojom::AIPageContentOptionsPtr DefaultAIPageContentOptions(
    bool on_critical_path) {
  auto options = blink::mojom::AIPageContentOptions::New();
  options->mode = blink::mojom::AIPageContentMode::kDefault;
  options->on_critical_path = on_critical_path;
  return options;
}

blink::mojom::AIPageContentOptionsPtr ActionableAIPageContentOptions(
    bool on_critical_path) {
  auto options = blink::mojom::AIPageContentOptions::New();
  options->mode = blink::mojom::AIPageContentMode::kActionableElements;
  options->on_critical_path = on_critical_path;
  return options;
}

void GetAIPageContent(content::WebContents* web_contents,
                      blink::mojom::AIPageContentOptionsPtr options,
                      OnAIPageContentDone done_callback) {
  DCHECK(web_contents);
  DCHECK(web_contents->GetPrimaryMainFrame());

  if (!web_contents->GetPrimaryMainFrame()->IsRenderFrameLive()) {
    std::move(done_callback).Run(base::unexpected("Main frame not live"));
    return;
  }

  ApplyOptionsOverridesForWebContents(web_contents, *options);
  auto timeout_helper = std::make_unique<GetAIPageContentTimeoutHelper>();

  const auto* main_frame_rph =
      web_contents->GetPrimaryMainFrame()->GetProcess();

  const url::Origin& top_level_origin =
      web_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin();

  gfx::Rect main_frame_view_rect_dips;
  if (content::RenderWidgetHostView* rwhv =
          web_contents->GetRenderWidgetHostView()) {
    main_frame_view_rect_dips = rwhv->GetViewBounds();
  }

  web_contents->GetPrimaryMainFrame()->ForEachRenderFrameHost(
      [&](content::RenderFrameHost* rfh) {
        if (!rfh->IsRenderFrameLive()) {
          return;
        }

        auto* parent_frame = rfh->GetParentOrOuterDocumentOrEmbedder();
        content::GlobalRenderFrameHostToken frame_token =
            rfh->GetGlobalFrameToken();

        const url::Origin& frame_origin = rfh->GetLastCommittedOrigin();
        if (options->include_same_site_only &&
            (!net::SchemefulSite::IsSameSite(top_level_origin, frame_origin) ||
             rfh->IsFencedFrameRoot())) {
          CHECK(timeout_helper->raw_page_content_map()->find(frame_token) ==
                timeout_helper->raw_page_content_map()->end());
          (*timeout_helper->raw_page_content_map())[frame_token] =
              blink::mojom::RedactedFrameMetadata::New(
                  blink::mojom::RedactedFrameMetadata_Reason::kCrossSite);
          return;
        }

        // Skip dispatching IPCs for non-local root frames. The local root
        // provides data for itself and all child local frames.
        const bool is_local_root =
            !parent_frame ||
            parent_frame->GetRenderWidgetHost() != rfh->GetRenderWidgetHost();

        if (!is_local_root) {
          return;
        }

        // Also true for the main frame of a GuestView.
        const bool is_subframe = parent_frame != nullptr;
        auto options_to_use =
            is_subframe ? ApplyOptionsOverridesForSubframe(
                              main_frame_rph, rfh->GetProcess(), *options)
                        : options.Clone();
        base::OnceClosure continue_closure =
            is_subframe ? timeout_helper->GetClosureForSubframe()
                        : timeout_helper->GetClosureForMainFrame();

        options_to_use->main_frame_view_rect_in_dips =
            main_frame_view_rect_dips;

        mojo::Remote<blink::mojom::AIPageContentAgent> agent;
        rfh->GetRemoteInterfaces()->GetInterface(
            agent.BindNewPipeAndPassReceiver());
        auto* agent_ptr = agent.get();
        agent_ptr->GetAIPageContent(
            std::move(options_to_use),
            mojo::WrapCallbackWithDefaultInvokeIfNotRun(
                base::BindOnce(
                    &GetAIPageContentTimeoutHelper::OnGotAIPageContentForFrame,
                    timeout_helper->AsWeakPtr(), frame_token, std::move(agent),
                    std::move(continue_closure)),
                nullptr));
      });

  GetAIPageContentTimeoutHelper* timeout_helper_ptr = timeout_helper.get();

  timeout_helper_ptr->Start(
      base::BindOnce(&OnGotAIPageContentOrTimedOutForAllFrames, options.Clone(),
                     base::ElapsedTimer(),
                     web_contents->GetPrimaryMainFrame()->GetGlobalFrameToken(),
                     web_contents->GetPrimaryMainFrame()->GetPageUkmSourceId(),
                     web_contents->GetSize(), std::move(timeout_helper),
                     std::move(done_callback)),
      features::GetSubframeGetAIPageContentTimeout(),
      features::GetMainFrameGetAIPageContentTimeout());
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

DocumentIdentifierUserData::DocumentIdentifierUserData(
    content::RenderFrameHost* rfh)
    : DocumentUserData<DocumentIdentifierUserData>(rfh),
      token_(base::UnguessableToken::Create()),
      serialized_token_(token_.ToString()) {}

DocumentIdentifierUserData::~DocumentIdentifierUserData() = default;

}  // namespace optimization_guide
