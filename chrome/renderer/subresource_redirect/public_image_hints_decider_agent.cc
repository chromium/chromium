// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/subresource_redirect/public_image_hints_decider_agent.h"

#include "base/metrics/field_trial_params.h"
#include "chrome/renderer/subresource_redirect/subresource_redirect_params.h"
#include "components/subresource_redirect/common/subresource_redirect_features.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/mojo_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace subresource_redirect {

namespace {

// Returns the url spec with username, password, ref fragment stripped to be
// useful for public URL decision making.
std::string GetURLForPublicDecision(const GURL& url) {
  GURL::Replacements rep;
  rep.ClearRef();
  rep.ClearPassword();
  rep.ClearUsername();
  return url.ReplaceComponents(rep).spec();
}

}  // namespace

PublicImageHintsDeciderAgent::PublicImageHintsDeciderAgent(
    blink::AssociatedInterfaceRegistry* associated_interfaces,
    content::RenderFrame* render_frame)
    : PublicResourceDeciderAgent(associated_interfaces, render_frame) {
  DCHECK(ShouldEnablePublicImageHintsBasedCompression());
}

PublicImageHintsDeciderAgent::~PublicImageHintsDeciderAgent() = default;

bool PublicImageHintsDeciderAgent::IsMainFrame() const {
  return render_frame()->IsMainFrame();
}

void PublicImageHintsDeciderAgent::DidStartNavigation(
    const GURL& url,
    base::Optional<blink::WebNavigationType> navigation_type) {
  if (!IsMainFrame())
    return;
  // Clear the hints when a navigation starts, so that hints from previous
  // navigation do not apply in case the same renderframe is reused.
  public_image_urls_ = base::nullopt;
}

void PublicImageHintsDeciderAgent::ReadyToCommitNavigation(
    blink::WebDocumentLoader* document_loader) {
  PublicResourceDeciderAgent::ReadyToCommitNavigation(document_loader);
  if (!IsMainFrame())
    return;
  // Its ok to use base::Unretained(this) here since the timer object is owned
  // by |this|, and the timer and its callback will get deleted when |this| is
  // destroyed.
  hint_receive_timeout_timer_.Start(
      FROM_HERE, base::TimeDelta::FromSeconds(GetHintsReceiveTimeout()),
      base::BindOnce(&PublicImageHintsDeciderAgent::OnHintsReceiveTimeout,
                     base::Unretained(this)));
}

void PublicImageHintsDeciderAgent::OnDestruct() {
  delete this;
}

void PublicImageHintsDeciderAgent::SetCompressPublicImagesHints(
    mojom::CompressPublicImagesHintsPtr images_hints) {
  if (!IsMainFrame())
    return;
  DCHECK(!public_image_urls_);
  public_image_urls_ = images_hints->image_urls;
  hint_receive_timeout_timer_.Stop();
  RecordImageHintsUnavailableMetrics();
}

base::Optional<SubresourceRedirectResult>
PublicImageHintsDeciderAgent::ShouldRedirectSubresource(
    const GURL& url,
    ShouldRedirectDecisionCallback callback) {
  if (!IsMainFrame())
    return SubresourceRedirectResult::kIneligibleSubframeResource;
  if (!public_image_urls_)
    return SubresourceRedirectResult::kIneligibleImageHintsUnavailable;

  if (public_image_urls_->find(GetURLForPublicDecision(url)) !=
      public_image_urls_->end()) {
    return SubresourceRedirectResult::kRedirectable;
  }

  return SubresourceRedirectResult::kIneligibleMissingInImageHints;
}

void PublicImageHintsDeciderAgent::RecordMetricsOnLoadFinished(
    const GURL& url,
    int64_t content_length,
    SubresourceRedirectResult redirect_result) {
  if (redirect_result ==
      SubresourceRedirectResult::kIneligibleImageHintsUnavailable) {
    unavailable_image_hints_urls_.insert(
        std::make_pair(GetURLForPublicDecision(url), content_length));
    return;
  }
  RecordMetrics(content_length, redirect_result);
}

void PublicImageHintsDeciderAgent::ClearImageHints() {
  if (public_image_urls_)
    public_image_urls_->clear();
}

