// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/test/chrome_iwa_runtime_data_provider_mixin.h"

#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"
#include "chrome/browser/web_applications/isolated_web_apps/runtime_data/chrome_iwa_runtime_data_provider.h"

namespace web_app {

namespace {
class ChromeIwaRuntimeDataProviderInitializer
    : public ChromeBrowserMainExtraParts {
 public:
  explicit ChromeIwaRuntimeDataProviderInitializer(
      ChromeIwaRuntimeDataProvider& data_provider)
      : data_provider_(data_provider) {}
  ~ChromeIwaRuntimeDataProviderInitializer() override = default;

 protected:
  void PreCreateThreads() override {
    resetter_ =
        ChromeIwaRuntimeDataProvider::SetInstanceForTesting(&*data_provider_);
  }

 private:
  raw_ref<ChromeIwaRuntimeDataProvider> data_provider_;
  std::optional<base::AutoReset<ChromeIwaRuntimeDataProvider*>> resetter_;
};
}  // namespace

ChromeIwaRuntimeDataProviderMixin::ChromeIwaRuntimeDataProviderMixin(
    InProcessBrowserTestMixinHost* host,
    ChromeIwaRuntimeDataProvider& data_provider)
    : InProcessBrowserTestMixin(host), data_provider_(&data_provider) {}

ChromeIwaRuntimeDataProviderMixin::~ChromeIwaRuntimeDataProviderMixin() =
    default;

void ChromeIwaRuntimeDataProviderMixin::CreatedBrowserMainParts(
    content::BrowserMainParts* parts) {
  static_cast<ChromeBrowserMainParts*>(parts)->AddParts(
      std::make_unique<ChromeIwaRuntimeDataProviderInitializer>(
          *data_provider_));
}

void ChromeIwaRuntimeDataProviderMixin::TearDownOnMainThread() {
  data_provider_ = nullptr;
}

}  // namespace web_app
