// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NACL_LOADER_NACL_TRUSTED_LISTENER_H_
#define COMPONENTS_NACL_LOADER_NACL_TRUSTED_LISTENER_H_

#include "base/macros.h"
#include "components/nacl/common/nacl.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace base {
class SingleThreadTaskRunner;
}

class NaClTrustedListener {
 public:
  NaClTrustedListener(
      mojo::PendingRemote<nacl::mojom::NaClRendererHost> renderer_host,
      base::SingleThreadTaskRunner* io_task_runner);
  ~NaClTrustedListener();

  nacl::mojom::NaClRendererHost* renderer_host() {
    return renderer_host_.get();
  }

 private:
  mojo::Remote<nacl::mojom::NaClRendererHost> renderer_host_;

  DISALLOW_COPY_AND_ASSIGN(NaClTrustedListener);
};

#endif  // COMPONENTS_NACL_LOADER_NACL_TRUSTED_LISTENER_H_
