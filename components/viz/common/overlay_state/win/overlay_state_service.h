// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_OVERLAY_STATE_WIN_OVERLAY_STATE_SERVICE_H_
#define COMPONENTS_VIZ_COMMON_OVERLAY_STATE_WIN_OVERLAY_STATE_SERVICE_H_

#include <memory>

#include "base/no_destructor.h"
#include "base/task/sequenced_task_runner.h"
#include "components/viz/common/overlay_state/win/overlay_state_aggregator.h"
#include "components/viz/common/viz_common_export.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/ipc/common/gpu_channel.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace viz {

using OverlayStateObserver = gpu::mojom::OverlayStateObserver;

// The OverlayStateService allows a MediaFoundationRendererClient, running in
// a Renderer process, to understand whether the quads associated with its
// video presentation textures are being promoted to a Direct Composition layer
// by Viz. This allows the MediaFoundationRendererClient to determine the
// appropriate presentation mode to use as the Windowless Swapchain mode
// requires Direct Composition support to work.
//
// The mailbox associated with a texture is used as the common identifier
// between the overlay processor in Viz & the MediaFoundationRendererClient.
// Further the quad associated with the mailbox has it's associated
// TransferResource tagged with 'wants_promotion_hint' to ensure that the
// overlay processor only sends the OverlayStateService hints for mailboxes of
// interest.
//
// The OverlayStateService aggregates hints to help ensure minimal IPC overhead
// in keeping the MediaFoundationRendererClient informed of the current
// promotion state.
class VIZ_COMMON_EXPORT OverlayStateService {
 public:
  static OverlayStateService* GetInstance();
  void Initialize(scoped_refptr<base::SequencedTaskRunner> task_runner);
  bool IsInitialized();

  void RegisterObserver(mojo::PendingRemote<gpu::mojom::OverlayStateObserver>
                            promotion_hint_observer,
                        const gpu::Mailbox& mailbox);
  void SetPromotionHint(const gpu::Mailbox& mailbox, bool promoted);
  void MailboxDestroyed(const gpu::Mailbox& mailbox);

 private:
  friend class base::NoDestructor<OverlayStateService>;
  OverlayStateService();
  ~OverlayStateService();
  OverlayStateService(const OverlayStateService&) = delete;
  OverlayStateService& operator=(const OverlayStateService&) = delete;

  void OnBoundObserverDisconnect(const gpu::Mailbox& mailbox,
                                 mojo::RemoteSetElementId);
  void OnStateChanged(const gpu::Mailbox& mailbox,
                      OverlayStateAggregator::PromotionState promoted);
  void OnStateChangedOnTaskRunnerSequence(
      const gpu::Mailbox& mailbox,
      OverlayStateAggregator::PromotionState promoted);
  void SetPromotionHintOnTaskRunnerSequence(const gpu::Mailbox& mailbox,
                                            bool promoted);
  void MailboxDestroyedOnTaskRunnerSequence(const gpu::Mailbox& mailbox);

  struct MailboxState {
    MailboxState();
    ~MailboxState();
    OverlayStateAggregator aggregator_;
    mojo::RemoteSet<OverlayStateObserver> observer_set_;
  };

  bool initialized_ = false;
  base::flat_map<gpu::Mailbox, std::unique_ptr<MailboxState>> mailboxes_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_OVERLAY_STATE_WIN_OVERLAY_STATE_SERVICE_H_
