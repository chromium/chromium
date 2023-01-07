// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NACL_LOADER_NACL_TRUSTED_LISTENER_H_
#define COMPONENTS_NACL_LOADER_NACL_TRUSTED_LISTENER_H_

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

  NaClTrustedListener(const NaClTrustedListener&) = delete;
  NaClTrustedListener& operator=(const NaClTrustedListener&) = delete;

  ~NaClTrustedListener();

  nacl::mojom::NaClRendererHost* renderer_host() {
    return renderer_host_.get();
  }

 private:
  mojo::Remote<nacl::mojom::NaClRendererHost> renderer_host_;
};

#endif  // COMPONENTS_NACL_LOADER_NACL_TRUSTED_LISTENER_H_
