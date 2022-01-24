// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_TAB_CONTENTS_CHROME_WEB_CONTENTS_VIEW_DELEGATE_MAC_H_
#define CHROME_BROWSER_UI_COCOA_TAB_CONTENTS_CHROME_WEB_CONTENTS_VIEW_DELEGATE_MAC_H_

#if defined(__OBJC__)

#include <memory>

#import "base/mac/scoped_nsobject.h"
#include "components/renderer_context_menu/context_menu_delegate.h"
#include "content/public/browser/web_contents_view_delegate.h"

class RenderViewContextMenuBase;
class WebDragBookmarkHandlerMac;

namespace content {
class RenderWidgetHostView;
class WebContents;
}

@class FocusTracker;

// A chrome/ specific class that extends WebContentsViewMac with features that
// live in chrome/.
class ChromeWebContentsViewDelegateMac
    : public content::WebContentsViewDelegate,
      public ContextMenuDelegate {
 public:
  explicit ChromeWebContentsViewDelegateMac(content::WebContents* web_contents);

  ChromeWebContentsViewDelegateMac(const ChromeWebContentsViewDelegateMac&) =
      delete;
  ChromeWebContentsViewDelegateMac& operator=(
      const ChromeWebContentsViewDelegateMac&) = delete;

  ~ChromeWebContentsViewDelegateMac() override;

  // Overridden from WebContentsViewDelegate:
  gfx::NativeWindow GetNativeWindow() override;
  NSObject<RenderWidgetHostViewMacDelegate>* CreateRenderWidgetHostViewDelegate(
      content::RenderWidgetHost* render_widget_host,
      bool is_popup) override;
  content::WebDragDestDelegate* GetDragDestDelegate() override;
  void ShowContextMenu(content::RenderFrameHost& render_frame_host,
                       const content::ContextMenuParams& params) override;
  void StoreFocus() override;
  bool RestoreFocus() override;
  void ResetStoredFocus() override;

  // Overridden from ContextMenuDelegate.
  std::unique_ptr<RenderViewContextMenuBase> BuildMenu(
      content::RenderFrameHost& render_frame_host,
      const content::ContextMenuParams& params) override;
  void ShowMenu(std::unique_ptr<RenderViewContextMenuBase> menu) override;

 protected:
  content::WebContents* web_contents() { return web_contents_; }

 private:
  content::RenderWidgetHostView* GetActiveRenderWidgetHostView() const;
  NSWindow* GetNSWindowForFocusTracker() const;

  RenderViewContextMenuBase* CreateRenderViewContextMenu(
      content::RenderFrameHost& render_frame_host,
      const content::ContextMenuParams& params);

  // The context menu. Callbacks are asynchronous so we need to keep it around.
  std::unique_ptr<RenderViewContextMenuBase> context_menu_;

  // The chrome specific delegate that receives events from WebDragDestMac.
  std::unique_ptr<WebDragBookmarkHandlerMac> bookmark_handler_;

  // Keeps track of which NSView has focus so we can restore the focus when
  // focus returns.
  base::scoped_nsobject<FocusTracker> focus_tracker_;

  // The WebContents that owns the view.
  content::WebContents* web_contents_;
};

#endif  // __OBJC__

#endif  // CHROME_BROWSER_UI_COCOA_TAB_CONTENTS_CHROME_WEB_CONTENTS_VIEW_DELEGATE_MAC_H_
