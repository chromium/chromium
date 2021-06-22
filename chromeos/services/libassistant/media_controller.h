// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_LIBASSISTANT_MEDIA_CONTROLLER_H_
#define CHROMEOS_SERVICES_LIBASSISTANT_MEDIA_CONTROLLER_H_

#include "chromeos/services/libassistant/assistant_client_observer.h"
#include "chromeos/services/libassistant/public/mojom/media_controller.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace assistant_client {
class AssistantManager;
class MediaManager;
}  // namespace assistant_client

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
  void OnDestroyingAssistantClient(AssistantClient* assistant_client) override;
  void OnAssistantManagerDestroyed() override;

 private:
  class LibassistantMediaManagerListener;
  class LibassistantMediaHandler;

  assistant_client::MediaManager* media_manager();

  assistant_client::AssistantManager* assistant_manager_ = nullptr;

  mojo::Receiver<mojom::MediaController> receiver_{this};
  mojo::Remote<mojom::MediaDelegate> delegate_;
  std::unique_ptr<LibassistantMediaManagerListener> listener_;
  std::unique_ptr<LibassistantMediaHandler> handler_;
};

}  // namespace libassistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_LIBASSISTANT_MEDIA_CONTROLLER_H_
