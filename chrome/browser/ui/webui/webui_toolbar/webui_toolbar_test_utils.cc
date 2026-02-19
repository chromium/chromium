// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/webui_toolbar/webui_toolbar_test_utils.h"

MockReloadButtonPage::MockReloadButtonPage() = default;
MockReloadButtonPage::~MockReloadButtonPage() = default;

mojo::PendingRemote<browser_controls_api::mojom::BrowserControlsObserver>
MockReloadButtonPage::BindAndGetRemote() {
  return receiver_.BindNewPipeAndPassRemote();
}

void MockReloadButtonPage::Bind(
    mojo::PendingReceiver<browser_controls_api::mojom::BrowserControlsObserver>
        receiver) {
  receiver_.Bind(std::move(receiver));
}

void MockReloadButtonPage::FlushForTesting() {
  receiver_.FlushForTesting();
}

MockWebWebUIToolbarDelegate::MockWebWebUIToolbarDelegate() = default;
MockWebWebUIToolbarDelegate::~MockWebWebUIToolbarDelegate() = default;

browser_controls_api::mojom::NavigationControlsStatePtr
CreateValidNavigationControlsState() {
  return browser_controls_api::mojom::NavigationControlsState::New(
      browser_controls_api::mojom::ReloadControlState::New(),
      browser_controls_api::mojom::SplitTabsControlState::New(),
      browser_controls_api::mojom::LayoutConstants::New());
}

MockCommandUpdater::MockCommandUpdater() = default;
MockCommandUpdater::~MockCommandUpdater() = default;
