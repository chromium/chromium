// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_ASSISTANT_TEST_SUPPORT_LIBASSISTANT_MEDIA_CONTROLLER_MOCK_H_
#define CHROMEOS_ASH_SERVICES_ASSISTANT_TEST_SUPPORT_LIBASSISTANT_MEDIA_CONTROLLER_MOCK_H_

#include "chromeos/services/libassistant/public/mojom/media_controller.mojom.h"

#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {
namespace assistant {

class LibassistantMediaControllerMock
    : public chromeos::libassistant::mojom::MediaController {
 public:
  LibassistantMediaControllerMock();
  LibassistantMediaControllerMock(const LibassistantMediaControllerMock&) =
      delete;
  LibassistantMediaControllerMock& operator=(
      const LibassistantMediaControllerMock&) = delete;
  ~LibassistantMediaControllerMock() override;

  void Bind(
      mojo::PendingReceiver<chromeos::libassistant::mojom::MediaController>);
  void FlushForTesting();

  // chromeos::libassistant::mojom::MediaController implementation:
  MOCK_METHOD(void, ResumeInternalMediaPlayer, ());
  MOCK_METHOD(void, PauseInternalMediaPlayer, ());
  MOCK_METHOD(void,
              SetExternalPlaybackState,
              (chromeos::libassistant::mojom::MediaStatePtr state));

 private:
  mojo::Receiver<chromeos::libassistant::mojom::MediaController> receiver_{
      this};
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_ASH_SERVICES_ASSISTANT_TEST_SUPPORT_LIBASSISTANT_MEDIA_CONTROLLER_MOCK_H_
