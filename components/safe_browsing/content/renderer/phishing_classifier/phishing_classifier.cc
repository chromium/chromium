// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/renderer/phishing_classifier/phishing_classifier.h"

#include <memory>
#include <string>
#include <utility>

#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "cc/paint/paint_recorder.h"
#include "cc/paint/skia_paint_canvas.h"
#include "components/paint_preview/common/paint_preview_tracker.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/content/common/visual_utils.h"
#include "components/safe_browsing/content/renderer/phishing_classifier/features.h"
#include "components/safe_browsing/content/renderer/phishing_classifier/phishing_dom_feature_extractor.h"
#include "components/safe_browsing/content/renderer/phishing_classifier/phishing_term_feature_extractor.h"
#include "components/safe_browsing/content/renderer/phishing_classifier/phishing_url_feature_extractor.h"
#include "components/safe_browsing/content/renderer/phishing_classifier/scorer.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "crypto/sha2.h"
#include "skia/ext/legacy_display_globals.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_document_loader.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_view.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "url/gurl.h"

namespace safe_browsing {

namespace {

std::unique_ptr<SkBitmap> PlaybackOnBackgroundThread(
    cc::PaintRecord paint_record,
    gfx::Rect bounds) {
  // Use the Rec. 2020 color space, in case the user input is wide-gamut.
  std::unique_ptr<SkBitmap> bitmap = std::make_unique<SkBitmap>();
  sk_sp<SkColorSpace> rec2020 = SkColorSpace::MakeRGB(
      {2.22222f, 0.909672f, 0.0903276f, 0.222222f, 0.0812429f, 0, 0},
      SkNamedGamut::kRec2020);
  SkImageInfo bitmap_info = SkImageInfo::Make(
      bounds.width(), bounds.height(), SkColorType::kN32_SkColorType,
      SkAlphaType::kUnpremul_SkAlphaType, rec2020);
  if (!bitmap->tryAllocPixels(bitmap_info))
    return nullptr;

  SkCanvas sk_canvas(*bitmap, skia::LegacyDisplayGlobals::GetSkSurfaceProps());
  paint_record.Playback(&sk_canvas);
  return bitmap;
}

}  // namespace

const float PhishingClassifier::kInvalidScore = -1.0;
const float PhishingClassifier::kPhishyThreshold = 0.5;

PhishingClassifier::PhishingClassifier(content::RenderFrame* render_frame)
    : render_frame_(render_frame) {
  Clear();
}

PhishingClassifier::~PhishingClassifier() {
  // The RenderView should have called CancelPendingClassification() before
  // we are destroyed.
  DCHECK(done_callback_.is_null());
  DCHECK(!page_text_);
}

bool PhishingClassifier::is_ready() const {
  return !!ScorerStorage::GetInstance()->GetScorer();
}

void PhishingClassifier::BeginClassification(const std::u16string* page_text,
                                             DoneCallback done_callback) {
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("safe_browsing", "PhishingClassification",
                                    this);
  DCHECK(is_ready());

  // The RenderView should have called CancelPendingClassification() before
  // starting a new classification, so DCHECK this.
  DCHECK(done_callback_.is_null());
  DCHECK(!page_text_);
  // However, in an opt build, we will go ahead and clean up the pending
  // classification so that we can start in a known state.
  CancelPendingClassification();

  Scorer* scorer = ScorerStorage::GetInstance()->GetScorer();
  url_extractor_ = std::make_unique<PhishingUrlFeatureExtractor>();
  dom_extractor_ = std::make_unique<PhishingDOMFeatureExtractor>();
  term_extractor_ = std::make_unique<PhishingTermFeatureExtractor>(
      scorer->find_page_term_callback(), scorer->find_page_word_callback(),
      scorer->max_words_per_term(), scorer->murmurhash3_seed(),
      scorer->max_shingles_per_page(), scorer->shingle_size());
  page_text_ = page_text;
  done_callback_ = std::move(done_callback);

  // For consistency, we always want to invoke the DoneCallback
  // asynchronously, rather than directly from this method.  To ensure that
  // this is the case, post a task to begin feature extraction on the next
  // iteration of the message loop.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&PhishingClassifier::BeginFeatureExtraction,
                                weak_factory_.GetWeakPtr()));
}

