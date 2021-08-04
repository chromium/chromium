// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_AUDIO_AUDIO_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_AUDIO_AUDIO_HANDLER_H_

#include "chrome/browser/ui/webui/chromeos/audio/audio.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {

class AudioHandler : public audio::mojom::PageHandler {
 public:
  AudioHandler(mojo::PendingReceiver<audio::mojom::PageHandler> receiver,
               mojo::PendingRemote<audio::mojom::Page> page);
  AudioHandler(const AudioHandler&) = delete;
  AudioHandler& operator=(const AudioHandler&) = delete;
  ~AudioHandler() override;

  void GetAudioDeviceInfo(GetAudioDeviceInfoCallback callback) override;

 private:
  mojo::Remote<audio::mojom::Page> page_;
  mojo::Receiver<audio::mojom::PageHandler> receiver_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_AUDIO_AUDIO_HANDLER_H_
