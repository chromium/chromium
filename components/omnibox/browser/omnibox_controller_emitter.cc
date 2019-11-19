// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "omnibox_controller_emitter.h"

#if !defined(OS_IOS)
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#endif  // !defined(OS_IOS)

namespace {

#if !defined(OS_IOS)
class OmniboxControllerEmitterFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static OmniboxControllerEmitter* GetForBrowserContext(
      content::BrowserContext* context) {
    return static_cast<OmniboxControllerEmitter*>(
        GetInstance()->GetServiceForBrowserContext(context, true));
  }

  static OmniboxControllerEmitterFactory* GetInstance() {
    return base::Singleton<OmniboxControllerEmitterFactory>::get();
  }

  OmniboxControllerEmitterFactory()
      : BrowserContextKeyedServiceFactory(
            "OmniboxControllerEmitter",
            BrowserContextDependencyManager::GetInstance()) {}

 private:
  // BrowserContextKeyedServiceFactory overrides

  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override {
    return new OmniboxControllerEmitter();
  }

  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override {
    return context;
  }
};
#endif  // !defined(OS_IOS)

}  // namespace

OmniboxControllerEmitter::OmniboxControllerEmitter() {}
OmniboxControllerEmitter::~OmniboxControllerEmitter() {}

#if !defined(OS_IOS)
// static
OmniboxControllerEmitter* OmniboxControllerEmitter::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return OmniboxControllerEmitterFactory::GetForBrowserContext(browser_context);
}
#endif  // !defined(OS_IOS)

void OmniboxControllerEmitter::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void OmniboxControllerEmitter::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void OmniboxControllerEmitter::NotifyOmniboxQuery(
    AutocompleteController* controller,
    const AutocompleteInput& input) {
  for (Observer& observer : observers_)
    observer.OnOmniboxQuery(controller, input);
}

void OmniboxControllerEmitter::NotifyOmniboxResultChanged(
    bool default_match_changed,
    AutocompleteController* controller) {
  for (Observer& observer : observers_)
    observer.OnOmniboxResultChanged(default_match_changed, controller);
}
