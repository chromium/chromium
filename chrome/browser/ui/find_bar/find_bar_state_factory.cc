// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/find_bar/find_bar_state_factory.h"

#include "chrome/browser/ui/find_bar/find_bar_state.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

// static
FindBarState* FindBarStateFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<FindBarState*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
FindBarStateFactory* FindBarStateFactory::GetInstance() {
  return base::Singleton<FindBarStateFactory>::get();
}

FindBarStateFactory::FindBarStateFactory()
    : BrowserContextKeyedServiceFactory(
          "FindBarState",
          BrowserContextDependencyManager::GetInstance()) {}

FindBarStateFactory::~FindBarStateFactory() = default;

KeyedService* FindBarStateFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new FindBarState(context);
}

content::BrowserContext* FindBarStateFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // Separate instance in incognito.
  return context;
}
