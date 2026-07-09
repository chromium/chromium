// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/connection_metrics.h"

#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "components/private_ai/connection.h"
#include "components/private_ai/proto/private_ai.pb.h"
#include "components/private_ai/status_code.h"

namespace private_ai {

namespace {

std::string FeatureNameToString(proto::FeatureName feature_name) {
  switch (feature_name) {
    case proto::FEATURE_NAME_DEMO_GEMINI_GENERATE_CONTENT:
      return "DemoGenerateContent";
    case proto::FEATURE_NAME_CHROME_ZERO_STATE_SUGGESTION:
      return "ZeroStateSuggestion";
    case proto::FEATURE_NAME_CHROME_FORMS_AI:
      return "FormsAi";
    case proto::FEATURE_NAME_CHROME_CONTEXTUAL_CUEING:
      return "ContextualCueing";
    case proto::FEATURE_NAME_CHROME_AUTOMATED_PASSWORD_CHANGE:
      return "AutomatedPasswordChange";
    case proto::FEATURE_NAME_UNSPECIFIED:
    case proto::FeatureName_INT_MAX_SENTINEL_DO_NOT_USE_:
    case proto::FeatureName_INT_MIN_SENTINEL_DO_NOT_USE_:
      break;
  }
  NOTREACHED() << feature_name;
}

}  // namespace

ConnectionMetrics::ConnectionMetrics(
    std::unique_ptr<Connection> inner_connection,
    proto::FeatureName feature_name)
    : inner_connection_(std::move(inner_connection)),
      feature_name_(feature_name) {
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

void ConnectionMetrics::OnDestroy(StatusCode status_code) {
  inner_connection_->OnDestroy(status_code);

  weak_factory_.InvalidateWeakPtrsAndDoom();
}

void ConnectionMetrics::OnResponse(
    base::TimeTicks start_time,
    OnRequestCallback callback,
    base::expected<proto::PrivateAiResponse, StatusCode> result) {
  const auto latency = base::TimeTicks::Now() - start_time;
  const std::string feature_suffix = FeatureNameToString(feature_name_);

  base::UmaHistogramMediumTimes(
      base::StrCat({"PrivateAi.Client.RequestLatency.", feature_suffix}),
      latency);

  auto status_code = result.has_value() ? StatusCode::kSuccess : result.error();
  base::UmaHistogramEnumeration("PrivateAi.Client.RequestStatusCode",
                                status_code);
  base::UmaHistogramEnumeration(
      base::StrCat({"PrivateAi.Client.RequestStatusCode.", feature_suffix}),
      status_code);

  if (result.has_value()) {
    // Records the response size in bytes. The max value is 1M bytes.
    base::UmaHistogramCounts1M("PrivateAi.Client.ResponseSize.Success",
                               result->ByteSizeLong());
    base::UmaHistogramMediumTimes("PrivateAi.Client.RequestLatency.Success",
                                  latency);
  } else {
    if (result.error() == StatusCode::kTimeout) {
      base::UmaHistogramMediumTimes("PrivateAi.Client.RequestLatency.Timeout",
                                    latency);
    } else {
      base::UmaHistogramMediumTimes("PrivateAi.Client.RequestLatency.Error",
                                    latency);
    }
  }

  std::move(callback).Run(std::move(result));
}

}  // namespace private_ai
