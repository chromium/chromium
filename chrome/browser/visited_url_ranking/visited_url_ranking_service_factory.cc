// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/visited_url_ranking/visited_url_ranking_service_factory.h"

#include <map>
#include <memory>

#include "base/containers/flat_set.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/commerce/shopping_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/segmentation_platform/segmentation_platform_service_factory.h"
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "chrome/browser/sync/session_sync_service_factory.h"
#include "chrome/browser/visited_url_ranking/url_deduplication/search_engine_url_strip_handler.h"
#include "chrome/common/channel_info.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/history/core/browser/history_service.h"
#include "components/history_clusters/core/config.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sync_device_info/device_info_sync_service.h"
#include "components/url_deduplication/url_deduplication_helper.h"
#include "components/visited_url_ranking/internal/history_url_visit_data_fetcher.h"
#include "components/visited_url_ranking/internal/session_url_visit_data_fetcher.h"
#include "components/visited_url_ranking/internal/transformer/bookmarks_url_visit_aggregates_transformer.h"
#include "components/visited_url_ranking/internal/transformer/default_app_url_visit_aggregates_transformer.h"
#include "components/visited_url_ranking/internal/transformer/history_url_visit_aggregates_browser_type_transformer.h"
#include "components/visited_url_ranking/internal/transformer/history_url_visit_aggregates_categories_transformer.h"
#include "components/visited_url_ranking/internal/transformer/history_url_visit_aggregates_visibility_score_transformer.h"
#include "components/visited_url_ranking/internal/transformer/recency_filter_transformer.h"
#include "components/visited_url_ranking/internal/transformer/url_visit_aggregates_segmentation_metrics_transformer.h"
#include "components/visited_url_ranking/internal/visited_url_ranking_service_impl.h"
#include "components/visited_url_ranking/public/features.h"
#include "components/visited_url_ranking/public/url_visit_aggregates_transformer.h"
#include "components/visited_url_ranking/public/url_visit_data_fetcher.h"
#include "components/visited_url_ranking/public/url_visit_util.h"
#include "components/visited_url_ranking/public/visited_url_ranking_service.h"

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/visited_url_ranking/desktop_tab_model_url_visit_data_fetcher.h"
#elif BUILDFLAG(IS_ANDROID)
#include "chrome/browser/visited_url_ranking/android_tab_model_url_visit_data_fetcher.h"
#endif

namespace visited_url_ranking {

// static
VisitedURLRankingService* VisitedURLRankingServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<VisitedURLRankingService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
VisitedURLRankingServiceFactory*
VisitedURLRankingServiceFactory::GetInstance() {
  static base::NoDestructor<VisitedURLRankingServiceFactory> instance;
  return instance.get();
}

VisitedURLRankingServiceFactory::VisitedURLRankingServiceFactory()
    : ProfileKeyedServiceFactory(
          "VisitedURLRankingService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithGuest(ProfileSelection::kNone)
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {
  DependsOn(
      segmentation_platform::SegmentationPlatformServiceFactory::GetInstance());
  DependsOn(SessionSyncServiceFactory::GetInstance());
  DependsOn(BookmarkModelFactory::GetInstance());
  DependsOn(HistoryServiceFactory::GetInstance());
  DependsOn(DeviceInfoSyncServiceFactory::GetInstance());
}

VisitedURLRankingServiceFactory::~VisitedURLRankingServiceFactory() = default;

std::unique_ptr<KeyedService>
VisitedURLRankingServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(features::kVisitedURLRankingService)) {
    return nullptr;
  }

  auto* profile = Profile::FromBrowserContext(context);
  if (profile->IsOffTheRecord()) {
    return nullptr;
  }

  auto* sps =
      segmentation_platform::SegmentationPlatformServiceFactory::GetForProfile(
          profile);
  if (!sps) {
    return nullptr;
  }

  std::unique_ptr<url_deduplication::URLDeduplicationHelper>
      deduplication_helper;
  if (base::FeatureList::IsEnabled(
          visited_url_ranking::features::kVisitedURLRankingDeduplication)) {
    deduplication_helper = CreateDefaultURLDeduplicationHelper();
    if (features::kVisitedURLRankingDeduplicationSearchEngine.Get() &&
        profile) {
      deduplication_helper->AddStripHandler(
          std::make_unique<url_deduplication::SearchEngineURLStripHandler>(
              TemplateURLServiceFactory::GetForProfile(profile)));
    }
  }

