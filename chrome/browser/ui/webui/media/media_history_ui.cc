// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/media/media_history_ui.h"

#include "base/functional/bind.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/media/history/media_history_keyed_service.h"
#include "chrome/browser/media/history/media_history_keyed_service_factory.h"
#include "chrome/browser/media/history/media_history_store.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/media_resources.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"

MediaHistoryUI::MediaHistoryUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  // Setup the data source behind chrome://media-history.
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      Profile::FromWebUI(web_ui), chrome::kChromeUIMediaHistoryHost);
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources chrome://webui-test 'self';");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::TrustedTypes,
      "trusted-types static-types;");
  source->AddResourcePath("media_data_table.js", IDR_MEDIA_MEDIA_DATA_TABLE_JS);
  source->AddResourcePath("media_history.js", IDR_MEDIA_MEDIA_HISTORY_JS);
  source->AddResourcePath("media_session.mojom-webui.js",
                          IDR_MEDIA_MEDIA_SESSION_MOJOM_WEBUI_JS);
  source->AddResourcePath("media_history_store.mojom-webui.js",
                          IDR_MEDIA_MEDIA_HISTORY_STORE_MOJOM_WEBUI_JS);
  source->SetDefaultResource(IDR_MEDIA_MEDIA_HISTORY_HTML);
}

WEB_UI_CONTROLLER_TYPE_IMPL(MediaHistoryUI)

MediaHistoryUI::~MediaHistoryUI() = default;

void MediaHistoryUI::BindInterface(
    mojo::PendingReceiver<media_history::mojom::MediaHistoryStore> pending) {
  receivers_.Add(this, std::move(pending));
}

void MediaHistoryUI::GetMediaHistoryStats(
    GetMediaHistoryStatsCallback callback) {
  return GetMediaHistoryService()->GetMediaHistoryStats(std::move(callback));
}

void MediaHistoryUI::GetMediaHistoryOriginRows(
    GetMediaHistoryOriginRowsCallback callback) {
  return GetMediaHistoryService()->GetOriginRowsForDebug(std::move(callback));
}

void MediaHistoryUI::GetMediaHistoryPlaybackRows(
    GetMediaHistoryPlaybackRowsCallback callback) {
  return GetMediaHistoryService()->GetMediaHistoryPlaybackRowsForDebug(
      std::move(callback));
}

void MediaHistoryUI::GetMediaHistoryPlaybackSessionRows(
    GetMediaHistoryPlaybackSessionRowsCallback callback) {
  return GetMediaHistoryService()->GetPlaybackSessions(
      absl::nullopt, absl::nullopt, std::move(callback));
}

media_history::MediaHistoryKeyedService*
MediaHistoryUI::GetMediaHistoryService() {
  Profile* profile = Profile::FromWebUI(web_ui());
  DCHECK(profile);

  media_history::MediaHistoryKeyedService* service =
      media_history::MediaHistoryKeyedServiceFactory::GetForProfile(profile);
  DCHECK(service);
  return service;
}
