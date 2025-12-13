// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_MULTI_CONTENTS_VIEW_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_MULTI_CONTENTS_VIEW_DELEGATE_H_

#include "chrome/browser/ui/views/frame/multi_contents_view_drop_target_controller.h"

class TabStripModel;
class Browser;

namespace content {
class WebContents;
}  // namespace content

class MultiContentsViewDelegate
    : public MultiContentsViewDropTargetController::DropDelegate {
 public:
  ~MultiContentsViewDelegate() override = default;

  virtual void WebContentsFocused(content::WebContents* contents) = 0;
  virtual void ResizeWebContents(double ratio, bool done_resizing) = 0;
  virtual void ReverseWebContents() = 0;
};

// Executes browser and tabstrip dependent behaviors on behalf of a
// `MultiContentsView`, such as handling drag and drop entrypoints, and general
// tabstrip operations.
class MultiContentsViewDelegateImpl : public MultiContentsViewDelegate {
 public:
  explicit MultiContentsViewDelegateImpl(Browser& browser);
  MultiContentsViewDelegateImpl(const MultiContentsViewDelegateImpl&) = delete;
  MultiContentsViewDelegateImpl& operator=(
      const MultiContentsViewDelegateImpl&) = delete;
  ~MultiContentsViewDelegateImpl() override = default;

  // Activates the focused contents.
  void WebContentsFocused(content::WebContents* contents) override;

  // Updates the split sizing ratio.
  // Must already be in a split.
  void ResizeWebContents(double ratio, bool done_resizing) override;

  // Reverses the order of split tabs.
  // Must already be in a split.
  void ReverseWebContents() override;

  // Creates a new tab for the first valid URL in `urls` and creates a split
  // with it and the active tab. If all URLs are blocked, then it will open
  // that.
  void HandleLinkDrop(MultiContentsDropTargetView::DropSide side,
                      const ui::DropTargetEvent& event) override;

  // Detaches a dragged tab from its current tabstrip and inserts it into a
  // split view in this delegate's tab strip.
  void HandleTabDrop(MultiContentsDropTargetView::DropSide side,
                     TabDragDelegate::DragController& drag_controller) override;

 private:
  // TODO(crbug.com/431000266): Use a browser window feature instead.
  const raw_ref<Browser> browser_;
  const raw_ref<TabStripModel> tab_strip_model_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_MULTI_CONTENTS_VIEW_DELEGATE_H_
