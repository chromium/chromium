// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/webview/webview_layout_manager.h"

#include "ui/aura/window.h"

namespace chromecast {

WebviewLayoutManager::WebviewLayoutManager(aura::Window* root) : root_(root) {}

WebviewLayoutManager::~WebviewLayoutManager() {}

void WebviewLayoutManager::OnWindowResized() {
  for (aura::Window* child : root_->children())
    SetChildBoundsDirect(child, gfx::Rect(root_->bounds().size()));
}

void WebviewLayoutManager::OnWindowAddedToLayout(aura::Window* child) {}

void WebviewLayoutManager::OnWillRemoveWindowFromLayout(aura::Window* child) {}

void WebviewLayoutManager::OnWindowRemovedFromLayout(aura::Window* child) {}

void WebviewLayoutManager::OnChildWindowVisibilityChanged(aura::Window* child,
                                                          bool visible) {}

void WebviewLayoutManager::SetChildBounds(aura::Window* child,
                                          const gfx::Rect& requested_bounds) {
  SetChildBoundsDirect(child, requested_bounds);
}

}  // namespace chromecast
