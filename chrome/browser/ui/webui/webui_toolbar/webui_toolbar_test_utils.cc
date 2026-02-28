// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/webui_toolbar/webui_toolbar_test_utils.h"

MockReloadButtonPage::MockReloadButtonPage() = default;
MockReloadButtonPage::~MockReloadButtonPage() = default;

mojo::PendingRemote<toolbar_ui_api::mojom::ToolbarUIObserver>
MockReloadButtonPage::BindAndGetRemote() {
  return receiver_.BindNewPipeAndPassRemote();
}

void MockReloadButtonPage::Bind(
    mojo::PendingReceiver<toolbar_ui_api::mojom::ToolbarUIObserver> receiver) {
  receiver_.Bind(std::move(receiver));
}

void MockReloadButtonPage::FlushForTesting() {
  receiver_.FlushForTesting();
}

MockToolbarUIServiceDelegate::MockToolbarUIServiceDelegate() = default;
MockToolbarUIServiceDelegate::~MockToolbarUIServiceDelegate() = default;

MockBrowserControlsServiceDelegate::MockBrowserControlsServiceDelegate() =
    default;
MockBrowserControlsServiceDelegate::~MockBrowserControlsServiceDelegate() =
    default;

toolbar_ui_api::mojom::NavigationControlsStatePtr
CreateValidNavigationControlsState() {
  return toolbar_ui_api::mojom::NavigationControlsState::New(
      toolbar_ui_api::mojom::ReloadControlState::New(),
      toolbar_ui_api::mojom::SplitTabsControlState::New(),
      /*layout_constants_version=*/0);
}

MockCommandUpdater::MockCommandUpdater() = default;
MockCommandUpdater::~MockCommandUpdater() = default;
