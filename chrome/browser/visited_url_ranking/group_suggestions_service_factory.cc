// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/visited_url_ranking/group_suggestions_service_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/visited_url_ranking/visited_url_ranking_service_factory.h"
#include "components/visited_url_ranking/internal/url_grouping/group_suggestions_service_impl.h"
#include "components/visited_url_ranking/internal/url_grouping/tab_events_visit_transformer.h"
#include "components/visited_url_ranking/public/features.h"

namespace visited_url_ranking {

// static
GroupSuggestionsService* GroupSuggestionsServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<GroupSuggestionsService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
GroupSuggestionsServiceFactory* GroupSuggestionsServiceFactory::GetInstance() {
  static base::NoDestructor<GroupSuggestionsServiceFactory> instance;
  return instance.get();
}

GroupSuggestionsServiceFactory::GroupSuggestionsServiceFactory()
    : ProfileKeyedServiceFactory(
          "GroupSuggestionsService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithGuest(ProfileSelection::kNone)
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {
  DependsOn(VisitedURLRankingServiceFactory::GetInstance());
}

GroupSuggestionsServiceFactory::~GroupSuggestionsServiceFactory() = default;

std::unique_ptr<KeyedService>
GroupSuggestionsServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(features::kGroupSuggestionService)) {
    return nullptr;
  }

  auto* profile = Profile::FromBrowserContext(context);
  if (profile->IsOffTheRecord()) {
    return nullptr;
  }

  auto* visited_url_ranking_service =
      VisitedURLRankingServiceFactory::GetForProfile(profile);
  if (!visited_url_ranking_service) {
    return nullptr;
  }

  auto tab_events_visit_transformer =
      std::make_unique<TabEventsVisitTransformer>();
  TabEventsVisitTransformer* tab_events_visit_transformer_ptr =
      tab_events_visit_transformer.get();
  visited_url_ranking_service->RegisterTransformer(
      URLVisitAggregatesTransformType::kTabEventsData,
      std::move(tab_events_visit_transformer));

  return std::make_unique<GroupSuggestionsServiceImpl>(
      visited_url_ranking_service, tab_events_visit_transformer_ptr,
      profile->GetPrefs());
}

}  // namespace visited_url_ranking
