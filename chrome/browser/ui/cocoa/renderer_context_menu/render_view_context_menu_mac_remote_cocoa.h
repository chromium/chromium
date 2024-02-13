// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_RENDERER_CONTEXT_MENU_RENDER_VIEW_CONTEXT_MENU_MAC_REMOTE_COCOA_H_
#define CHROME_BROWSER_UI_COCOA_RENDERER_CONTEXT_MENU_RENDER_VIEW_CONTEXT_MENU_MAC_REMOTE_COCOA_H_

#include "chrome/browser/ui/cocoa/renderer_context_menu/render_view_context_menu_mac.h"
#include "ui/views/controls/menu/menu_runner_impl_remote_cocoa.h"

// Mac Remote Cocoa implementation of the renderer context menu display code.
// Delegates to views::MenuRunnerImplRemoteCocoa to use a NSMenu to display the
// context menu in a possibly remote process.
class RenderViewContextMenuMacRemoteCocoa : public RenderViewContextMenuMac {
 public:
  RenderViewContextMenuMacRemoteCocoa(
      content::RenderFrameHost& render_frame_host,
      const content::ContextMenuParams& params,
      content::RenderWidgetHostView* parent_view);

  RenderViewContextMenuMacRemoteCocoa(
      const RenderViewContextMenuMacRemoteCocoa&) = delete;
  RenderViewContextMenuMacRemoteCocoa& operator=(
      const RenderViewContextMenuMacRemoteCocoa&) = delete;

  ~RenderViewContextMenuMacRemoteCocoa() override;

  // RenderViewContextMenu:
  void Show() override;

 private:
  // RenderViewContextMenuViewsMac:
  void CancelToolkitMenu() override;
  void UpdateToolkitMenuItem(int command_id,
                             bool enabled,
                             bool hidden,
                             const std::u16string& title) override;

  raw_ptr<views::MenuRunnerImplRemoteCocoa> runner_ = nullptr;
  const uint64_t target_view_id_;
  const gfx::Rect target_view_bounds_;
};

#endif  // CHROME_BROWSER_UI_COCOA_RENDERER_CONTEXT_MENU_RENDER_VIEW_CONTEXT_MENU_MAC_REMOTE_COCOA_H_