void PhishingClassifier::BeginFeatureExtraction() {
  blink::WebLocalFrame* frame = render_frame_->GetWebFrame();

  // Check whether the URL is one that we should classify.
  // Currently, we only classify http/https URLs that are GET requests.
  GURL url(frame->GetDocument().Url());
  if (!url.SchemeIsHTTPOrHTTPS()) {
    RunFailureCallback();
    return;
  }

  blink::WebDocumentLoader* document_loader = frame->GetDocumentLoader();
  if (!document_loader || document_loader->HttpMethod().Ascii() != "GET") {
    RunFailureCallback();
    return;
  }

  features_ = std::make_unique<FeatureMap>();
  if (!url_extractor_->ExtractFeatures(url, features_.get())) {
    RunFailureCallback();
    return;
  }

  // DOM feature extraction can take awhile, so it runs asynchronously
  // in several chunks of work and invokes the callback when finished.
  dom_extractor_->ExtractFeatures(
      frame->GetDocument(), features_.get(),
      base::BindOnce(&PhishingClassifier::DOMExtractionFinished,
                     base::Unretained(this)));
}

void PhishingClassifier::CancelPendingClassification() {
  // Note that cancelling the feature extractors is simply a no-op if they
  // were not running.
  DCHECK(is_ready());
  dom_extractor_.reset();
  term_extractor_.reset();
  weak_factory_.InvalidateWeakPtrs();
  Clear();
}

void PhishingClassifier::DOMExtractionFinished(bool success) {
  shingle_hashes_ = std::make_unique<std::set<uint32_t>>();
  if (success) {
    // Term feature extraction can take awhile, so it runs asynchronously
    // in several chunks of work and invokes the callback when finished.
    term_extractor_->ExtractFeatures(
        page_text_, features_.get(), shingle_hashes_.get(),
        base::BindOnce(&PhishingClassifier::TermExtractionFinished,
                       base::Unretained(this)));
  } else {
    RunFailureCallback();
  }
}

void PhishingClassifier::TermExtractionFinished(bool success) {
  if (success) {
#if BUILDFLAG(FULL_SAFE_BROWSING)
    ExtractVisualFeatures();
#else
    if (ScorerStorage::GetInstance()->GetScorer()->HasVisualTfLiteModel()) {
      ExtractVisualFeatures();
    } else {
      VisualExtractionFinished(true);
    }
#endif
  } else {
    RunFailureCallback();
  }
}

void PhishingClassifier::ExtractVisualFeatures() {
  DCHECK(content::RenderThread::IsMainThread());
  base::TimeTicks start_time = base::TimeTicks::Now();
  TRACE_EVENT0("safe_browsing", "ExtractVisualFeatures");

  blink::WebLocalFrame* frame = render_frame_->GetWebFrame();
  gfx::SizeF viewport_size = frame->View()->VisualViewportSize();
  gfx::Rect bounds = ToEnclosingRect(gfx::RectF(viewport_size));

  auto tracker = std::make_unique<paint_preview::PaintPreviewTracker>(
      base::UnguessableToken::Create(), frame->GetEmbeddingToken(),
      /*is_main_frame=*/true);
  cc::PaintRecorder recorder;
  cc::PaintCanvas* canvas = recorder.beginRecording();
  canvas->SetPaintPreviewTracker(tracker.get());

  if (!frame->CapturePaintPreview(bounds, canvas,
                                  /*include_linked_destinations=*/false,
                                  /*skip_accelerated_content=*/true)) {
    VisualExtractionFinished(/*success=*/false);
  }

  cc::PaintRecord paint_record = recorder.finishRecordingAsPicture();

  base::UmaHistogramTimes("SBClientPhishing.VisualFeatureTime",
                          base::TimeTicks::Now() - start_time);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::WithBaseSyncPrimitives()},
      base::BindOnce(&PlaybackOnBackgroundThread, std::move(paint_record),
                     bounds),
      base::BindOnce(&PhishingClassifier::OnPlaybackDone,
                     weak_factory_.GetWeakPtr()));
}

