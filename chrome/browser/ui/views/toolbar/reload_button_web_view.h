// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_RELOAD_BUTTON_WEB_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_RELOAD_BUTTON_WEB_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

class Profile;

namespace views {
class WebView;
}

// A view that displays the reload button as a WebView.
class ReloadButtonWebView : public views::View {
  METADATA_HEADER(ReloadButtonWebView, views::View)

 public:
  explicit ReloadButtonWebView(Profile* profile);
  ReloadButtonWebView(const ReloadButtonWebView&) = delete;
  ReloadButtonWebView& operator=(const ReloadButtonWebView&) = delete;
  ~ReloadButtonWebView() override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_RELOAD_BUTTON_WEB_VIEW_H_
