// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/collaboration_messaging_observer_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/collaboration/messaging/messaging_backend_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/collaboration_messaging_observer.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"

namespace tab_groups {

// static
CollaborationMessagingObserver*
CollaborationMessagingObserverFactory::GetForProfile(Profile* profile) {
  DCHECK(profile);
  return static_cast<CollaborationMessagingObserver*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

CollaborationMessagingObserverFactory*
CollaborationMessagingObserverFactory::GetInstance() {
  static base::NoDestructor<CollaborationMessagingObserverFactory> instance;
  return instance.get();
}

CollaborationMessagingObserverFactory::CollaborationMessagingObserverFactory()
    : ProfileKeyedServiceFactory(
          "CollaborationMessagingObserver",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(
      collaboration::messaging::MessagingBackendServiceFactory::GetInstance());
}

CollaborationMessagingObserverFactory::
    ~CollaborationMessagingObserverFactory() = default;

std::unique_ptr<KeyedService>
CollaborationMessagingObserverFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<CollaborationMessagingObserver>(profile);
}

bool CollaborationMessagingObserverFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

}  // namespace tab_groups
