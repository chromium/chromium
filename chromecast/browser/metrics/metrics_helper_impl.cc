// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/metrics/metrics_helper_impl.h"

#include "base/logging.h"
#include "chromecast/base/metrics/cast_metrics_helper.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromecast {
namespace metrics {

MetricsHelperImpl::MetricsHelperImpl() = default;
MetricsHelperImpl::~MetricsHelperImpl() = default;

void MetricsHelperImpl::AddReceiver(
    mojo::PendingReceiver<mojom::MetricsHelper> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void MetricsHelperImpl::RecordApplicationEvent(const std::string& app_id,
                                               const std::string& session_id,
                                               const std::string& sdk_version,
                                               const std::string& event) {
  CastMetricsHelper::GetInstance()->RecordApplicationEvent(app_id, session_id,
                                                           sdk_version, event);
}

}  // namespace metrics
}  // namespace chromecast
