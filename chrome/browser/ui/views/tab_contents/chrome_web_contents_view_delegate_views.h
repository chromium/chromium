// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TAB_CONTENTS_CHROME_WEB_CONTENTS_VIEW_DELEGATE_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_TAB_CONTENTS_CHROME_WEB_CONTENTS_VIEW_DELEGATE_VIEWS_H_

#include <memory>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/renderer_context_menu/context_menu_delegate.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/web_contents_view_delegate.h"
#include "ui/base/clipboard/clipboard_format_type.h"

class RenderViewContextMenuBase;
class ChromeWebContentsViewFocusHelper;

namespace content {
class WebContents;
class WebDragDestDelegate;
class RenderFrameHost;
}  // namespace content

namespace ui {
class DataTransferEndpoint;
}  // namespace ui

// A chrome specific class that extends WebContentsViewWin with features like
// focus management, which live in chrome.
class ChromeWebContentsViewDelegateViews
    : public content::WebContentsViewDelegate,
      public ContextMenuDelegate {
 public:
  explicit ChromeWebContentsViewDelegateViews(
      content::WebContents* web_contents);

  ChromeWebContentsViewDelegateViews(
      const ChromeWebContentsViewDelegateViews&) = delete;
  ChromeWebContentsViewDelegateViews& operator=(
      const ChromeWebContentsViewDelegateViews&) = delete;

  ~ChromeWebContentsViewDelegateViews() override;

  // Overridden from WebContentsViewDelegate:
  gfx::NativeWindow GetNativeWindow() override;
  content::WebDragDestDelegate* GetDragDestDelegate() override;
  void StoreFocus() override;
  bool RestoreFocus() override;
  void ResetStoredFocus() override;
  bool Focus() override;
  bool TakeFocus(bool reverse) override;
  void ShowContextMenu(content::RenderFrameHost& render_frame_host,
                       const content::ContextMenuParams& params) override;
  void ExecuteCommandForTesting(int command_id, int event_flags) override;
  bool IsContextMenuShowingForTesting() override;
  void OnPerformingDrop(const content::DropData& drop_data,
                        DropCompletionCallback callback) override;

  // Overridden from ContextMenuDelegate.
  std::unique_ptr<RenderViewContextMenuBase> BuildMenu(
      content::RenderFrameHost& render_frame_host,
      const content::ContextMenuParams& params) override;
  void BuildMenuAsync(
      content::RenderFrameHost& render_frame_host,
      const content::ContextMenuParams& params,
      base::OnceCallback<void(std::unique_ptr<RenderViewContextMenuBase>)>
          callback) override;
  void ShowMenu(std::unique_ptr<RenderViewContextMenuBase> menu) override;

 private:
  void OnReadAvailableTypes(
      content::GlobalRenderFrameHostId render_frame_host_id,
      const content::ContextMenuParams& params,
      std::optional<ui::DataTransferEndpoint> data_dst,
      base::OnceCallback<void(std::unique_ptr<RenderViewContextMenuBase>)>
          callback,
      std::vector<std::u16string> types);

  void OnGetAllAvailableFormats(
      content::GlobalRenderFrameHostId render_frame_host_id,
      const content::ContextMenuParams& params,
      base::OnceCallback<void(std::unique_ptr<RenderViewContextMenuBase>)>
          callback,
      base::flat_set<ui::ClipboardFormatType> formats);

  // The context menu is reset every time we show it, but we keep a pointer to
  // between uses so that it won't go out of scope before we're done with it.
  std::unique_ptr<RenderViewContextMenuBase> context_menu_;

  // The chrome specific delegate that receives events from WebDragDest.
  std::unique_ptr<content::WebDragDestDelegate> bookmark_handler_;

  raw_ptr<content::WebContents> web_contents_;

  bool is_paste_enabled_ = false;
  bool is_paste_and_match_style_enabled_ = false;

  ChromeWebContentsViewFocusHelper* GetFocusHelper() const;

  base::WeakPtrFactory<ChromeWebContentsViewDelegateViews> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_TAB_CONTENTS_CHROME_WEB_CONTENTS_VIEW_DELEGATE_VIEWS_H_
