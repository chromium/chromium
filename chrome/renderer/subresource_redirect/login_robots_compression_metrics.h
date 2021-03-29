// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_SUBRESOURCE_REDIRECT_LOGIN_ROBOTS_COMPRESSION_METRICS_H_
#define CHROME_RENDERER_SUBRESOURCE_REDIRECT_LOGIN_ROBOTS_COMPRESSION_METRICS_H_

#include <cstdint>
#include "base/optional.h"
#include "base/time/time.h"
#include "components/subresource_redirect/common/subresource_redirect_result.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace subresource_redirect {

// Holds the subresource redirect compression data use and timing metrics for
// one resource.
class LoginRobotsCompressionMetrics {
 public:
  LoginRobotsCompressionMetrics(ukm::SourceId ukm_source_id,
                                const base::TimeTicks& navigation_start_time);

  // Notifies the subresource request was started.
  void NotifyRequestStart();

  // Notifies the subresource request fetch was sent to the origin server or
  // compression server.
  void NotifyRequestSent();

  // Records metrics when resource load is complete. |content_length| is the
  // network response bytes, and |ofcl| is populated for compressed response as
  // the original full content length of the response sent by compression
  // server.
  void RecordMetricsOnLoadFinished(SubresourceRedirectResult redirect_result,
                                   size_t content_length,
                                   base::Optional<size_t> ofcl);

 private:
  ukm::SourceId ukm_source_id_;

  // The start time of current navigation.
  base::TimeTicks navigation_start_time_;

  // The start time of the subresource request. This is the time robots rules
  // fetch triggered.
  base::TimeTicks request_start_time_;

  // The time the subresource request was sent to the origin server (when
  // compression allowed) or the compression server (when compression is
  // possible). This is the time robots rules have been retrieved from network
  // or cache and the rules have been applied to the subresource.
  base::TimeTicks request_sent_time_;
};

}  // namespace subresource_redirect

#endif  // CHROME_RENDERER_SUBRESOURCE_REDIRECT_LOGIN_ROBOTS_COMPRESSION_METRICS_H_
