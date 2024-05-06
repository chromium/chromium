// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VISITED_URL_RANKING_VISITED_URL_RANKING_SERVICE_FACTORY_H_
#define CHROME_BROWSER_VISITED_URL_RANKING_VISITED_URL_RANKING_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace visited_url_ranking {

class VisitedURLRankingService;

// Singleton that owns all `VisitedURLRankingService` instances and associates
// them with profiles.
class VisitedURLRankingServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static visited_url_ranking::VisitedURLRankingService* GetForProfile(
      Profile* profile);
  static VisitedURLRankingServiceFactory* GetInstance();
  VisitedURLRankingServiceFactory(const VisitedURLRankingServiceFactory&) =
      delete;

 private:
  friend base::NoDestructor<VisitedURLRankingServiceFactory>;

  VisitedURLRankingServiceFactory();
  ~VisitedURLRankingServiceFactory() override;

  // ProfileKeyedServiceFactory::
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
};

}  // namespace visited_url_ranking

#endif  // CHROME_BROWSER_VISITED_URL_RANKING_VISITED_URL_RANKING_SERVICE_FACTORY_H_
