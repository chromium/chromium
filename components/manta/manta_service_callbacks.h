// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MANTA_MANTA_SERVICE_CALLBACKS_H_
#define COMPONENTS_MANTA_MANTA_SERVICE_CALLBACKS_H_

#include <memory>

#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/manta/manta_status.h"
#include "components/manta/proto/manta.pb.h"

namespace endpoint_fetcher {
class EndpointFetcher;
struct EndpointResponse;
}  // namespace endpoint_fetcher

namespace manta {

// Enum that indicates the source of the call to manta server, used for logging
// UMA metrics.
enum class MantaMetricType {
  kAnchovy,
  kOrca,
  kScanner,
  kSnapper,
  kMahiSummary,
  kMahiElucidation,
  kMahiQA,
  kWalrus,
};

// Manta service uses this callback to return a Response proto parsed
// from server response, and a MantaStatus struct that indicates OK status or
// errors if server does not respond properly.
using MantaProtoResponseCallback =
    base::OnceCallback<void(std::unique_ptr<manta::proto::Response>,
                            MantaStatus)>;

// Manta service uses this callback to return the parsed result / error messages
// to the caller.
using MantaGenericCallback =
    base::OnceCallback<void(base::Value::Dict, MantaStatus)>;

COMPONENT_EXPORT(MANTA)
void OnEndpointFetcherComplete(
    MantaProtoResponseCallback callback,
    base::Time start_time,
    const MantaMetricType request_type,
    std::unique_ptr<endpoint_fetcher::EndpointFetcher> fetcher,
    std::unique_ptr<endpoint_fetcher::EndpointResponse> responses);

}  // namespace manta

#endif  // COMPONENTS_MANTA_MANTA_SERVICE_CALLBACKS_H_