  std::map<Fetcher, std::unique_ptr<URLVisitDataFetcher>> data_fetchers;
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
  data_fetchers.emplace(
      Fetcher::kTabModel,
      std::make_unique<visited_url_ranking::DesktopTabModelURLVisitDataFetcher>(
          profile));
#elif BUILDFLAG(IS_ANDROID)
  data_fetchers.emplace(
      Fetcher::kTabModel,
      std::make_unique<visited_url_ranking::AndroidTabModelURLVisitDataFetcher>(
          profile));
#endif

  sync_sessions::SessionSyncService* sss =
      SessionSyncServiceFactory::GetInstance()->GetForProfile(profile);
  if (sss) {
    data_fetchers.emplace(Fetcher::kSession,
                          std::make_unique<SessionURLVisitDataFetcher>(sss));
  }
  history::HistoryService* hs = HistoryServiceFactory::GetForProfile(
      profile, ServiceAccessType::IMPLICIT_ACCESS);
  if (hs) {
    syncer::DeviceInfoSyncService* device_info_sync_service =
        DeviceInfoSyncServiceFactory::GetForProfile(profile);
    data_fetchers.emplace(
        Fetcher::kHistory,
        std::make_unique<visited_url_ranking::HistoryURLVisitDataFetcher>(
            hs, device_info_sync_service));
  }

  // TODO(crbug.com/349317344): Move transformers map to a shareable helper
  // method to avoid having to update the various factories when adding new
  // transformers.
  std::map<URLVisitAggregatesTransformType,
           std::unique_ptr<URLVisitAggregatesTransformer>>
      transformers = {};
  transformers.emplace(
      URLVisitAggregatesTransformType::kSegmentationMetricsData,
      std::make_unique<URLVisitAggregatesSegmentationMetricsTransformer>(sps));
  auto* bookmark_model = BookmarkModelFactory::GetForBrowserContext(profile);
  if (bookmark_model) {
    auto bookmarks_transformer =
        std::make_unique<BookmarksURLVisitAggregatesTransformer>(
            bookmark_model);
    transformers.emplace(URLVisitAggregatesTransformType::kBookmarkData,
                         std::move(bookmarks_transformer));
  }
  transformers.emplace(
      URLVisitAggregatesTransformType::kHistoryVisibilityScoreFilter,
      std::make_unique<HistoryURLVisitAggregatesVisibilityScoreTransformer>(
          history_clusters::Config().content_visibility_threshold));
  transformers.emplace(
      URLVisitAggregatesTransformType::kHistoryCategoriesFilter,
      std::make_unique<HistoryURLVisitAggregatesCategoriesTransformer>(
          base::flat_set<std::string>(kBlocklistedCategories.begin(),
                                      kBlocklistedCategories.end())));
  transformers.emplace(URLVisitAggregatesTransformType::kRecencyFilter,
                       std::make_unique<RecencyFilterTransformer>());

#if BUILDFLAG(IS_ANDROID)
  base::flat_set<std::string_view> default_app_blocklist(
      kDefaultAppBlocklist.begin(), kDefaultAppBlocklist.end());
  auto default_app_transformer =
      std::make_unique<DefaultAppURLVisitAggregatesTransformer>(
          std::move(default_app_blocklist));
  transformers.emplace(URLVisitAggregatesTransformType::kDefaultAppUrlFilter,
                       std::move(default_app_transformer));
  transformers.emplace(
      URLVisitAggregatesTransformType::kHistoryBrowserTypeFilter,
      std::make_unique<HistoryURLVisitAggregatesBrowserTypeTransformer>());
#endif  // BUILDFLAG(IS_ANDROID)

  return std::make_unique<VisitedURLRankingServiceImpl>(
      sps, std::move(data_fetchers), std::move(transformers),
      std::move(deduplication_helper));
}

bool VisitedURLRankingServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

bool VisitedURLRankingServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace visited_url_ranking
