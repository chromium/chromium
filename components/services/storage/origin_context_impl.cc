// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/origin_context_impl.h"

#include "components/services/storage/partition_impl.h"

namespace storage {

OriginContextImpl::OriginContextImpl(PartitionImpl* partition,
                                     const url::Origin& origin)
    : partition_(partition), origin_(origin) {
  receivers_.set_disconnect_handler(base::BindRepeating(
      &OriginContextImpl::OnDisconnect, base::Unretained(this)));
}

OriginContextImpl::~OriginContextImpl() = default;

void OriginContextImpl::BindReceiver(
    mojo::PendingReceiver<mojom::OriginContext> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void OriginContextImpl::OnDisconnect() {
  if (receivers_.empty()) {
    // Deletes |this|.
    partition_->RemoveOriginContext(origin_);
  }
}

}  // namespace storage
