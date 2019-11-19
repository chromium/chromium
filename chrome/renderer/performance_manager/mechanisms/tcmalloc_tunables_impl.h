// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PERFORMANCE_MANAGER_MECHANISMS_TCMALLOC_TUNABLES_IMPL_H_
#define CHROME_RENDERER_PERFORMANCE_MANAGER_MECHANISMS_TCMALLOC_TUNABLES_IMPL_H_

#include "base/macros.h"
#include "chrome/common/performance_manager/mojom/tcmalloc.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace performance_manager {
namespace mechanism {

class TcmallocTunablesImpl : public tcmalloc::mojom::TcmallocTunables {
 public:
  ~TcmallocTunablesImpl() override;
  TcmallocTunablesImpl();

  static void Create(
      mojo::PendingReceiver<tcmalloc::mojom::TcmallocTunables> receiver);

 protected:
  // TcmallocTunables impl:
  void SetMaxTotalThreadCacheBytes(uint32_t size_bytes) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(TcmallocTunablesImpl);
};

}  // namespace mechanism
}  // namespace performance_manager

#endif  // CHROME_RENDERER_PERFORMANCE_MANAGER_MECHANISMS_TCMALLOC_TUNABLES_IMPL_H_
