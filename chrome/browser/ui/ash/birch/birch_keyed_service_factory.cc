// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/birch/birch_keyed_service_factory.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "base/no_destructor.h"
#include "chrome/browser/ash/calendar/calendar_keyed_service_factory.h"
#include "chrome/browser/ash/file_suggest/file_suggest_keyed_service_factory.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/session_sync_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/ash/birch/birch_keyed_service.h"
#include "content/public/browser/browser_context.h"

namespace ash {

// static
BirchKeyedServiceFactory* BirchKeyedServiceFactory::GetInstance() {
  static base::NoDestructor<BirchKeyedServiceFactory> factory;
  return factory.get();
}

BirchKeyedServiceFactory::BirchKeyedServiceFactory()
    : ProfileKeyedServiceFactory(
          "BirchKeyedService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(FileSuggestKeyedServiceFactory::GetInstance());
  // Indirect dependency via BirchCalendarProvider.
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(SessionSyncServiceFactory::GetInstance());
  DependsOn(SyncServiceFactory::GetInstance());

  // Indirect dependency via calendar_utils, used by BirchCalendarProvider.
  DependsOn(CalendarKeyedServiceFactory::GetInstance());

  // Indirect dependencies via BirchLastActiveProvider and
  // BirchMostVisitedProvider.
  DependsOn(HistoryServiceFactory::GetInstance());
  DependsOn(FaviconServiceFactory::GetInstance());
}

BirchKeyedService* BirchKeyedServiceFactory::GetService(
    content::BrowserContext* context) {
  return static_cast<BirchKeyedService*>(
      GetInstance()->GetServiceForBrowserContext(
          context, /*create=*/features::IsForestFeatureEnabled()));
}

std::unique_ptr<KeyedService>
BirchKeyedServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<BirchKeyedService>(
      Profile::FromBrowserContext(context));
}

}  // namespace ash
