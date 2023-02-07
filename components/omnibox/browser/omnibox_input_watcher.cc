// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "omnibox_input_watcher.h"
#include "base/observer_list.h"
#include "build/build_config.h"

#if !BUILDFLAG(IS_IOS)
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#endif  // !BUILDFLAG(IS_IOS)

namespace {

#if !BUILDFLAG(IS_IOS)
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
#endif  // !BUILDFLAG(IS_IOS)

}  // namespace

OmniboxInputWatcher::OmniboxInputWatcher() = default;
OmniboxInputWatcher::~OmniboxInputWatcher() = default;

#if !BUILDFLAG(IS_IOS)
// static
OmniboxInputWatcher* OmniboxInputWatcher::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return OmniboxInputWatcherFactory::GetForBrowserContext(browser_context);
}
#endif  // !BUILDFLAG(IS_IOS)

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

// static
void OmniboxInputWatcher::EnsureFactoryBuilt() {
  OmniboxInputWatcherFactory::GetInstance();
}
