// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_SHARE_THIS_TAB_SOURCE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_SHARE_THIS_TAB_SOURCE_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/view.h"

// View displaying a preview, icon and title for the tab being shared, or a
// throbber while the dialog is not yet activated.
class ShareThisTabSourceView : public views::View {
 public:
  ShareThisTabSourceView();
  ShareThisTabSourceView(const ShareThisTabSourceView&) = delete;
  ShareThisTabSourceView& operator=(const ShareThisTabSourceView&) = delete;
  ~ShareThisTabSourceView() override;

  void Activate();

  // views::View:
  gfx::Size CalculatePreferredSize() const override;

 private:
  raw_ptr<views::Throbber> throbber_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_SHARE_THIS_TAB_SOURCE_VIEW_H_
