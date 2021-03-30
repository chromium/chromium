// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/subresource_redirect/login_robots_compression_metrics.h"

#include "content/public/renderer/render_thread.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/mojo_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"

namespace subresource_redirect {

LoginRobotsCompressionMetrics::LoginRobotsCompressionMetrics(
    ukm::SourceId ukm_source_id,
    const base::TimeTicks& navigation_start_time)
    : ukm_source_id_(ukm_source_id),
      navigation_start_time_(navigation_start_time) {}

void LoginRobotsCompressionMetrics::NotifyRequestStart() {
  request_start_time_ = base::TimeTicks::Now();
}

void LoginRobotsCompressionMetrics::NotifyRequestSent() {
  request_sent_time_ = base::TimeTicks::Now();
}

void LoginRobotsCompressionMetrics::RecordMetricsOnLoadFinished(
    SubresourceRedirectResult redirect_result,
    size_t content_length,
    base::Optional<size_t> ofcl) {
  base::TimeTicks response_received_time = base::TimeTicks::Now();

  ukm::builders::PublicImageCompressionImageLoad
      public_image_compression_image_load(ukm_source_id_);
  public_image_compression_image_load.SetRedirectResult(
      static_cast<int64_t>(redirect_result));
  if (!navigation_start_time_.is_null()) {
    if (!request_start_time_.is_null()) {
      public_image_compression_image_load.SetNavigationToRequestStart(
          base::TimeDelta(request_start_time_ - navigation_start_time_)
              .InMilliseconds());
    }
    if (!request_sent_time_.is_null()) {
      public_image_compression_image_load.SetNavigationToRequestSent(
          base::TimeDelta(request_sent_time_ - navigation_start_time_)
              .InMilliseconds());
    }
    public_image_compression_image_load.SetNavigationToResponseReceived(
        base::TimeDelta(response_received_time - navigation_start_time_)
            .InMilliseconds());
  }
  if (!request_start_time_.is_null() && !request_sent_time_.is_null()) {
    public_image_compression_image_load.SetRobotsRulesFetchLatency(
        base::TimeDelta(request_sent_time_ - request_start_time_)
            .InMilliseconds());
  }

  if (ofcl) {
    public_image_compression_image_load.SetOriginalBytes(
        ukm::GetExponentialBucketMin(*ofcl, 1.3));
    public_image_compression_image_load.SetCompressionPercentage(
        static_cast<int64_t>(
            100 - ((content_length / static_cast<float>(*ofcl)) * 100)));
  } else {
    public_image_compression_image_load.SetOriginalBytes(
        ukm::GetExponentialBucketMin(content_length, 1.3));
  }
  mojo::PendingRemote<ukm::mojom::UkmRecorderInterface> recorder;
  content::RenderThread::Get()->BindHostReceiver(
      recorder.InitWithNewPipeAndPassReceiver());
  std::unique_ptr<ukm::MojoUkmRecorder> ukm_recorder =
      std::make_unique<ukm::MojoUkmRecorder>(std::move(recorder));
  public_image_compression_image_load.Record(ukm_recorder.get());
}

}  // namespace subresource_redirect
