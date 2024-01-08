// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_LIBASSISTANT_MEDIA_CONTROLLER_H_
#define CHROMEOS_ASH_SERVICES_LIBASSISTANT_MEDIA_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/services/libassistant/grpc/assistant_client_observer.h"
#include "chromeos/ash/services/libassistant/public/mojom/media_controller.mojom.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/delegate/event_handler_interface.pb.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/device_state_event.pb.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash::libassistant {

class MediaController : public mojom::MediaController,
                        public AssistantClientObserver {
 public:
  MediaController();
  MediaController(const MediaController&) = delete;
  MediaController& operator=(const MediaController&) = delete;
  ~MediaController() override;

  void Bind(mojo::PendingReceiver<mojom::MediaController> receiver,
            mojo::PendingRemote<mojom::MediaDelegate> delegate);

  // mojom::MediaController implementation:
  void ResumeInternalMediaPlayer() override;
  void PauseInternalMediaPlayer() override;
  void SetExternalPlaybackState(mojom::MediaStatePtr state) override;

  // AssistantClientObserver implementation:
  void OnAssistantClientRunning(AssistantClient* assistant_client) override;

  void SendGrpcMessageForTesting(
      const ::assistant::api::OnDeviceStateEventRequest& request);
  void SendGrpcMessageForTesting(
      const ::assistant::api::OnMediaActionFallbackEventRequest& request);

 private:
  class GrpcEventsObserver;

  raw_ptr<AssistantClient> assistant_client_ = nullptr;

  mojo::Receiver<mojom::MediaController> receiver_{this};
  mojo::Remote<mojom::MediaDelegate> delegate_;
  std::unique_ptr<GrpcEventsObserver> events_observer_;
};

}  // namespace ash::libassistant

#endif  // CHROMEOS_ASH_SERVICES_LIBASSISTANT_MEDIA_CONTROLLER_H_
