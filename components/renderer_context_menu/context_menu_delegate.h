// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_RENDERER_CONTEXT_MENU_CONTEXT_MENU_DELEGATE_H_
#define COMPONENTS_RENDERER_CONTEXT_MENU_CONTEXT_MENU_DELEGATE_H_

#include <memory>
#include "base/memory/raw_ptr.h"

class RenderViewContextMenuBase;

namespace content {
class RenderFrameHost;
class WebContents;
struct ContextMenuParams;
}

// A ContextMenuDelegate can build and show renderer context menu.
class ContextMenuDelegate {
 public:
  explicit ContextMenuDelegate(content::WebContents* web_contents);

  ContextMenuDelegate(const ContextMenuDelegate&) = delete;
  ContextMenuDelegate& operator=(const ContextMenuDelegate&) = delete;

  void ClearWebContents();

  virtual ~ContextMenuDelegate();

  static ContextMenuDelegate* FromWebContents(
      content::WebContents* web_contents);

  // Builds and returns a context menu for a context specified by |params|.
  //
  // The |render_frame_host| represents the frame where the context menu has
  // been opened (typically this frame is focused, but this is not necessarily
  // the case - see https://crbug.com/1257907#c14).
  //
  // The returned value can be used to display the context menu.
  virtual std::unique_ptr<RenderViewContextMenuBase> BuildMenu(
      content::RenderFrameHost& render_frame_host,
      const content::ContextMenuParams& params) = 0;

  // Displays the context menu.
  virtual void ShowMenu(std::unique_ptr<RenderViewContextMenuBase> menu) = 0;

 private:
  raw_ptr<content::WebContents> web_contents_ = nullptr;
};

#endif  // COMPONENTS_RENDERER_CONTEXT_MENU_CONTEXT_MENU_DELEGATE_H_
