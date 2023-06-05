// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WAFFLE_WAFFLE_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WAFFLE_WAFFLE_DIALOG_VIEW_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

class Browser;

namespace views {
class WebView;
}

// Implements the Waffle dialog as a View. The view contains a WebView
// into which is loaded a WebUI page which renders the actual dialog content.
class WaffleDialogView : public views::View {
 public:
  METADATA_HEADER(WaffleDialogView);
  explicit WaffleDialogView(Browser* browser);

  // Initialize WaffleDialogView's web_view_ element.
  void Initialize();

 private:
  // Shows the dialog widget.
  void ShowNativeView();

  raw_ptr<views::WebView> web_view_ = nullptr;
  const raw_ptr<const Browser> browser_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_WAFFLE_WAFFLE_DIALOG_VIEW_H_