void PhishingClassifier::OnPlaybackDone(std::unique_ptr<SkBitmap> bitmap) {
  if (bitmap) {
    bitmap_ = std::move(bitmap);
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE,
        {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
         base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
        base::BindOnce(&visual_utils::ExtractVisualFeatures, *bitmap_),
        base::BindOnce(&PhishingClassifier::OnVisualFeaturesExtracted,
                       weak_factory_.GetWeakPtr()));
  } else {
    VisualExtractionFinished(/*success=*/false);
  }
}

void PhishingClassifier::OnVisualFeaturesExtracted(
    std::unique_ptr<VisualFeatures> visual_features) {
  visual_features_ = std::move(visual_features);
  VisualExtractionFinished(/*success=*/true);
}

void PhishingClassifier::VisualExtractionFinished(bool success) {
  DCHECK(content::RenderThread::IsMainThread());
  if (!success) {
    RunFailureCallback();
    return;
  }

  blink::WebLocalFrame* main_frame = render_frame_->GetWebFrame();

  // Hash all of the features so that they match the model, then compute
  // the score.
  Scorer* scorer = ScorerStorage::GetInstance()->GetScorer();
  FeatureMap hashed_features;
  std::unique_ptr<ClientPhishingRequest> verdict =
      std::make_unique<ClientPhishingRequest>();
  verdict->set_model_version(scorer->model_version());
  verdict->set_dom_model_version(scorer->dom_model_version());
  verdict->set_url(main_frame->GetDocument().Url().GetString().Utf8());
  for (const auto& it : features_->features()) {
    bool result = hashed_features.AddRealFeature(
        crypto::SHA256HashString(it.first), it.second);
    DCHECK(result);
    ClientPhishingRequest::Feature* feature = verdict->add_feature_map();
    feature->set_name(it.first);
    feature->set_value(it.second);
  }
  for (const auto& it : *shingle_hashes_) {
    verdict->add_shingle_hashes(it);
  }
  float score = static_cast<float>(scorer->ComputeScore(hashed_features));
  verdict->set_client_score(score);
  bool is_dom_match = (score >= scorer->threshold_probability());
  verdict->set_is_phishing(is_dom_match);
  verdict->set_is_dom_match(is_dom_match);
  if (visual_features_) {
    verdict->mutable_visual_features()->Swap(visual_features_.get());
  }

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  ScorerStorage::GetInstance()->GetScorer()->ApplyVisualTfLiteModel(
      *bitmap_, base::BindOnce(&PhishingClassifier::OnVisualTfLiteModelDone,
                               weak_factory_.GetWeakPtr(), std::move(verdict)));
#else
  RunCallback(*verdict);
#endif
}

void PhishingClassifier::OnVisualTfLiteModelDone(
    std::unique_ptr<ClientPhishingRequest> verdict,
    std::vector<double> result) {
  Scorer* scorer = ScorerStorage::GetInstance()->GetScorer();
  if (static_cast<int>(result.size()) > scorer->tflite_thresholds().size()) {
    // Model is misconfigured, so bail out.
    RunFailureCallback();
    return;
  }

  verdict->set_tflite_model_version(scorer->tflite_model_version());

  for (size_t i = 0; i < result.size(); i++) {
    ClientPhishingRequest::CategoryScore* category =
        verdict->add_tflite_model_scores();
    category->set_label(scorer->tflite_thresholds().at(i).label());
    category->set_value(result[i]);
  }

  RunCallback(*verdict);
}

void PhishingClassifier::RunCallback(const ClientPhishingRequest& verdict) {
  TRACE_EVENT_NESTABLE_ASYNC_END0("safe_browsing", "PhishingClassification",
                                  this);
  std::move(done_callback_).Run(verdict);
  Clear();
}

void PhishingClassifier::RunFailureCallback() {
  ClientPhishingRequest verdict;
  // In this case we're not guaranteed to have a valid URL.  Just set it
  // to the empty string to make sure we have a valid protocol buffer.
  verdict.set_url("");
  verdict.set_client_score(kInvalidScore);
  verdict.set_is_phishing(false);
  RunCallback(verdict);
}

void PhishingClassifier::Clear() {
  page_text_ = nullptr;
  done_callback_.Reset();
  features_.reset(nullptr);
  shingle_hashes_.reset(nullptr);
  bitmap_.reset(nullptr);
}

}  // namespace safe_browsing
