// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "omnibox_watcher.h"

#if !defined(OS_IOS)
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#endif  // !defined(OS_IOS)

namespace {

#if !defined(OS_IOS)
class OmniboxWatcherFactory : public BrowserContextKeyedServiceFactory {
 public:
  static OmniboxWatcher* GetForBrowserContext(
      content::BrowserContext* context) {
    return static_cast<OmniboxWatcher*>(
        GetInstance()->GetServiceForBrowserContext(context, true));
  }

  static OmniboxWatcherFactory* GetInstance() {
    return base::Singleton<OmniboxWatcherFactory>::get();
  }

  OmniboxWatcherFactory()
      : BrowserContextKeyedServiceFactory(
            "OmniboxWatcher",
            BrowserContextDependencyManager::GetInstance()) {}

 private:
  // BrowserContextKeyedServiceFactory overrides
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override {
    return new OmniboxWatcher();
  }

  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override {
    return context;
  }
};
#endif  // !defined(OS_IOS)

}  // namespace

OmniboxWatcher::OmniboxWatcher() = default;
OmniboxWatcher::~OmniboxWatcher() = default;

#if !defined(OS_IOS)
// static
OmniboxWatcher* OmniboxWatcher::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return OmniboxWatcherFactory::GetForBrowserContext(browser_context);
}
#endif  // !defined(OS_IOS)

void OmniboxWatcher::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void OmniboxWatcher::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void OmniboxWatcher::NotifyInputEntered() {
  for (auto& observer : observers_)
    observer.OnOmniboxInputEntered();
}
