// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/waap/window_creation.h"

#include <utility>

#include "base/time/time.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/browser/ui/waap/initial_webui_window_metrics_manager.h"

namespace waap {

BrowserWindowInterface* CreateBrowserWindowAndRecordCreationSource(
    BrowserWindowCreateParams create_params,
    NewWindowCreationSource creation_source) {
  const base::TimeTicks now = base::TimeTicks::Now();
  BrowserWindowInterface* browser_window =
      CreateBrowserWindow(std::move(create_params));
  if (auto* manager = InitialWebUIWindowMetricsManager::From(browser_window)) {
    manager->SetWindowCreationInfo(creation_source, now);
  }
  return browser_window;
}

}  // namespace waap
