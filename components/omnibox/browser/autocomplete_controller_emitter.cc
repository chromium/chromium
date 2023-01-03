// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "autocomplete_controller_emitter.h"

#include "base/observer_list.h"
#include "build/build_config.h"

#if !BUILDFLAG(IS_IOS)
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#endif  // !BUILDFLAG(IS_IOS)

namespace {

#if !BUILDFLAG(IS_IOS)
class AutocompleteControllerEmitterFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static AutocompleteControllerEmitter* GetForBrowserContext(
      content::BrowserContext* context) {
    return static_cast<AutocompleteControllerEmitter*>(
        GetInstance()->GetServiceForBrowserContext(context, true));
  }

  static AutocompleteControllerEmitterFactory* GetInstance() {
    return base::Singleton<AutocompleteControllerEmitterFactory>::get();
  }

  AutocompleteControllerEmitterFactory()
      : BrowserContextKeyedServiceFactory(
            "AutocompleteControllerEmitter",
            BrowserContextDependencyManager::GetInstance()) {}

 private:
  // BrowserContextKeyedServiceFactory overrides

  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override {
    return new AutocompleteControllerEmitter();
  }

  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override {
    return context;
  }
};
#endif  // !BUILDFLAG(IS_IOS)

}  // namespace

AutocompleteControllerEmitter::AutocompleteControllerEmitter() = default;
AutocompleteControllerEmitter::~AutocompleteControllerEmitter() = default;

#if !BUILDFLAG(IS_IOS)
// static
AutocompleteControllerEmitter*
AutocompleteControllerEmitter::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return AutocompleteControllerEmitterFactory::GetForBrowserContext(
      browser_context);
}
#endif  // !BUILDFLAG(IS_IOS)

void AutocompleteControllerEmitter::AddObserver(
    AutocompleteController::Observer* observer) {
  observers_.AddObserver(observer);
}

void AutocompleteControllerEmitter::RemoveObserver(
    AutocompleteController::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void AutocompleteControllerEmitter::OnStart(AutocompleteController* controller,
                                            const AutocompleteInput& input) {
  for (auto& observer : observers_) {
    observer.OnStart(controller, input);
  }
}

void AutocompleteControllerEmitter::OnResultChanged(
    AutocompleteController* controller,
    bool default_match_changed) {
  for (auto& observer : observers_) {
    observer.OnResultChanged(controller, default_match_changed);
  }
}
