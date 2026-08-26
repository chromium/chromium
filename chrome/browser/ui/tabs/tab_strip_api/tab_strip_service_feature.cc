// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_service_feature.h"

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/browser_apis/tab_strip/adapters/platform_adapters_provider.h"
#include "components/browser_apis/tab_strip/tab_strip_service_impl.h"

DEFINE_USER_DATA(TabStripServiceFeature);

// static
TabStripServiceFeature* TabStripServiceFeature::From(
    BrowserWindowInterface* browser) {
  return Get(browser->GetUnownedUserDataHost());
}

TabStripServiceFeature::TabStripServiceFeature(
    std::unique_ptr<tabs_api::PlatformAdaptersProvider> provider,
    ui::UnownedUserDataHost& host)
    : tab_strip_service_(
          std::make_unique<tabs_api::TabStripServiceImpl>(std::move(provider))),
      scoped_unowned_user_data_(host, *this) {}
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
