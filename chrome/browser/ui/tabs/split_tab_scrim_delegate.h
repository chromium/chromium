// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_SPLIT_TAB_SCRIM_DELEGATE_H_
#define CHROME_BROWSER_UI_TABS_SPLIT_TAB_SCRIM_DELEGATE_H_

#include "base/memory/raw_ptr.h"

class BrowserView;

namespace split_tabs {
// Delegate that triggers the scrim to show and hide
class SplitTabScrimDelegate {
 public:
  SplitTabScrimDelegate() = default;
  virtual ~SplitTabScrimDelegate() = default;

  // Shows the scrim over the inactive split tab.
  virtual void ShowScrim() = 0;

  // Hides the scrim.
  virtual void HideScrim() = 0;
};

class SplitTabScrimDelegateImpl : public split_tabs::SplitTabScrimDelegate {
 public:
  explicit SplitTabScrimDelegateImpl(BrowserView* browser_view);
  ~SplitTabScrimDelegateImpl() override;
  void ShowScrim() override;
  void HideScrim() override;

 private:
  void UpdateScrimVisibility(bool show_scrim);
  raw_ptr<BrowserView> browser_view_ = nullptr;
};
}  // namespace split_tabs

#endif  // CHROME_BROWSER_UI_TABS_SPLIT_TAB_SCRIM_DELEGATE_H_
