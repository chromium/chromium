// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_service_feature.h"

#include "chrome/browser/ui/tabs/tab_strip_api/adapters/experimental_platform_adapters_provider.h"
#include "chrome/browser/ui/tabs/tab_strip_api/adapters/platform_adapters_provider.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_service_impl.h"

TabStripServiceFeature::TabStripServiceFeature(
    std::unique_ptr<tabs_api::PlatformAdaptersProvider> provider,
    std::unique_ptr<tabs_api::ExperimentalPlatformAdaptersProvider>
        experimental_provider)
    : tab_strip_service_(std::make_unique<tabs_api::TabStripServiceImpl>(
          std::move(provider),
          std::move(experimental_provider))) {}
TabStripServiceFeature::~TabStripServiceFeature() = default;

void TabStripServiceFeature::Accept(
    mojo::PendingReceiver<tabs_api::mojom::TabStripService> client) {
  tab_strip_service_->Accept(std::move(client));
}

void TabStripServiceFeature::AcceptExperimental(
    mojo::PendingReceiver<tabs_api::mojom::TabStripExperimentService> client) {
  tab_strip_service_->AcceptExperimental(std::move(client));
}

tabs_api::TabStripService* TabStripServiceFeature::GetTabStripService() const {
  return tab_strip_service_.get();
}
