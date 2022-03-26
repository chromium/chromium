// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_LIBASSISTANT_MEDIA_CONTROLLER_H_
#define CHROMEOS_SERVICES_LIBASSISTANT_MEDIA_CONTROLLER_H_

#include "chromeos/assistant/internal/proto/shared/proto/v2/device_state_event.pb.h"
#include "chromeos/services/libassistant/grpc/assistant_client_observer.h"
#include "chromeos/services/libassistant/public/mojom/media_controller.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {
namespace libassistant {

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

 private:
  class GrpcEventsObserver;

  AssistantClient* assistant_client_ = nullptr;

  mojo::Receiver<mojom::MediaController> receiver_{this};
  mojo::Remote<mojom::MediaDelegate> delegate_;
  std::unique_ptr<GrpcEventsObserver> events_observer_;
};

}  // namespace libassistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_LIBASSISTANT_MEDIA_CONTROLLER_H_
