// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "omnibox_suggestions_watcher.h"

#if !defined(OS_IOS)
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#endif  // !defined(OS_IOS)

namespace {

#if !defined(OS_IOS)
class OmniboxSuggestionsWatcherFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static OmniboxSuggestionsWatcher* GetForBrowserContext(
      content::BrowserContext* context) {
    return static_cast<OmniboxSuggestionsWatcher*>(
        GetInstance()->GetServiceForBrowserContext(context, true));
  }

  static OmniboxSuggestionsWatcherFactory* GetInstance() {
    return base::Singleton<OmniboxSuggestionsWatcherFactory>::get();
  }

  OmniboxSuggestionsWatcherFactory()
      : BrowserContextKeyedServiceFactory(
            "OmniboxSuggestionsWatcher",
            BrowserContextDependencyManager::GetInstance()) {}

 private:
  // BrowserContextKeyedServiceFactory overrides
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override {
    return new OmniboxSuggestionsWatcher();
  }

  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override {
    return context;
  }
};
#endif  // !defined(OS_IOS)

}  // namespace

OmniboxSuggestionsWatcher::OmniboxSuggestionsWatcher() = default;
OmniboxSuggestionsWatcher::~OmniboxSuggestionsWatcher() = default;

#if !defined(OS_IOS)
// static
OmniboxSuggestionsWatcher* OmniboxSuggestionsWatcher::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return OmniboxSuggestionsWatcherFactory::GetForBrowserContext(
      browser_context);
}
#endif  // !defined(OS_IOS)

void OmniboxSuggestionsWatcher::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void OmniboxSuggestionsWatcher::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void OmniboxSuggestionsWatcher::NotifySuggestionsReady(
    extensions::api::omnibox::SendSuggestions::Params* suggestions) {
  for (auto& observer : observers_)
    observer.OnOmniboxSuggestionsReady(suggestions);
}

void OmniboxSuggestionsWatcher::NotifyDefaultSuggestionChanged() {
  for (auto& observer : observers_)
    observer.OnOmniboxDefaultSuggestionChanged();
}
