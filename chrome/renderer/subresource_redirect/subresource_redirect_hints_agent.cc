// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/subresource_redirect/subresource_redirect_hints_agent.h"
#include "base/metrics/field_trial_params.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/mojo_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace subresource_redirect {

namespace {

// Default timeout for the hints to be received from the time navigation starts.
const int64_t kHintsReceiveDefaultTimeoutSeconds = 5;

// Returns the hinte receive timeout value from field trial.
int64_t GetHintsReceiveTimeout() {
  return base::GetFieldTrialParamByFeatureAsInt(
      blink::features::kSubresourceRedirect, "hints_receive_timeout",
      kHintsReceiveDefaultTimeoutSeconds);
}

}  // namespace

SubresourceRedirectHintsAgent::SubresourceRedirectHintsAgent() = default;
SubresourceRedirectHintsAgent::~SubresourceRedirectHintsAgent() = default;

void SubresourceRedirectHintsAgent::DidStartNavigation() {
  // Clear the hints when a navigation starts, so that hints from previous
  // navigation do not apply in case the same renderframe is reused.
  public_image_urls_.clear();
  public_image_urls_received_ = false;
}

void SubresourceRedirectHintsAgent::ReadyToCommitNavigation(
    int render_frame_id) {
  // Its ok to use base::Unretained(this) here since the timer object is owned
  // by |this|, and the timer and its callback will get deleted when |this| is
  // destroyed.
  hint_receive_timeout_timer_.Start(
      FROM_HERE, base::TimeDelta::FromSeconds(GetHintsReceiveTimeout()),
      base::BindOnce(&SubresourceRedirectHintsAgent::OnHintsReceiveTimeout,
                     base::Unretained(this)));
  render_frame_id_ = render_frame_id;
}

void SubresourceRedirectHintsAgent::SetCompressPublicImagesHints(
    blink::mojom::CompressPublicImagesHintsPtr images_hints) {
  DCHECK(public_image_urls_.empty());
  DCHECK(!public_image_urls_received_);
  public_image_urls_ = images_hints->image_urls;
  public_image_urls_received_ = true;
  hint_receive_timeout_timer_.Stop();
  RecordImageHintsUnavailableMetrics();
}

SubresourceRedirectHintsAgent::RedirectResult
SubresourceRedirectHintsAgent::ShouldRedirectImage(const GURL& url) const {
  if (!public_image_urls_received_) {
    return RedirectResult::kIneligibleImageHintsUnavailable;
  }

  GURL::Replacements rep;
  rep.ClearRef();
  // TODO(rajendrant): Skip redirection if the URL contains username or password
  if (public_image_urls_.find(url.ReplaceComponents(rep).spec()) !=
      public_image_urls_.end()) {
    return RedirectResult::kRedirectable;
  }

  return RedirectResult::kIneligibleMissingInImageHints;
}

void SubresourceRedirectHintsAgent::RecordMetricsOnLoadFinished(
    const GURL& url,
    int64_t content_length,
    RedirectResult redirect_result) {
  if (redirect_result == RedirectResult::kIneligibleImageHintsUnavailable) {
    GURL::Replacements rep;
    rep.ClearRef();
    unavailable_image_hints_urls_.insert(
        std::make_pair(url.ReplaceComponents(rep).spec(), content_length));
    return;
  }
  RecordMetrics(content_length, redirect_result);
}

void SubresourceRedirectHintsAgent::ClearImageHints() {
  public_image_urls_.clear();
}

void SubresourceRedirectHintsAgent::RecordMetrics(
    int64_t content_length,
    RedirectResult redirect_result) const {
  content::RenderFrame* render_frame =
      content::RenderFrame::FromRoutingID(render_frame_id_);
  if (!render_frame || !render_frame->GetWebFrame())
    return;

  ukm::builders::PublicImageCompressionDataUse
      public_image_compression_data_use(
          render_frame->GetWebFrame()->GetDocument().GetUkmSourceId());
  content_length = ukm::GetExponentialBucketMin(content_length, 1.3);

  switch (redirect_result) {
    case RedirectResult::kRedirectable:
      public_image_compression_data_use.SetCompressibleImageBytes(
          content_length);
      break;
    case RedirectResult::kIneligibleImageHintsUnavailable:
      public_image_compression_data_use.SetIneligibleImageHintsUnavailableBytes(
          content_length);
      break;
    case RedirectResult::kIneligibleImageHintsUnavailableButRedirectableBytes:
      public_image_compression_data_use
          .SetIneligibleImageHintsUnavailableButCompressibleBytes(
              content_length);
      break;
    case RedirectResult::kIneligibleImageHintsUnavailableAndMissingInHintsBytes:
      public_image_compression_data_use
          .SetIneligibleImageHintsUnavailableAndMissingInHintsBytes(
              content_length);
      break;
    case RedirectResult::kIneligibleMissingInImageHints:
      public_image_compression_data_use.SetIneligibleMissingInImageHintsBytes(
          content_length);
      break;
    case RedirectResult::kIneligibleOtherImage:
      public_image_compression_data_use.SetIneligibleOtherImageBytes(
          content_length);
      break;
  }
  mojo::PendingRemote<ukm::mojom::UkmRecorderInterface> recorder;
  content::RenderThread::Get()->BindHostReceiver(
      recorder.InitWithNewPipeAndPassReceiver());
  std::unique_ptr<ukm::MojoUkmRecorder> ukm_recorder =
      std::make_unique<ukm::MojoUkmRecorder>(std::move(recorder));
  public_image_compression_data_use.Record(ukm_recorder.get());
}

void SubresourceRedirectHintsAgent::OnHintsReceiveTimeout() {
  RecordImageHintsUnavailableMetrics();
}

void SubresourceRedirectHintsAgent::RecordImageHintsUnavailableMetrics() {
  for (const auto& resource : unavailable_image_hints_urls_) {
    auto redirect_result = RedirectResult::kIneligibleImageHintsUnavailable;
    if (public_image_urls_received_) {
      if (public_image_urls_.find(resource.first) != public_image_urls_.end()) {
        redirect_result = RedirectResult::
            kIneligibleImageHintsUnavailableButRedirectableBytes;
      } else {
        redirect_result = RedirectResult::
            kIneligibleImageHintsUnavailableAndMissingInHintsBytes;
      }
    }
    RecordMetrics(resource.second, redirect_result);
  }
  unavailable_image_hints_urls_.clear();
}

}  // namespace subresource_redirect
