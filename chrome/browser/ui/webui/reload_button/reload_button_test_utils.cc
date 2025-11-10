// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/reload_button/reload_button_test_utils.h"

MockReloadButtonPage::MockReloadButtonPage() = default;
MockReloadButtonPage::~MockReloadButtonPage() = default;

mojo::PendingRemote<reload_button::mojom::Page>
MockReloadButtonPage::BindAndGetRemote() {
  return receiver_.BindNewPipeAndPassRemote();
}

void MockReloadButtonPage::FlushForTesting() {
  receiver_.FlushForTesting();
}

MockCommandUpdater::MockCommandUpdater() = default;
MockCommandUpdater::~MockCommandUpdater() = default;
