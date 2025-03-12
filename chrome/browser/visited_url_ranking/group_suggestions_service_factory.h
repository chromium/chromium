// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VISITED_URL_RANKING_GROUP_SUGGESTIONS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_VISITED_URL_RANKING_GROUP_SUGGESTIONS_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/visited_url_ranking/public/url_grouping/group_suggestions_service.h"

namespace visited_url_ranking {

class GroupSuggestionsServiceFactory : public ProfileKeyedServiceFactory {
 public:
  GroupSuggestionsServiceFactory(const GroupSuggestionsServiceFactory&) =
      delete;
  GroupSuggestionsServiceFactory& operator=(
      const GroupSuggestionsServiceFactory&) = delete;

  static visited_url_ranking::GroupSuggestionsService* GetForProfile(
      Profile* profile);
  static GroupSuggestionsServiceFactory* GetInstance();

 private:
  friend base::NoDestructor<GroupSuggestionsServiceFactory>;

  GroupSuggestionsServiceFactory();
  ~GroupSuggestionsServiceFactory() override;

  // ProfileKeyedServiceFactory::
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace visited_url_ranking

#endif  // CHROME_BROWSER_VISITED_URL_RANKING_GROUP_SUGGESTIONS_SERVICE_FACTORY_H_
