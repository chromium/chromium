// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_MULTI_CONTENTS_VIEW_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_MULTI_CONTENTS_VIEW_DELEGATE_H_

#include "chrome/browser/ui/views/frame/multi_contents_drop_target_view.h"

class TabStripModel;

namespace content {
class WebContents;
}  // namespace content

class MultiContentsViewDelegate
    : public MultiContentsDropTargetView::DropDelegate {
 public:
  ~MultiContentsViewDelegate() override = default;

  virtual void WebContentsFocused(content::WebContents* contents) = 0;
  virtual void ResizeWebContents(double ratio) = 0;
  virtual void ReverseWebContents() = 0;
};

class MultiContentsViewDelegateImpl : public MultiContentsViewDelegate {
 public:
  explicit MultiContentsViewDelegateImpl(TabStripModel& tab_strip_model);
  MultiContentsViewDelegateImpl(const MultiContentsViewDelegateImpl&) = delete;
  MultiContentsViewDelegateImpl& operator=(
      const MultiContentsViewDelegateImpl&) = delete;
  ~MultiContentsViewDelegateImpl() override = default;

  void WebContentsFocused(content::WebContents* contents) override;
  void ResizeWebContents(double ratio) override;
  void ReverseWebContents() override;
  void HandleLinkDrop(MultiContentsDropTargetView::DropSide side,
                      const std::vector<GURL>& urls) override;

 private:
  const raw_ref<TabStripModel> tab_strip_model_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_MULTI_CONTENTS_VIEW_DELEGATE_H_
