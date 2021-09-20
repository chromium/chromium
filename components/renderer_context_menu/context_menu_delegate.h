// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_RENDERER_CONTEXT_MENU_CONTEXT_MENU_DELEGATE_H_
#define COMPONENTS_RENDERER_CONTEXT_MENU_CONTEXT_MENU_DELEGATE_H_

#include <memory>

#include "base/macros.h"

class RenderViewContextMenuBase;

namespace content {
class WebContents;
struct ContextMenuParams;
}

// A ContextMenuDelegate can build and show renderer context menu.
class ContextMenuDelegate {
 public:
  explicit ContextMenuDelegate(content::WebContents* web_contents);

  ContextMenuDelegate(const ContextMenuDelegate&) = delete;
  ContextMenuDelegate& operator=(const ContextMenuDelegate&) = delete;

  virtual ~ContextMenuDelegate();

  static ContextMenuDelegate* FromWebContents(
      content::WebContents* web_contents);

  // Builds and returns a context menu for a context specified by |params|.
  // The returned value can be used to display the context menu.
  virtual std::unique_ptr<RenderViewContextMenuBase> BuildMenu(
      content::WebContents* web_contents,
      const content::ContextMenuParams& params) = 0;

  // Displays the context menu.
  virtual void ShowMenu(std::unique_ptr<RenderViewContextMenuBase> menu) = 0;
};

#endif  // COMPONENTS_RENDERER_CONTEXT_MENU_CONTEXT_MENU_DELEGATE_H_
