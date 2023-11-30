// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted_memory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/record_replay/record_replay_ui.h"
#include "chrome/browser/ui/webui/record_replay/record_replay_manager_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/record_replay_resources.h"
#include "chrome/grit/record_replay_resources_map.h"
#include "content/public/browser/web_contents.h"

RecordReplayUI::RecordReplayUI(
    content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, /*enable_chrome_send=*/true) {
  content::WebUIDataSource* data_source =
      content::WebUIDataSource::CreateAndAdd(
          web_ui->GetWebContents()->GetBrowserContext(),
          chrome::kChromeUIRecordReplayHost);

  data_source->AddResourcePaths(base::make_span(
      kRecordReplayResources, kRecordReplayResourcesSize));
  data_source->AddResourcePath("", IDR_RECORD_REPLAY_RECORD_REPLAY_HTML);

  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui), data_source);
}

RecordReplayUI::~RecordReplayUI() = default;

void RecordReplayUI::BindInterface(
    mojo::PendingReceiver<mojom::RecordReplayManagerHandler> receiver) {
  record_replay_manager_handler_ = std::make_unique<RecordReplayManagerHandler>(
      Profile::FromWebUI(web_ui()), std::move(receiver));
}

WEB_UI_CONTROLLER_TYPE_IMPL(RecordReplayUI)
