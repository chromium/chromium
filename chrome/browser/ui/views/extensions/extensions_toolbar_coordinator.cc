// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_toolbar_coordinator.h"

#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container_view_controller.h"

ExtensionsToolbarCoordinator::ExtensionsToolbarCoordinator(
    Browser* browser,
    ExtensionsToolbarContainer* extensions_container) {
  extensions_container_tracker_.SetView(extensions_container);
  // Safe to use base::Unretained() because `this` owns / outlives
  // `extensions_container_tracker_`.
  extensions_container_tracker_.SetIsDeletingCallback(
      base::BindOnce(&ExtensionsToolbarCoordinator::ResetCoordinatorState,
                     base::Unretained(this)));
  extensions_container_tracker_.SetTrackEntireViewHierarchy(true);

  extensions_container_controller_ =
      std::make_unique<ExtensionsToolbarContainerViewController>(
          browser, extensions_container);
}

ExtensionsToolbarCoordinator::~ExtensionsToolbarCoordinator() = default;

void ExtensionsToolbarCoordinator::ResetCoordinatorState() {
  extensions_container_tracker_.SetView(nullptr);
  extensions_container_controller_.reset();
}
