// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NACL_RENDERER_TRUSTED_PLUGIN_CHANNEL_H_
#define COMPONENTS_NACL_RENDERER_TRUSTED_PLUGIN_CHANNEL_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "components/nacl/common/nacl.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "native_client/src/trusted/service_runtime/nacl_error_code.h"
#include "ppapi/c/pp_instance.h"

namespace nacl {
class NexeLoadManager;

class TrustedPluginChannel : public mojom::NaClRendererHost {
 public:
  TrustedPluginChannel(NexeLoadManager* nexe_load_manager,
                       mojo::PendingReceiver<mojom::NaClRendererHost> receiver,
                       bool is_helper_nexe);

  TrustedPluginChannel(const TrustedPluginChannel&) = delete;
  TrustedPluginChannel& operator=(const TrustedPluginChannel&) = delete;

  ~TrustedPluginChannel() override;

 private:
  void OnChannelError();

  // mojom::NaClRendererHost overrides.
  void ReportExitStatus(int exit_status,
                        ReportExitStatusCallback callback) override;
  void ReportLoadStatus(NaClErrorCode load_status,
                        ReportLoadStatusCallback callback) override;
  void ProvideExitControl(
      mojo::PendingRemote<mojom::NaClExitControl> exit_control) override;

  // Non-owning pointer. This is safe because the TrustedPluginChannel is owned
  // by the NexeLoadManager pointed to here.
  raw_ptr<NexeLoadManager> nexe_load_manager_;
  mojo::Receiver<mojom::NaClRendererHost> receiver_;
  mojo::Remote<mojom::NaClExitControl> exit_control_;
  const bool is_helper_nexe_;
};

}  // namespace nacl

#endif  // COMPONENTS_NACL_RENDERER_TRUSTED_PLUGIN_CHANNEL_H_
