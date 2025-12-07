// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"

#include "ui/base/unowned_user_data/unowned_user_data_host.h"

MockBrowserWindowInterface::MockBrowserWindowInterface() = default;
MockBrowserWindowInterface::~MockBrowserWindowInterface() = default;

ui::UnownedUserDataHost& MockBrowserWindowInterface::GetUnownedUserDataHost() {
  return const_cast<ui::UnownedUserDataHost&>(
      const_cast<const MockBrowserWindowInterface*>(this)
          ->GetUnownedUserDataHost());
}
