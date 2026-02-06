// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ON_DEVICE_AI_AI_UTILS_H_
#define COMPONENTS_ON_DEVICE_AI_AI_UTILS_H_

#include "base/containers/flat_set.h"
#include "base/metrics/field_trial_params.h"
#include "components/optimization_guide/core/model_execution/on_device_capability.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "third_party/blink/public/mojom/ai/ai_common.mojom.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom.h"

namespace on_device_ai {

using LanguageCodes =
    std::optional<std::vector<blink::mojom::AILanguageCodePtr>>;

template <typename ClientRemoteInterface>
void SendClientRemoteError(
    const mojo::Remote<ClientRemoteInterface>& client_remote,
    blink::mojom::AIManagerCreateClientError error,
    blink::mojom::QuotaErrorInfoPtr quota_error_info = nullptr) {
  if (client_remote) {
    client_remote->OnError(error, std::move(quota_error_info));
  }
}

inline void SendStreamingStatus(
    const mojo::Remote<blink::mojom::ModelStreamingResponder>& responder,
    blink::mojom::ModelStreamingResponseStatus status,
    blink::mojom::QuotaErrorInfoPtr quota_error_info = nullptr) {
  if (responder) {
    responder->OnError(status, std::move(quota_error_info));
  }
}

inline void SendStreamingStatus(
    blink::mojom::ModelStreamingResponder* responder,
    blink::mojom::ModelStreamingResponseStatus status,
    blink::mojom::QuotaErrorInfoPtr quota_error_info = nullptr) {
  if (responder) {
    responder->OnError(status, std::move(quota_error_info));
  }
}

blink::mojom::ModelStreamingResponseStatus ConvertOnDeviceError(
    optimization_guide::OnDeviceError error);

base::flat_set<std::string_view> RestrictSupportedLanguagesForFeature(
    const base::flat_set<std::string_view>& supported,
    const base::FeatureParam<std::string>& feature_param);

}  // namespace on_device_ai

#endif  // COMPONENTS_ON_DEVICE_AI_AI_UTILS_H_
