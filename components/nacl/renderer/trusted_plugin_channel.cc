// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/renderer/trusted_plugin_channel.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "components/nacl/renderer/histogram.h"
#include "components/nacl/renderer/nexe_load_manager.h"
#include "ppapi/c/pp_errors.h"

namespace nacl {

TrustedPluginChannel::TrustedPluginChannel(
    NexeLoadManager* nexe_load_manager,
    mojo::PendingReceiver<mojom::NaClRendererHost> receiver,
    bool is_helper_nexe)
    : nexe_load_manager_(nexe_load_manager),
      receiver_(this, std::move(receiver)),
      is_helper_nexe_(is_helper_nexe) {
  receiver_.set_disconnect_handler(base::BindOnce(
      &TrustedPluginChannel::OnChannelError, base::Unretained(this)));
}

TrustedPluginChannel::~TrustedPluginChannel() {
}

void TrustedPluginChannel::OnChannelError() {
  if (!is_helper_nexe_)
    nexe_load_manager_->NexeDidCrash();
}

void TrustedPluginChannel::ReportExitStatus(int exit_status,
                                            ReportExitStatusCallback callback) {
  std::move(callback).Run();
  if (!is_helper_nexe_)
    nexe_load_manager_->set_exit_status(exit_status);
}

void TrustedPluginChannel::ReportLoadStatus(NaClErrorCode load_status,
                                            ReportLoadStatusCallback callback) {
  std::move(callback).Run();
  if (load_status < 0 || load_status > NACL_ERROR_CODE_MAX) {
    load_status = LOAD_STATUS_UNKNOWN;
  }
  // For now, we only report UMA for non-helper nexes
  // (don't report for the PNaCl translators nexes).
  if (!is_helper_nexe_) {
    HistogramEnumerate("NaCl.LoadStatus.SelLdr", load_status,
                       NACL_ERROR_CODE_MAX);
    // Gather data to see if being installed changes load outcomes.
    const char* name = nexe_load_manager_->is_installed()
                           ? "NaCl.LoadStatus.SelLdr.InstalledApp"
                           : "NaCl.LoadStatus.SelLdr.NotInstalledApp";
    HistogramEnumerate(name, load_status, NACL_ERROR_CODE_MAX);
  }
  if (load_status != LOAD_OK) {
    nexe_load_manager_->ReportLoadError(PP_NACL_ERROR_SEL_LDR_START_STATUS,
                                        NaClErrorString(load_status));
  }
}

void TrustedPluginChannel::ProvideExitControl(
    mojo::PendingRemote<mojom::NaClExitControl> exit_control) {
  exit_control_.Bind(std::move(exit_control));
}

}  // namespace nacl
