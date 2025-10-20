// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_RELOAD_BUTTON_WEB_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_RELOAD_BUTTON_WEB_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/toolbar/reload_button.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

class ReloadButtonUI;
class BrowserWindowInterface;

namespace views {
class WebView;
}

// A view that displays the reload button as a WebView.
class ReloadButtonWebView : public views::View {
  METADATA_HEADER(ReloadButtonWebView, views::View)

 public:
  explicit ReloadButtonWebView(BrowserWindowInterface* browser);
  ReloadButtonWebView(const ReloadButtonWebView&) = delete;
  ReloadButtonWebView& operator=(const ReloadButtonWebView&) = delete;
  ~ReloadButtonWebView() override;

  void ChangeMode(ReloadButton::Mode mode, bool force);

 private:
  raw_ptr<ReloadButtonUI> reload_button_ui_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_RELOAD_BUTTON_WEB_VIEW_H_
