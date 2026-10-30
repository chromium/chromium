// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PICTURE_IN_PICTURE_DOCUMENT_PIP_CONTENTS_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PICTURE_IN_PICTURE_DOCUMENT_PIP_CONTENTS_VIEW_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/webview/webview.h"

namespace content {
class WebContents;
}  // namespace content

namespace gfx {
class Rect;
}  // namespace gfx

class Profile;
class ScrimView;

// DocumentPipContentsView is the contents view for the standalone Document
// Picture-in-Picture widget. It is a views::WebView that hosts the PiP child
// WebContents and fills the widget's client area.
class DocumentPipContentsView : public views::WebView {
  METADATA_HEADER(DocumentPipContentsView, views::WebView)

 public:
  // `profile` must outlive this view. `child_web_contents` ownership is
  // transferred to this view.
  DocumentPipContentsView(
      Profile* profile,
      std::unique_ptr<content::WebContents> child_web_contents);

  DocumentPipContentsView(const DocumentPipContentsView&) = delete;
  DocumentPipContentsView& operator=(const DocumentPipContentsView&) = delete;

  ~DocumentPipContentsView() override;

  void SetDialogScrimVisible(bool visible);

  ScrimView* dialog_scrim_for_testing() { return dialog_scrim_; }

 protected:
  // views::WebView:
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

 private:
  raw_ptr<ScrimView> dialog_scrim_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PICTURE_IN_PICTURE_DOCUMENT_PIP_CONTENTS_VIEW_H_
