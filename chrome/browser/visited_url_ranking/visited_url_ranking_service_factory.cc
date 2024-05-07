// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/visited_url_ranking/visited_url_ranking_service_factory.h"

#include <map>
#include <memory>

#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/commerce/shopping_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/sync/session_sync_service_factory.h"
#include "chrome/common/channel_info.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/core_bookmark_model.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/visited_url_ranking/internal/session_url_visit_data_fetcher.h"
#include "components/visited_url_ranking/internal/transformer/bookmarks_url_visit_aggregates_transformer.h"
#include "components/visited_url_ranking/internal/visited_url_ranking_service_impl.h"
#include "components/visited_url_ranking/public/url_visit_aggregates_transformer.h"
#include "components/visited_url_ranking/public/url_visit_data_fetcher.h"
#include "components/visited_url_ranking/public/visited_url_ranking_service.h"

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
  DependsOn(SessionSyncServiceFactory::GetInstance());
  DependsOn(BookmarkModelFactory::GetInstance());
}

VisitedURLRankingServiceFactory::~VisitedURLRankingServiceFactory() = default;

std::unique_ptr<KeyedService>
VisitedURLRankingServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  auto* profile = Profile::FromBrowserContext(context);

  std::map<Fetcher, std::unique_ptr<URLVisitDataFetcher>> data_fetchers;
  sync_sessions::SessionSyncService* sss =
      SessionSyncServiceFactory::GetInstance()->GetForProfile(profile);
  if (sss) {
    data_fetchers.emplace(Fetcher::kSession,
                          std::make_unique<SessionURLVisitDataFetcher>(sss));
  }

  // TODO(crbug.com/329242209): Add various aggregate transformers (e.g,
  // shopping) to the service's map of supported transformers.
  std::map<URLVisitAggregatesTransformType,
           std::unique_ptr<URLVisitAggregatesTransformer>>
      transformers = {};
  auto* bookmark_model = BookmarkModelFactory::GetForBrowserContext(profile);
  if (bookmark_model) {
    auto bookmarks_transformer =
        std::make_unique<BookmarksURLVisitAggregatesTransformer>(
            bookmark_model);
    transformers.emplace(URLVisitAggregatesTransformType::kBookmarkData,
                         std::move(bookmarks_transformer));
  }

  return std::make_unique<VisitedURLRankingServiceImpl>(
      std::move(data_fetchers), std::move(transformers));
}

bool VisitedURLRankingServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

bool VisitedURLRankingServiceFactory::ServiceIsNULLWhileTesting() const {
  return false;
}

}  // namespace visited_url_ranking
