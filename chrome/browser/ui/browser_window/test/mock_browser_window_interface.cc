// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"

#include "chrome/browser/ui/unowned_user_data/unowned_user_data_host.h"

MockBrowserWindowInterface::MockBrowserWindowInterface() = default;
MockBrowserWindowInterface::~MockBrowserWindowInterface() = default;

UnownedUserDataHost& MockBrowserWindowInterface::GetUnownedUserDataHost() {
  return const_cast<UnownedUserDataHost&>(
      const_cast<const MockBrowserWindowInterface*>(this)
          ->GetUnownedUserDataHost());
}
