// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRACING_CHILD_BACKGROUND_TRACING_AGENT_PROVIDER_IMPL_H_
#define COMPONENTS_TRACING_CHILD_BACKGROUND_TRACING_AGENT_PROVIDER_IMPL_H_

#include "base/macros.h"
#include "components/tracing/common/background_tracing_agent.mojom.h"
#include "components/tracing/tracing_export.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"

namespace tracing {

class TRACING_EXPORT BackgroundTracingAgentProviderImpl
    : public mojom::BackgroundTracingAgentProvider {
 public:
  BackgroundTracingAgentProviderImpl();
  ~BackgroundTracingAgentProviderImpl() override;

  void AddBinding(
      mojo::PendingReceiver<mojom::BackgroundTracingAgentProvider> provider);

  // mojom::BackgroundTracingAgentProvider methods:
  void Create(
      uint64_t tracing_process_id,
      mojo::PendingRemote<mojom::BackgroundTracingAgentClient> client,
      mojo::PendingReceiver<mojom::BackgroundTracingAgent> agent) override;

 private:
  mojo::ReceiverSet<mojom::BackgroundTracingAgentProvider> self_receiver_set_;
  mojo::UniqueReceiverSet<mojom::BackgroundTracingAgent> agent_receiver_set_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundTracingAgentProviderImpl);
};

}  // namespace tracing

#endif  // COMPONENTS_TRACING_CHILD_BACKGROUND_TRACING_AGENT_PROVIDER_IMPL_H_
