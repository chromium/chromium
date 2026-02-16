// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/legion/connection_metrics.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "components/legion/connection.h"
#include "components/legion/proto/legion.pb.h"

namespace private_ai {

ConnectionMetrics::ConnectionMetrics(
    std::unique_ptr<Connection> inner_connection)
    : inner_connection_(std::move(inner_connection)) {
  CHECK(inner_connection_);
}

ConnectionMetrics::~ConnectionMetrics() = default;

void ConnectionMetrics::Send(proto::LegionRequest request,
                             base::TimeDelta timeout,
                             OnRequestCallback callback) {
  base::UmaHistogramCounts1M("Legion.Client.RequestSize",
                             static_cast<int>(request.ByteSizeLong()));
  base::UmaHistogramSparse("Legion.Client.FeatureName",
                           static_cast<int>(request.feature_name()));

  inner_connection_->Send(
      std::move(request), timeout,
      base::BindOnce(&ConnectionMetrics::OnResponse, weak_factory_.GetWeakPtr(),
                     base::TimeTicks::Now(), std::move(callback)));
}

void ConnectionMetrics::OnResponse(
    base::TimeTicks start_time,
    OnRequestCallback callback,
    base::expected<proto::LegionResponse, ErrorCode> result) {
  const auto latency = base::TimeTicks::Now() - start_time;

  if (result.has_value()) {
    // Records the response size in bytes. The max value is 1M bytes.
    base::UmaHistogramCounts1M("Legion.Client.ResponseSize.Success",
                               result->ByteSizeLong());
    base::UmaHistogramMediumTimes("Legion.Client.RequestLatency.Success",
                                  latency);
  } else if (result.error() == ErrorCode::kTimeout) {
    base::UmaHistogramEnumeration("Legion.Client.RequestErrorCode",
                                  ErrorCode::kTimeout);
    base::UmaHistogramMediumTimes("Legion.Client.RequestLatency.Timeout",
                                  latency);
  } else {
    base::UmaHistogramEnumeration("Legion.Client.RequestErrorCode",
                                  result.error());
    base::UmaHistogramMediumTimes("Legion.Client.RequestLatency.Error",
                                  latency);
  }

  std::move(callback).Run(std::move(result));
}

}  // namespace private_ai
