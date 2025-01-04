// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_COLLABORATION_MESSAGING_OBSERVER_FACTORY_H_
#define CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_COLLABORATION_MESSAGING_OBSERVER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace tab_groups {

class CollaborationMessagingObserver;

class CollaborationMessagingObserverFactory
    : public ProfileKeyedServiceFactory {
 public:
  static CollaborationMessagingObserver* GetForProfile(Profile* profile);
  static CollaborationMessagingObserverFactory* GetInstance();

 private:
  friend base::NoDestructor<CollaborationMessagingObserverFactory>;

  CollaborationMessagingObserverFactory();
  ~CollaborationMessagingObserverFactory() override;

  // BrowserContextKeyedServiceFactory overrides.
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace tab_groups

#endif  // CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_COLLABORATION_MESSAGING_OBSERVER_FACTORY_H_
