// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_SPLIT_TAB_HIGHLIGHT_DELEGATE_H_
#define CHROME_BROWSER_UI_TABS_SPLIT_TAB_HIGHLIGHT_DELEGATE_H_

#include "base/memory/raw_ptr.h"

class BrowserView;

namespace split_tabs {

// Delegate that triggers the scrim to show and hide
class SplitTabHighlightDelegate {
 public:
  SplitTabHighlightDelegate() = default;
  virtual ~SplitTabHighlightDelegate() = default;

  virtual void SetHighlight(bool is_highlighted) = 0;
};

class SplitTabHighlightDelegateImpl
    : public split_tabs::SplitTabHighlightDelegate {
 public:
  explicit SplitTabHighlightDelegateImpl(BrowserView* browser_view);
  ~SplitTabHighlightDelegateImpl() override;

  void SetHighlight(bool is_highlighted) override;

 private:
  raw_ptr<BrowserView> browser_view_ = nullptr;
};

}  // namespace split_tabs

#endif  // CHROME_BROWSER_UI_TABS_SPLIT_TAB_HIGHLIGHT_DELEGATE_H_
