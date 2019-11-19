// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_WEBVIEW_WEBVIEW_LAYOUT_MANAGER_H_
#define CHROMECAST_BROWSER_WEBVIEW_WEBVIEW_LAYOUT_MANAGER_H_

#include "base/macros.h"
#include "ui/aura/layout_manager.h"

namespace chromecast {

// Resizes the provided window to be the parent window's size.
class WebviewLayoutManager : public aura::LayoutManager {
 public:
  WebviewLayoutManager(aura::Window* root);
  ~WebviewLayoutManager() override;

  void OnWindowResized() override;
  void OnWindowAddedToLayout(aura::Window* child) override;
  void OnWillRemoveWindowFromLayout(aura::Window* child) override;
  void OnWindowRemovedFromLayout(aura::Window* child) override;
  void OnChildWindowVisibilityChanged(aura::Window* child,
                                      bool visible) override;
  void SetChildBounds(aura::Window* child,
                      const gfx::Rect& requested_bounds) override;

 private:
  aura::Window* root_;

  DISALLOW_COPY_AND_ASSIGN(WebviewLayoutManager);
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_WEBVIEW_WEBVIEW_LAYOUT_MANAGER_H_
