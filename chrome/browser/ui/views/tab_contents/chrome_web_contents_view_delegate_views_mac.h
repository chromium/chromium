// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TAB_CONTENTS_CHROME_WEB_CONTENTS_VIEW_DELEGATE_VIEWS_MAC_H_
#define CHROME_BROWSER_UI_VIEWS_TAB_CONTENTS_CHROME_WEB_CONTENTS_VIEW_DELEGATE_VIEWS_MAC_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "components/renderer_context_menu/context_menu_delegate.h"
#include "content/public/browser/web_contents_view_delegate.h"

class ChromeWebContentsViewFocusHelper;
class RenderViewContextMenuBase;
class WebDragBookmarkHandlerMac;

namespace content {
class RenderWidgetHostView;
class WebContents;
}  // namespace content

class ChromeWebContentsViewDelegateViewsMac
    : public content::WebContentsViewDelegate,
      public ContextMenuDelegate {
 public:
  explicit ChromeWebContentsViewDelegateViewsMac(
      content::WebContents* web_contents);

  ChromeWebContentsViewDelegateViewsMac(
      const ChromeWebContentsViewDelegateViewsMac&) = delete;
  ChromeWebContentsViewDelegateViewsMac& operator=(
      const ChromeWebContentsViewDelegateViewsMac&) = delete;

  ~ChromeWebContentsViewDelegateViewsMac() override;

  // WebContentsViewDelegate:
  gfx::NativeWindow GetNativeWindow() override;
  NSObject<RenderWidgetHostViewMacDelegate>* GetDelegateForHost(
      content::RenderWidgetHost* render_widget_host,
      bool is_popup) override;
  content::WebDragDestDelegate* GetDragDestDelegate() override;
  void ShowContextMenu(content::RenderFrameHost& render_frame_host,
                       const content::ContextMenuParams& params) override;
  void StoreFocus() override;
  bool RestoreFocus() override;
  void ResetStoredFocus() override;
  bool Focus() override;
  bool TakeFocus(bool reverse) override;
  void OnPerformingDrop(const content::DropData& drop_data,
                        DropCompletionCallback callback) override;

  // ContextMenuDelegate:
  std::unique_ptr<RenderViewContextMenuBase> BuildMenu(
      content::RenderFrameHost& render_frame_host,
      const content::ContextMenuParams& params) override;
  void ShowMenu(std::unique_ptr<RenderViewContextMenuBase> menu) override;

 private:
  content::RenderWidgetHostView* GetActiveRenderWidgetHostView() const;
  ChromeWebContentsViewFocusHelper* GetFocusHelper() const;

  // The WebContents that owns the view.
  raw_ptr<content::WebContents> web_contents_;

  // The context menu. Callbacks are asynchronous so we need to keep it around.
  std::unique_ptr<RenderViewContextMenuBase> context_menu_;

  // The chrome specific delegate that receives events from WebDragDestMac.
  std::unique_ptr<WebDragBookmarkHandlerMac> bookmark_handler_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TAB_CONTENTS_CHROME_WEB_CONTENTS_VIEW_DELEGATE_VIEWS_MAC_H_
