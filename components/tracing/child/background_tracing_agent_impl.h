// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRACING_CHILD_BACKGROUND_TRACING_AGENT_IMPL_H_
#define COMPONENTS_TRACING_CHILD_BACKGROUND_TRACING_AGENT_IMPL_H_

#include <stdint.h>
#include <string>

#include "base/macros.h"
#include "base/metrics/histogram.h"
#include "base/time/time.h"
#include "components/tracing/common/background_tracing_agent.mojom.h"
#include "components/tracing/tracing_export.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace base {
class SequencedTaskRunner;
}

namespace tracing {

// This class sends and receives trace messages on child processes.
class TRACING_EXPORT BackgroundTracingAgentImpl
    : public mojom::BackgroundTracingAgent {
 public:
  explicit BackgroundTracingAgentImpl(
      mojo::PendingRemote<mojom::BackgroundTracingAgentClient> client);
  ~BackgroundTracingAgentImpl() override;

  // mojom::BackgroundTracingAgent methods:
  void SetUMACallback(const std::string& histogram_name,
                      int32_t histogram_lower_value,
                      int32_t histogram_upper_value,
                      bool repeat) override;
  void ClearUMACallback(const std::string& histogram_name) override;

 private:
  static void OnHistogramChanged(
      base::WeakPtr<BackgroundTracingAgentImpl> weak_self,
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      const std::string& histogram_name,
      base::Histogram::Sample reference_lower_value,
      base::Histogram::Sample reference_upper_value,
      bool repeat,
      base::Histogram::Sample actual_value);
  void SendTriggerMessage(const std::string& histogram_name);
  void SendAbortBackgroundTracingMessage();

  mojo::Remote<mojom::BackgroundTracingAgentClient> client_;
  base::Time histogram_last_changed_;

  base::WeakPtrFactory<BackgroundTracingAgentImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(BackgroundTracingAgentImpl);
};

}  // namespace tracing

#endif  // COMPONENTS_TRACING_CHILD_BACKGROUND_TRACING_AGENT_IMPL_H_
