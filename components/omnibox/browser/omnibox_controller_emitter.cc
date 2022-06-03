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

void OmniboxControllerEmitter::AddObserver(
    AutocompleteController::Observer* observer) {
  observers_.AddObserver(observer);
}

void OmniboxControllerEmitter::RemoveObserver(
    AutocompleteController::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void OmniboxControllerEmitter::OnStart(AutocompleteController* controller,
                                       const AutocompleteInput& input) {
  for (auto& observer : observers_)
    observer.OnStart(controller, input);
}

void OmniboxControllerEmitter::OnResultChanged(
    AutocompleteController* controller,
    bool default_match_changed) {
  for (auto& observer : observers_)
    observer.OnResultChanged(controller, default_match_changed);
}