void PublicImageHintsDeciderAgent::RecordMetrics(
    int64_t content_length,
    SubresourceRedirectResult redirect_result) const {
  // TODO(1156757): Reduce the number of ukm records, by aggregating the
  // image bytes per SubresourceRedirectResult and then recording once every k
  // seconds, or k images.
  if (!render_frame() || !render_frame()->GetWebFrame())
    return;

  ukm::builders::PublicImageCompressionDataUse
      public_image_compression_data_use(
          render_frame()->GetWebFrame()->GetDocument().GetUkmSourceId());
  content_length = ukm::GetExponentialBucketMin(content_length, 1.3);

  switch (redirect_result) {
    case SubresourceRedirectResult::kRedirectable:
      public_image_compression_data_use.SetCompressibleImageBytes(
          content_length);
      break;
    case SubresourceRedirectResult::kIneligibleImageHintsUnavailable:
      public_image_compression_data_use.SetIneligibleImageHintsUnavailableBytes(
          content_length);
      break;
    case SubresourceRedirectResult::
        kIneligibleImageHintsUnavailableButRedirectable:
      public_image_compression_data_use
          .SetIneligibleImageHintsUnavailableButCompressibleBytes(
              content_length);
      break;
    case SubresourceRedirectResult::
        kIneligibleImageHintsUnavailableAndMissingInHints:
      public_image_compression_data_use
          .SetIneligibleImageHintsUnavailableAndMissingInHintsBytes(
              content_length);
      break;
    case SubresourceRedirectResult::kIneligibleMissingInImageHints:
      public_image_compression_data_use.SetIneligibleMissingInImageHintsBytes(
          content_length);
      break;
    case SubresourceRedirectResult::kUnknown:
    case SubresourceRedirectResult::kIneligibleRedirectFailed:
    case SubresourceRedirectResult::kIneligibleBlinkDisallowed:
    case SubresourceRedirectResult::kIneligibleSubframeResource:
      public_image_compression_data_use.SetIneligibleOtherImageBytes(
          content_length);
      break;
    case SubresourceRedirectResult::kIneligibleRobotsDisallowed:
    case SubresourceRedirectResult::kIneligibleRobotsTimeout:
    case SubresourceRedirectResult::kIneligibleLoginDetected:
      NOTREACHED();
  }
  mojo::PendingRemote<ukm::mojom::UkmRecorderInterface> recorder;
  content::RenderThread::Get()->BindHostReceiver(
      recorder.InitWithNewPipeAndPassReceiver());
  std::unique_ptr<ukm::MojoUkmRecorder> ukm_recorder =
      std::make_unique<ukm::MojoUkmRecorder>(std::move(recorder));
  public_image_compression_data_use.Record(ukm_recorder.get());
}

void PublicImageHintsDeciderAgent::OnHintsReceiveTimeout() {
  RecordImageHintsUnavailableMetrics();
}

void PublicImageHintsDeciderAgent::RecordImageHintsUnavailableMetrics() {
  for (const auto& resource : unavailable_image_hints_urls_) {
    auto redirect_result =
        SubresourceRedirectResult::kIneligibleImageHintsUnavailable;
    if (public_image_urls_) {
      if (public_image_urls_->find(resource.first) !=
          public_image_urls_->end()) {
        redirect_result = SubresourceRedirectResult::
            kIneligibleImageHintsUnavailableButRedirectable;
      } else {
        redirect_result = SubresourceRedirectResult::
            kIneligibleImageHintsUnavailableAndMissingInHints;
      }
    }
    RecordMetrics(resource.second, redirect_result);
  }
  unavailable_image_hints_urls_.clear();
}

void PublicImageHintsDeciderAgent::NotifyCompressedResourceFetchFailed(
    base::TimeDelta retry_after) {
  PublicResourceDeciderAgent::NotifyCompressedResourceFetchFailed(retry_after);
  ClearImageHints();
}

void PublicImageHintsDeciderAgent::SetLoggedInState(bool is_logged_in) {
  // This mojo from browser process should not be called for public image hints
  // based compression.
  NOTIMPLEMENTED();
}

}  // namespace subresource_redirect
