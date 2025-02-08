// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_MULTI_CONTENTS_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_MULTI_CONTENTS_VIEW_H_

#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

class ContentsWebView;

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

// MultiContentsView shows up to two contents web views side by side, and
// manages their layout relative to each other.
class MultiContentsView : public views::View {
  METADATA_HEADER(MultiContentsView, views::View)

 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kMultiContentsViewElementId);

  explicit MultiContentsView(content::BrowserContext* browser_context);
  MultiContentsView(const MultiContentsView&) = delete;
  MultiContentsView& operator=(const MultiContentsView&) = delete;
  ~MultiContentsView() override;

  ContentsWebView* active_contents_view() { return active_contents_view_; }
  ContentsWebView* inactive_contents_view() { return inactive_contents_view_; }

  // Assigns the given |web_contents| to a ContentsWebView. If |active| it will
  // be assigned to active_contents_view_, else it will be assigned to
  // inactive_contents_view_.
  void SetWebContents(content::WebContents* web_contents, bool active);

  // Sets the index of the active contents view, as relative to the inactive
  // contents view. In LTR, a value of 0 will place the active view on the
  // left.
  void SetActivePosition(int position);

 private:
  raw_ptr<ContentsWebView> active_contents_view_ = nullptr;
  raw_ptr<ContentsWebView> inactive_contents_view_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_MULTI_CONTENTS_VIEW_H_
