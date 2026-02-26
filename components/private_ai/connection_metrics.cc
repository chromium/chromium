// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/connection_metrics.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "components/private_ai/connection.h"
#include "components/private_ai/proto/private_ai.pb.h"

namespace private_ai {

ConnectionMetrics::ConnectionMetrics(
    std::unique_ptr<Connection> inner_connection)
    : inner_connection_(std::move(inner_connection)) {
  CHECK(inner_connection_);
}

ConnectionMetrics::~ConnectionMetrics() = default;

void ConnectionMetrics::Send(proto::PrivateAiRequest request,
                             base::TimeDelta timeout,
                             OnRequestCallback callback) {
  base::UmaHistogramCounts1M("PrivateAi.Client.RequestSize",
                             static_cast<int>(request.ByteSizeLong()));
  base::UmaHistogramSparse("PrivateAi.Client.FeatureName",
                           static_cast<int>(request.feature_name()));

  inner_connection_->Send(
      std::move(request), timeout,
      base::BindOnce(&ConnectionMetrics::OnResponse, weak_factory_.GetWeakPtr(),
                     base::TimeTicks::Now(), std::move(callback)));
}

void ConnectionMetrics::OnDestroy(ErrorCode error) {
  inner_connection_->OnDestroy(error);

  weak_factory_.InvalidateWeakPtrsAndDoom();
}

void ConnectionMetrics::OnResponse(
    base::TimeTicks start_time,
    OnRequestCallback callback,
    base::expected<proto::PrivateAiResponse, ErrorCode> result) {
  const auto latency = base::TimeTicks::Now() - start_time;

  if (result.has_value()) {
    // Records the response size in bytes. The max value is 1M bytes.
    base::UmaHistogramCounts1M("PrivateAi.Client.ResponseSize.Success",
                               result->ByteSizeLong());
    base::UmaHistogramMediumTimes("PrivateAi.Client.RequestLatency.Success",
                                  latency);
  } else if (result.error() == ErrorCode::kTimeout) {
    base::UmaHistogramEnumeration("PrivateAi.Client.RequestErrorCode",
                                  ErrorCode::kTimeout);
    base::UmaHistogramMediumTimes("PrivateAi.Client.RequestLatency.Timeout",
                                  latency);
  } else {
    base::UmaHistogramEnumeration("PrivateAi.Client.RequestErrorCode",
                                  result.error());
    base::UmaHistogramMediumTimes("PrivateAi.Client.RequestLatency.Error",
                                  latency);
  }

  std::move(callback).Run(std::move(result));
}

}  // namespace private_ai
