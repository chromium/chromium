// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_ASSISTANT_TEST_SUPPORT_LIBASSISTANT_MEDIA_CONTROLLER_MOCK_H_
#define CHROMEOS_ASH_SERVICES_ASSISTANT_TEST_SUPPORT_LIBASSISTANT_MEDIA_CONTROLLER_MOCK_H_

#include "chromeos/ash/services/libassistant/public/mojom/media_controller.mojom.h"

#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash::assistant {

class LibassistantMediaControllerMock
    : public libassistant::mojom::MediaController {
 public:
  LibassistantMediaControllerMock();
  LibassistantMediaControllerMock(const LibassistantMediaControllerMock&) =
      delete;
  LibassistantMediaControllerMock& operator=(
      const LibassistantMediaControllerMock&) = delete;
  ~LibassistantMediaControllerMock() override;

  void Bind(mojo::PendingReceiver<libassistant::mojom::MediaController>);
  void FlushForTesting();

  // libassistant::mojom::MediaController implementation:
  MOCK_METHOD(void, ResumeInternalMediaPlayer, ());
  MOCK_METHOD(void, PauseInternalMediaPlayer, ());
  MOCK_METHOD(void,
              SetExternalPlaybackState,
              (libassistant::mojom::MediaStatePtr state));

 private:
  mojo::Receiver<libassistant::mojom::MediaController> receiver_{this};
};

}  // namespace ash::assistant

#endif  // CHROMEOS_ASH_SERVICES_ASSISTANT_TEST_SUPPORT_LIBASSISTANT_MEDIA_CONTROLLER_MOCK_H_
