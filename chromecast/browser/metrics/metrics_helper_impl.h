// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_METRICS_METRICS_HELPER_IMPL_H_
#define CHROMECAST_BROWSER_METRICS_METRICS_HELPER_IMPL_H_

#include <vector>

#include "chromecast/common/mojom/metrics_helper.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace chromecast {
namespace metrics {

class MetricsHelperImpl : public mojom::MetricsHelper {
 public:
  MetricsHelperImpl();
  MetricsHelperImpl(const MetricsHelperImpl&) = delete;
  MetricsHelperImpl& operator=(const MetricsHelperImpl&) = delete;
  ~MetricsHelperImpl() override;

  void AddReceiver(mojo::PendingReceiver<mojom::MetricsHelper> receiver);

 private:
  // chromecast::mojom::MetricsHelper implementation.
  void RecordApplicationEvent(const std::string& app_id,
                              const std::string& session_id,
                              const std::string& sdk_version,
                              const std::string& event) override;

  mojo::ReceiverSet<mojom::MetricsHelper> receivers_;
};

}  // namespace metrics
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_METRICS_METRICS_HELPER_IMPL_H_
