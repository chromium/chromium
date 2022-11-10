// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_AUDIO_AUDIO_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_AUDIO_AUDIO_UI_H_

#include <memory>

#include "chrome/browser/ui/webui/ash/audio/audio.mojom.h"
#include "chrome/browser/ui/webui/ash/audio/audio_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace ash {

class AudioUI;

// WebUIConfig for chrome://audio
class AudioUIConfig : public content::DefaultWebUIConfig<AudioUI> {
 public:
  AudioUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUIAudioHost) {}

  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

// The WebUI Controller for chrome://audio
class AudioUI : public ui::MojoWebUIController,
                public audio::mojom::PageHandlerFactory {
 public:
  explicit AudioUI(content::WebUI* web_ui);
  AudioUI(const AudioUI&) = delete;
  AudioUI& operator=(const AudioUI&) = delete;
  ~AudioUI() override;

  void BindInterface(
      mojo::PendingReceiver<audio::mojom::PageHandlerFactory> receiver);

 private:
  void CreatePageHandler(
      mojo::PendingRemote<audio::mojom::Page> page,
      mojo::PendingReceiver<audio::mojom::PageHandler> receiver) override;
  std::unique_ptr<AudioHandler> page_handler_;
  mojo::Receiver<audio::mojom::PageHandlerFactory> factory_receiver_{this};
  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_AUDIO_AUDIO_UI_H_
