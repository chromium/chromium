// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/media/media_feeds_ui.h"

#include "base/bind.h"

#include "base/macros.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/media/feeds/media_feeds_converter.h"
#include "chrome/browser/media/feeds/media_feeds_fetcher.h"
#include "chrome/browser/media/feeds/media_feeds_service.h"
#include "chrome/browser/media/feeds/media_feeds_store.mojom-shared.h"
#include "chrome/browser/media/history/media_history_keyed_service.h"
#include "chrome/browser/media/history/media_history_keyed_service_factory.h"
#include "chrome/browser/media/history/media_history_store.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/dev_ui_browser_resources.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "media/base/media_switches.h"

MediaFeedsUI::MediaFeedsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  // Setup the data source behind chrome://media-feeds.
  std::unique_ptr<content::WebUIDataSource> source(
      content::WebUIDataSource::Create(chrome::kChromeUIMediaFeedsHost));
  source->AddResourcePath("media_data_table.js", IDR_MEDIA_DATA_TABLE_JS);
  source->AddResourcePath("media_feeds.js", IDR_MEDIA_FEEDS_JS);
  source->AddResourcePath("media_session.mojom-lite.js",
                          IDR_MEDIA_SESSION_MOJOM_LITE_JS);
  source->AddResourcePath("ui/gfx/geometry/mojom/geometry.mojom-lite.js",
                          IDR_UI_GEOMETRY_MOJOM_LITE_JS);
  source->AddResourcePath("media_feeds_store.mojom-lite.js",
                          IDR_MEDIA_FEEDS_STORE_MOJOM_LITE_JS);
  source->SetDefaultResource(IDR_MEDIA_FEEDS_HTML);
  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui), source.release());
}

WEB_UI_CONTROLLER_TYPE_IMPL(MediaFeedsUI)

MediaFeedsUI::~MediaFeedsUI() = default;

void MediaFeedsUI::BindInterface(
    mojo::PendingReceiver<media_feeds::mojom::MediaFeedsStore> pending) {
  receiver_.Add(this, std::move(pending));
}

void MediaFeedsUI::GetMediaFeeds(GetMediaFeedsCallback callback) {
  GetMediaHistoryService()->GetMediaFeeds(
      media_history::MediaHistoryKeyedService::GetMediaFeedsRequest(),
      std::move(callback));
}

void MediaFeedsUI::GetItemsForMediaFeed(int64_t feed_id,
                                        GetItemsForMediaFeedCallback callback) {
  GetMediaHistoryService()->GetMediaFeedItems(
      media_history::MediaHistoryKeyedService::GetMediaFeedItemsRequest::
          CreateItemsForDebug(feed_id),
      std::move(callback));
}

void MediaFeedsUI::FetchMediaFeed(int64_t feed_id,
                                  FetchMediaFeedCallback callback) {
  GetMediaFeedsService()->FetchMediaFeed(feed_id, /*bypass_cache=*/false,
                                         nullptr, std::move(callback));
}

void MediaFeedsUI::GetDebugInformation(GetDebugInformationCallback callback) {
  auto info = media_feeds::mojom::DebugInformation::New();

  info->safe_search_feature_enabled =
      base::FeatureList::IsEnabled(media::kMediaFeedsSafeSearch);
  info->safe_search_pref_value =
      GetProfile()->GetPrefs()->GetBoolean(prefs::kMediaFeedsSafeSearchEnabled);

  info->background_fetching_feature_enabled =
      base::FeatureList::IsEnabled(media::kMediaFeedsBackgroundFetching);
  info->background_fetching_pref_value = GetProfile()->GetPrefs()->GetBoolean(
      prefs::kMediaFeedsBackgroundFetching);

  std::move(callback).Run(std::move(info));
}

void MediaFeedsUI::SetSafeSearchEnabledPref(
    bool value,
    SetSafeSearchEnabledPrefCallback callback) {
  GetProfile()->GetPrefs()->SetBoolean(prefs::kMediaFeedsSafeSearchEnabled,
                                       value);

  std::move(callback).Run();
}

void MediaFeedsUI::SetBackgroundFetchingPref(
    bool value,
    SetBackgroundFetchingPrefCallback callback) {
  GetProfile()->GetPrefs()->SetBoolean(prefs::kMediaFeedsBackgroundFetching,
                                       value);

  std::move(callback).Run();
}

media_history::MediaHistoryKeyedService*
MediaFeedsUI::GetMediaHistoryService() {
  media_history::MediaHistoryKeyedService* service =
      media_history::MediaHistoryKeyedServiceFactory::GetForProfile(
          GetProfile());
  DCHECK(service);
  return service;
}

media_feeds::MediaFeedsService* MediaFeedsUI::GetMediaFeedsService() {
  media_feeds::MediaFeedsService* service =
      media_feeds::MediaFeedsService::Get(GetProfile());
  DCHECK(service);
  return service;
}

Profile* MediaFeedsUI::GetProfile() {
  Profile* profile = Profile::FromWebUI(web_ui());
  DCHECK(profile);
  return profile;
}
