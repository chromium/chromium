// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_SERVICE_WORKER_CLIENT_H_
#define COMPONENTS_PERFORMANCE_MANAGER_SERVICE_WORKER_CLIENT_H_

#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/service_worker_client_info.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_client.mojom.h"

// Represents a client of a service worker node.
//
// This class is essentially a tagged union where only the field corresponding
// to the |type()| can be accessed.
class ServiceWorkerClient {
 public:
  explicit ServiceWorkerClient(
      content::GlobalRenderFrameHostId render_frame_host_id);
  explicit ServiceWorkerClient(
      blink::DedicatedWorkerToken dedicated_worker_token);
  explicit ServiceWorkerClient(blink::SharedWorkerToken shared_worker_token);

  ServiceWorkerClient(const ServiceWorkerClient& other);
  ServiceWorkerClient& operator=(const ServiceWorkerClient& other);

  ~ServiceWorkerClient();

  blink::mojom::ServiceWorkerClientType type() const { return type_; }

  content::GlobalRenderFrameHostId GetRenderFrameHostId() const;
  blink::DedicatedWorkerToken GetDedicatedWorkerToken() const;
  blink::SharedWorkerToken GetSharedWorkerToken() const;

  bool operator<(const ServiceWorkerClient& other) const;

 private:
  // The client type.
  blink::mojom::ServiceWorkerClientType type_;

  union {
    // The frame tree node ID, if this is a window client.
    content::GlobalRenderFrameHostId render_frame_host_id_;

    // The token of the client, if this is a worker client.
    content::DedicatedOrSharedWorkerToken worker_token_;
  };
};

#endif  // COMPONENTS_PERFORMANCE_MANAGER_SERVICE_WORKER_CLIENT_H_
