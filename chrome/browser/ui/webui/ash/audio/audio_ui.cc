// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/ash/audio/audio_ui.h"

#include <memory>
#include <utility>

#include "ash/constants/ash_features.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/audio_resources.h"
#include "chrome/grit/audio_resources_map.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

namespace ash {

bool AudioUIConfig::IsWebUIEnabled(content::BrowserContext* browser_context) {
  return base::FeatureList::IsEnabled(features::kAudioUrl);
}

AudioUI::AudioUI(content::WebUI* web_ui) : ui::MojoWebUIController(web_ui) {
  // Set up the chrome://audio source.
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::CreateAndAdd(
          web_ui->GetWebContents()->GetBrowserContext(),
          chrome::kChromeUIAudioHost);

  webui::SetupWebUIDataSource(
      html_source, base::make_span(kAudioResources, kAudioResourcesSize),
      IDR_AUDIO_AUDIO_HTML);
}

WEB_UI_CONTROLLER_TYPE_IMPL(AudioUI)

AudioUI::~AudioUI() = default;

void AudioUI::BindInterface(
    mojo::PendingReceiver<audio::mojom::PageHandlerFactory> receiver) {
  factory_receiver_.reset();
  factory_receiver_.Bind(std::move(receiver));
}

void AudioUI::CreatePageHandler(
    mojo::PendingRemote<audio::mojom::Page> page,
    mojo::PendingReceiver<audio::mojom::PageHandler> receiver) {
  page_handler_ =
      std::make_unique<AudioHandler>(std::move(receiver), std::move(page));
}

}  // namespace ash
