// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/android_tab_model_impl/android_tab_strip_api_injector.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "chrome/browser/ui/tabs/tab_strip_api/android_tab_model_impl/android_browser_adapter_impl.h"
#include "chrome/browser/ui/tabs/tab_strip_api/android_tab_model_impl/android_tab_model_event_bridge.h"
#include "chrome/browser/ui/tabs/tab_strip_api/android_tab_model_impl/android_tab_strip_model_adapter.h"
#include "chrome/browser/ui/tabs/tab_strip_api/android_tab_model_impl/android_translation_adapter.h"
#include "components/browser_apis/tab_strip/adapters/event_bridge.h"
#include "components/browser_apis/tab_strip/adapters/translation_adapter.h"

namespace tabs_api {

namespace {

class NoOpEventBridge : public EventBridge {
 public:
  ~NoOpEventBridge() override = default;
  void AddObserver(events::EventObserver* observer) override {}
  void RemoveObserver(events::EventObserver* observer) override {}
};

}  // namespace

// static
std::unique_ptr<AndroidTabStripApiInjector>
AndroidTabStripApiInjector::CreateWithoutEventBridge(
    TabModel* model,
    TabModel::TabLaunchType launch_type) {
  return base::WrapUnique(new AndroidTabStripApiInjector(
      model, launch_type, std::make_unique<NoOpEventBridge>()));
}

AndroidTabStripApiInjector::AndroidTabStripApiInjector(
    TabModel* model,
    TabModel::TabLaunchType launch_type)
    : AndroidTabStripApiInjector(model, launch_type, nullptr) {
  event_bridge_ = std::make_unique<AndroidTabModelEventBridge>(
      model, *tab_model_adapter_, *translation_adapter_);
}

AndroidTabStripApiInjector::AndroidTabStripApiInjector(
    TabModel* model,
    TabModel::TabLaunchType launch_type,
    std::unique_ptr<EventBridge> event_bridge)
    : browser_adapter_(
          std::make_unique<AndroidBrowserAdapterImpl>(model, launch_type)),
      tab_model_adapter_(std::make_unique<AndroidTabStripModelAdapter>(model)),
      translation_adapter_(
          std::make_unique<AndroidTranslationAdapter>(model,
                                                      *tab_model_adapter_)),
      event_bridge_(std::move(event_bridge)) {}

AndroidTabStripApiInjector::~AndroidTabStripApiInjector() = default;

BrowserAdapter& AndroidTabStripApiInjector::browser_adapter() {
  return *browser_adapter_;
}

TabStripModelAdapter& AndroidTabStripApiInjector::tab_strip_model_adapter() {
  return *tab_model_adapter_;
}

TranslationAdapter& AndroidTabStripApiInjector::translation_adapter() {
  return *translation_adapter_;
}

EventBridge& AndroidTabStripApiInjector::event_bridge() {
  return *event_bridge_;
}

}  // namespace tabs_api
