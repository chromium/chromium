// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_INPUT_ANDROID_STATE_TRANSFER_HANDLER_H_
#define COMPONENTS_VIZ_SERVICE_INPUT_ANDROID_STATE_TRANSFER_HANDLER_H_

#include <optional>

#include "base/containers/queue.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "components/input/android/android_input_callback.h"
#include "components/input/render_input_router.mojom.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/service/input/render_input_router_support_android.h"
#include "components/viz/service/viz_service_export.h"

namespace viz {

class AndroidStateTransferHandlerClient {
 public:
  virtual bool TransferInputBackToBrowser() = 0;
};

// AndroidStateTransferHandler listens to input events coming from Android
// platform and receives |TouchTransferState| coming from Browser. Input events
// are queued until state for corresponding touch sequence is received from
// Browser, similarly state is queued until we started receiving input events
// from Android platform.
class VIZ_SERVICE_EXPORT AndroidStateTransferHandler
    : public input::AndroidInputCallbackClient {
 public:
  explicit AndroidStateTransferHandler(
      AndroidStateTransferHandlerClient& client);
  ~AndroidStateTransferHandler();

  // `root_frame_sink_id` could be invalid. In cases when root frame sink is
  // destroyed and there was an ongoing touch sequence, OS may send a few events
  // before a touch cancel is sent for the ongoing touch sequence -
  // https://crbug.com/388478270#comment2 for more context.
  bool OnMotionEvent(base::android::ScopedInputEvent input_event,
                     const FrameSinkId& root_frame_sink_id) override;

  // `rir_support`: RenderInputRouterSupport corresponding to root widget's
  // FrameSinkId in TouchTransferState. `rir_support` can be null at the start
  // or in the middle of sequence in case CompositorFrameSink got destroyed.
  void StateOnTouchTransfer(
      input::mojom::TouchTransferStatePtr state,
      base::WeakPtr<RenderInputRouterSupportAndroidInterface> rir_support);

  size_t GetEventsBufferSizeForTesting() const { return events_buffer_.size(); }
  size_t GetPendingTransferredStatesSizeForTesting() const {
    return pending_transferred_states_.size();
  }

  static constexpr const char* kPendingTransfersHistogramNonNull =
      "Android.InputOnViz.Viz.PendingStateTransfers.NonNullCurrentState";
  static constexpr const char* kPendingTransfersHistogramNull =
      "Android.InputOnViz.Viz.PendingStateTransfers.NullCurrentState";

 private:
  bool CanStartProcessingVizEvents(
      const base::android::ScopedInputEvent& event);

  void HandleTouchEvent(base::android::ScopedInputEvent input_event);
  void MaybeDropEventsFromEarlierSequences(
      const input::mojom::TouchTransferStatePtr& state);
  void EmitPendingTransfersHistogram();
  void ValidateRootFrameSinkId(const FrameSinkId& root_frame_sink_id);

  bool ignore_remaining_touch_sequence_ = false;

  struct TransferState {
    TransferState(
        base::WeakPtr<RenderInputRouterSupportAndroidInterface> support,
        input::mojom::TouchTransferStatePtr transfer_state);
    TransferState(TransferState&& other);
    ~TransferState();

    base::WeakPtr<RenderInputRouterSupportAndroidInterface> rir_support;
    input::mojom::TouchTransferStatePtr transfer_state;
  };

  // State corresponding to active touch sequence.
  std::optional<TransferState> state_for_curr_sequence_ = std::nullopt;

  // The list maintains sorted order by key `TouchTransferState.down_time_ms`.
  // Any state transfer received out of order is dropped.
  base::queue<TransferState> pending_transferred_states_;
  static constexpr int kMaxPendingTransferredStates = 3;

  // Stores input events until we have received state from Browser for the
  // currently transferred touch sequence.
  base::queue<base::android::ScopedInputEvent> events_buffer_;

  const raw_ref<AndroidStateTransferHandlerClient> client_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_INPUT_ANDROID_STATE_TRANSFER_HANDLER_H_
