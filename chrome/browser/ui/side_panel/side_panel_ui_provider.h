// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIDE_PANEL_SIDE_PANEL_UI_PROVIDER_H_
#define CHROME_BROWSER_UI_SIDE_PANEL_SIDE_PANEL_UI_PROVIDER_H_

class BrowserWindowInterface;
class SidePanelUI;

// Provides `SidePanelUI` (the cross-platform interface).
//
// Instead of implementing the "provider" functionality in `SidePanelUI`, we
// need a separate class in a separate build target because:
// (1) The "provider" code depends on `SidePanelUI` implementations on different
//     platforms, and
// (2) Those implementations depend on `SidePanelUI`.
//
// Therefore, if the "provider" code is in `SidePanelUI`, there will be circular
// dependencies:
//   `SidePanelUI` ->
//     `SidePanelUIBase` ->
//       `SidePanelCoordinator`/`SidePanelCoordinatorAndroid` ->
//     `SidePanelUIBase` ->
//   `SidePanelUI`
class SidePanelUIProvider {
 public:
  // Returns the `SidePanelUI` associated with the given `browser`.
  //
  // TODO(crbug.com/495379184): Move this to `SidePanelUI` when it owns
  // `UnownedUserData` for all its implementations.
  static SidePanelUI* From(BrowserWindowInterface* browser);

  SidePanelUIProvider() = delete;
};

#endif  // CHROME_BROWSER_UI_SIDE_PANEL_SIDE_PANEL_UI_PROVIDER_H_
