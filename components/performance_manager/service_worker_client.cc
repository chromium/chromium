// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/service_worker_client.h"

ServiceWorkerClient::ServiceWorkerClient(
    content::GlobalRenderFrameHostId render_frame_host_id)
    : type_(blink::mojom::ServiceWorkerClientType::kWindow),
      render_frame_host_id_(render_frame_host_id) {}
ServiceWorkerClient::ServiceWorkerClient(
    blink::DedicatedWorkerToken dedicated_worker_token)
    : type_(blink::mojom::ServiceWorkerClientType::kDedicatedWorker),
      worker_token_(dedicated_worker_token) {}
ServiceWorkerClient::ServiceWorkerClient(
    blink::SharedWorkerToken shared_worker_token)
    : type_(blink::mojom::ServiceWorkerClientType::kSharedWorker),
      worker_token_(shared_worker_token) {}

ServiceWorkerClient::ServiceWorkerClient(const ServiceWorkerClient& other) =
    default;
ServiceWorkerClient& ServiceWorkerClient::operator=(
    const ServiceWorkerClient& other) = default;

ServiceWorkerClient::~ServiceWorkerClient() = default;

content::GlobalRenderFrameHostId ServiceWorkerClient::GetRenderFrameHostId()
    const {
  DCHECK_EQ(type_, blink::mojom::ServiceWorkerClientType::kWindow);
  return render_frame_host_id_;
}

blink::DedicatedWorkerToken ServiceWorkerClient::GetDedicatedWorkerToken()
    const {
  DCHECK_EQ(type_, blink::mojom::ServiceWorkerClientType::kDedicatedWorker);
  return worker_token_.GetAs<blink::DedicatedWorkerToken>();
}

blink::SharedWorkerToken ServiceWorkerClient::GetSharedWorkerToken() const {
  DCHECK_EQ(type_, blink::mojom::ServiceWorkerClientType::kSharedWorker);
  return worker_token_.GetAs<blink::SharedWorkerToken>();
}

bool ServiceWorkerClient::operator<(const ServiceWorkerClient& o) const {
  if (type() == o.type()) {
    switch (type()) {
      case blink::mojom::ServiceWorkerClientType::kWindow:
        return GetRenderFrameHostId() < o.GetRenderFrameHostId();
      case blink::mojom::ServiceWorkerClientType::kDedicatedWorker:
        return GetDedicatedWorkerToken() < o.GetDedicatedWorkerToken();
      case blink::mojom::ServiceWorkerClientType::kSharedWorker:
        return GetSharedWorkerToken() < o.GetSharedWorkerToken();
      case blink::mojom::ServiceWorkerClientType::kAll:
        NOTREACHED();
        return false;
    }
  }
  return type() < o.type();
}
