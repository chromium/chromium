// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_API_ANDROID_TAB_MODEL_IMPL_ANDROID_TAB_STRIP_API_INJECTOR_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_API_ANDROID_TAB_MODEL_IMPL_ANDROID_TAB_STRIP_API_INJECTOR_H_

#include "base/types/expected.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_api/adapters/event_bridge.h"
#include "chrome/browser/ui/tabs/tab_strip_api/adapters/platform_adapters_provider.h"
#include "chrome/browser/ui/tabs/tab_strip_api/adapters/translation_adapter.h"
#include "components/browser_apis/tab_strip/tab_strip_api.mojom.h"
#include "components/tabs/public/tab_interface.h"
#include "mojo/public/mojom/base/error.mojom.h"

namespace tabs_api {

class BrowserAdapter;
class TabStripModelAdapter;
class AndroidBrowserAdapterImpl;
class AndroidTabStripModelAdapter;

class UselessTranslationAdapter : public TranslationAdapter {
 public:
  UselessTranslationAdapter() = default;
  ~UselessTranslationAdapter() override = default;

  base::expected<mojom::TabPtr, mojo_base::mojom::ErrorPtr> ToMojoTab(
      tabs::TabHandle handle) override;
};

class UselessEventBridge : public EventBridge {
 public:
  UselessEventBridge() = default;
  ~UselessEventBridge() override = default;

  void AddObserver(events::EventObserver* observer) override {}
  void RemoveObserver(events::EventObserver* observer) override {}
};

class AndroidTabStripApiInjector : public PlatformAdaptersProvider {
 public:
  explicit AndroidTabStripApiInjector(TabModel* model);
  AndroidTabStripApiInjector(const AndroidTabStripApiInjector&&) = delete;
  AndroidTabStripApiInjector operator=(const AndroidTabStripApiInjector&) =
      delete;
  ~AndroidTabStripApiInjector() override;

  BrowserAdapter& browser_adapter() override;
  TabStripModelAdapter& tab_strip_model_adapter() override;
  TranslationAdapter& translation_adapter() override;
  EventBridge& event_bridge() override;

 private:
  std::unique_ptr<AndroidBrowserAdapterImpl> browser_adapter_;
  std::unique_ptr<AndroidTabStripModelAdapter> tab_model_adapter_;
  UselessEventBridge bridge_;
  UselessTranslationAdapter translator_;
};

}  // namespace tabs_api

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_API_ANDROID_TAB_MODEL_IMPL_ANDROID_TAB_STRIP_API_INJECTOR_H_
