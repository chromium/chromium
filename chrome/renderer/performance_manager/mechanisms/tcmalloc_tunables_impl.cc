// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/performance_manager/mechanisms/tcmalloc_tunables_impl.h"

#include "base/allocator/allocator_extension.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace performance_manager {
namespace mechanism {

namespace {
constexpr char kMaxTotalThreadCacheBytesKey[] =
    "tcmalloc.max_total_thread_cache_bytes";
}  // namespace

TcmallocTunablesImpl::TcmallocTunablesImpl() = default;
TcmallocTunablesImpl::~TcmallocTunablesImpl() = default;

// Static
void TcmallocTunablesImpl::Create(
    mojo::PendingReceiver<tcmalloc::mojom::TcmallocTunables> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<TcmallocTunablesImpl>(),
                              std::move(receiver));
}

void TcmallocTunablesImpl::SetMaxTotalThreadCacheBytes(uint32_t size_bytes) {
  bool res = base::allocator::SetNumericProperty(kMaxTotalThreadCacheBytesKey,
                                                 size_bytes);
  LOG_IF(ERROR, !res) << "Unable to SetNumericProperty("
                      << kMaxTotalThreadCacheBytesKey << ") to " << size_bytes;
}

}  // namespace mechanism
}  // namespace performance_manager
