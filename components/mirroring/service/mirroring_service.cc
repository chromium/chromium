// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/mirroring_service.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/mirroring/service/openscreen_session_host.h"
#include "ui/gfx/geometry/size.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/win_util.h"
#endif  // BUILDFLAG(IS_WIN)

namespace mirroring {

MirroringService::MirroringService(
    mojo::PendingReceiver<mojom::MirroringService> receiver,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    : receiver_(this, std::move(receiver)),
      io_task_runner_(std::move(io_task_runner)) {
  DETACH_FROM_SEQUENCE(sequence_checker_);

#if BUILDFLAG(IS_WIN)
  // Disable high resolution timer throttling to prevent degraded audio quality.
  base::win::SetProcessTimerThrottleState(
      base::GetCurrentProcessHandle(), base::win::ProcessPowerState::kDisabled);
#endif  // BUILDFLAG(IS_WIN)

  receiver_.set_disconnect_handler(
      base::BindOnce(&MirroringService::OnDisconnect, base::Unretained(this)));
}

MirroringService::~MirroringService() = default;

void MirroringService::Start(
    mojom::SessionParametersPtr params,
    const gfx::Size& max_resolution,
    mojo::PendingRemote<mojom::SessionObserver> observer,
    mojo::PendingRemote<mojom::ResourceProvider> resource_provider,
    mojo::PendingRemote<mojom::CastMessageChannel> outbound_channel,
    mojo::PendingReceiver<mojom::CastMessageChannel> inbound_channel) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  session_host_.reset();
  session_host_ = std::make_unique<OpenscreenSessionHost>(
      std::move(params), max_resolution, std::move(observer),
      std::move(resource_provider), std::move(outbound_channel),
      std::move(inbound_channel), io_task_runner_, base::DoNothing());
  session_host_->AsyncInitialize();
}

void MirroringService::SwitchMirroringSourceTab() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  session_host_->SwitchSourceTab();
}

void MirroringService::GetMirroringStats(GetMirroringStatsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback).Run(base::Value(session_host_->GetMirroringStats()));
}

void MirroringService::OnDisconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  session_host_.reset();
}

}  // namespace mirroring
