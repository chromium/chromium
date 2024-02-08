// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_RENDERER_CONTEXT_MENU_RENDER_VIEW_CONTEXT_MENU_MAC_COCOA_H_
#define CHROME_BROWSER_UI_COCOA_RENDERER_CONTEXT_MENU_RENDER_VIEW_CONTEXT_MENU_MAC_COCOA_H_

#import <Cocoa/Cocoa.h>

#include "chrome/browser/ui/cocoa/renderer_context_menu/render_view_context_menu_mac.h"

@class MenuControllerCocoa;
@class MenuControllerCocoaDelegateImpl;

// Mac Cocoa implementation of the renderer context menu display code. Uses a
// NSMenu to display the context menu. Internally uses an Obj-C object as the
// target of the NSMenu, bridging back to this C++ class.
class RenderViewContextMenuMacCocoa : public RenderViewContextMenuMac {
 public:
  RenderViewContextMenuMacCocoa(content::RenderFrameHost& render_frame_host,
                                const content::ContextMenuParams& params,
                                NSView* parent_view);

  RenderViewContextMenuMacCocoa(const RenderViewContextMenuMacCocoa&) = delete;
  RenderViewContextMenuMacCocoa& operator=(
      const RenderViewContextMenuMacCocoa&) = delete;

  ~RenderViewContextMenuMacCocoa() override;

  // RenderViewContextMenu:
  void Show() override;

 private:
  // RenderViewContextMenuViewsMac:
  void CancelToolkitMenu() override;
  void UpdateToolkitMenuItem(int command_id,
                             bool enabled,
                             bool hidden,
                             const std::u16string& title) override;

  // The Cocoa menu controller for this menu.
  MenuControllerCocoa* __strong menu_controller_;
  MenuControllerCocoaDelegateImpl* __strong menu_controller_delegate_;

  // The Cocoa parent view.
  NSView* __weak parent_view_;
};

#endif  // CHROME_BROWSER_UI_COCOA_RENDERER_CONTEXT_MENU_RENDER_VIEW_CONTEXT_MENU_MAC_COCOA_H_
