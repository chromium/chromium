// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "omnibox_input_watcher.h"

#if !defined(OS_IOS)
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#endif  // !defined(OS_IOS)

namespace {

#if !defined(OS_IOS)
class OmniboxInputWatcherFactory : public BrowserContextKeyedServiceFactory {
 public:
  static OmniboxInputWatcher* GetForBrowserContext(
      content::BrowserContext* context) {
    return static_cast<OmniboxInputWatcher*>(
        GetInstance()->GetServiceForBrowserContext(context, true));
  }

  static OmniboxInputWatcherFactory* GetInstance() {
    return base::Singleton<OmniboxInputWatcherFactory>::get();
  }

  OmniboxInputWatcherFactory()
      : BrowserContextKeyedServiceFactory(
            "OmniboxInputWatcher",
            BrowserContextDependencyManager::GetInstance()) {}

 private:
  // BrowserContextKeyedServiceFactory overrides
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override {
    return new OmniboxInputWatcher();
  }

  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override {
    return context;
  }
};
#endif  // !defined(OS_IOS)

}  // namespace

OmniboxInputWatcher::OmniboxInputWatcher() = default;
OmniboxInputWatcher::~OmniboxInputWatcher() = default;

#if !defined(OS_IOS)
// static
OmniboxInputWatcher* OmniboxInputWatcher::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return OmniboxInputWatcherFactory::GetForBrowserContext(browser_context);
}
#endif  // !defined(OS_IOS)

void OmniboxInputWatcher::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void OmniboxInputWatcher::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void OmniboxInputWatcher::NotifyInputEntered() {
  for (auto& observer : observers_)
    observer.OnOmniboxInputEntered();
}
