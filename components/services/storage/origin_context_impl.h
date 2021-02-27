// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_ORIGIN_CONTEXT_IMPL_H_
#define COMPONENTS_SERVICES_STORAGE_ORIGIN_CONTEXT_IMPL_H_

#include "base/macros.h"
#include "components/services/storage/public/mojom/origin_context.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "url/origin.h"

namespace storage {

class PartitionImpl;

class OriginContextImpl : public mojom::OriginContext {
 public:
  OriginContextImpl(PartitionImpl* partition, const url::Origin& origin);
  ~OriginContextImpl() override;

  const mojo::ReceiverSet<mojom::OriginContext>& receivers() const {
    return receivers_;
  }

  void BindReceiver(mojo::PendingReceiver<mojom::OriginContext> receiver);

 private:
  void OnDisconnect();

  PartitionImpl* const partition_;
  const url::Origin origin_;
  mojo::ReceiverSet<mojom::OriginContext> receivers_;

  DISALLOW_COPY_AND_ASSIGN(OriginContextImpl);
};

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_ORIGIN_CONTEXT_IMPL_H_
