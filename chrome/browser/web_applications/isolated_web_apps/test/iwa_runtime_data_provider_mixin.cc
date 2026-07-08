// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/test/iwa_runtime_data_provider_mixin.h"

#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"
#include "components/webapps/isolated_web_apps/public/iwa_runtime_data_provider.h"

namespace web_app {

namespace {
class IwaRuntimeDataProviderInitializer : public ChromeBrowserMainExtraParts {
 public:
  explicit IwaRuntimeDataProviderInitializer(
      IwaRuntimeDataProvider& data_provider)
      : data_provider_(data_provider) {}
  ~IwaRuntimeDataProviderInitializer() override = default;

 protected:
  void PreCreateThreads() override {
    resetter_ = IwaRuntimeDataProvider::SetInstanceForTesting(&*data_provider_);
  }

 private:
  raw_ref<IwaRuntimeDataProvider> data_provider_;
  std::optional<base::AutoReset<IwaRuntimeDataProvider*>> resetter_;
};
}  // namespace

IwaRuntimeDataProviderMixin::IwaRuntimeDataProviderMixin(
    InProcessBrowserTestMixinHost* host,
    IwaRuntimeDataProvider& data_provider)
    : InProcessBrowserTestMixin(host), data_provider_(&data_provider) {}

IwaRuntimeDataProviderMixin::~IwaRuntimeDataProviderMixin() = default;

void IwaRuntimeDataProviderMixin::CreatedBrowserMainParts(
    content::BrowserMainParts* parts) {
  static_cast<ChromeBrowserMainParts*>(parts)->AddParts(
      std::make_unique<IwaRuntimeDataProviderInitializer>(*data_provider_));
}

void IwaRuntimeDataProviderMixin::TearDownOnMainThread() {
  data_provider_ = nullptr;
}

}  // namespace web_app
