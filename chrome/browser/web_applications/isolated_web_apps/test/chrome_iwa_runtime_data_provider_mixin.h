// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_TEST_CHROME_IWA_RUNTIME_DATA_PROVIDER_MIXIN_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_TEST_CHROME_IWA_RUNTIME_DATA_PROVIDER_MIXIN_H_

#include <type_traits>

#include "chrome/browser/web_applications/isolated_web_apps/runtime_data/chrome_iwa_runtime_data_provider.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"

namespace web_app {

// Mixin to inject `data_provider` right after browser process initialization.
// Doesn't own the `data_provider`.
class ChromeIwaRuntimeDataProviderMixin : public InProcessBrowserTestMixin {
 public:
  ChromeIwaRuntimeDataProviderMixin(
      InProcessBrowserTestMixinHost* host,
      ChromeIwaRuntimeDataProvider& data_provider);
  ~ChromeIwaRuntimeDataProviderMixin() override;

  void CreatedBrowserMainParts(content::BrowserMainParts*) override;
  void TearDownOnMainThread() override;

 private:
  raw_ptr<ChromeIwaRuntimeDataProvider> data_provider_ = nullptr;
};

// Mixin to create & inject an instance of <DataProvider> right after browser
// process initialization. Owns the instance.
template <typename DataProvider>
  requires(std::is_base_of_v<ChromeIwaRuntimeDataProvider, DataProvider>)
class TypedIwaRuntimeDataProviderMixin
    : public ChromeIwaRuntimeDataProviderMixin {
 public:
  template <typename... Args>
  explicit TypedIwaRuntimeDataProviderMixin(InProcessBrowserTestMixinHost* host,
                                            Args&&... args)
      : TypedIwaRuntimeDataProviderMixin(
            host,
            std::make_unique<DataProvider>(std::forward<Args>(args)...)) {}
  ~TypedIwaRuntimeDataProviderMixin() override = default;

  DataProvider* operator->() { return data_provider_.get(); }

 private:
  TypedIwaRuntimeDataProviderMixin(InProcessBrowserTestMixinHost* host,
                                   std::unique_ptr<DataProvider> data_provider)
      : ChromeIwaRuntimeDataProviderMixin(host, *data_provider),
        data_provider_(std::move(data_provider)) {}

  std::unique_ptr<DataProvider> data_provider_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_TEST_CHROME_IWA_RUNTIME_DATA_PROVIDER_MIXIN_H_
